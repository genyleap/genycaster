module;

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

module farcaster.provider.hypersnap.async_gateway;

import farcaster.application.executor;
import farcaster.application.read.async_gateway;
import farcaster.application.read.model;
import farcaster.capability.catalog;
import farcaster.capability.feed;
import farcaster.capability.model;
import farcaster.capability.notification;
import farcaster.capability.pagination;
import farcaster.capability.search;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.feed;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.domain.social;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.async_request;
import farcaster.provider.hypersnap.codec;
import farcaster.provider.hypersnap.config;
import farcaster.provider.hypersnap.continuation;
import farcaster.provider.hypersnap.request;
import farcaster.storage.continuation;
import farcaster.transport.async_http;

namespace farcaster::provider::hypersnap::async_gateway {
namespace {

using AppRequest = application::read::async_gateway::Request;
using Error = foundation::error::Error;
using ErrorCode = foundation::error::ErrorCode;

// A 5-6s overall deadline expires during cold startup and proxy/TLS retry.
// Bound foreground reads to 12s while local-first cache remains interactive.
constexpr std::chrono::milliseconds followingRequestTimeout{12'000};
constexpr std::chrono::milliseconds discoveryRequestTimeout{12'000};
// A ranking query on the public node can stall while other feed reads work.
// Limit its initial attempt so the supported community discovery fallback has
// time to return actual signed casts, rather than tripping the whole circuit.
constexpr std::chrono::milliseconds trendingProbeTimeout{3'500};
constexpr std::string_view communityCursorPrefix{"community:"};
constexpr std::chrono::milliseconds profileFeedRequestTimeout{12'000};
constexpr std::chrono::milliseconds viewerReactionRequestTimeout{7'500};
// The reaction snapshot is filtered locally, so a bounded but broad window must
// cover reactions on casts the viewer engaged with recently. Mirrors the exact
// blocking source (feed.cpp viewerReactionsFor) so async reads cannot silently
// drop older reactions that /reaction/user would still report.
constexpr std::size_t viewerReactionSnapshotLimit{10'000};

[[nodiscard]] transport::async_http::Priority priorityFor(
    const application::read::model::ReadOptions options,
    const transport::async_http::Priority foreground) noexcept
{
    return options.intent == application::read::model::ReadIntent::Prefetch
        ? transport::async_http::Priority::Prefetch
        : foreground;
}

struct Group final {
    std::atomic_bool cancelled{false};
    std::mutex mutex{};
    std::vector<async_request::Operation> operations{};

    void add(async_request::Operation operation)
    {
        if (!operation.isActive()) {
            return;
        }
        if (cancelled.load(std::memory_order_acquire)) {
            operation.cancel();
            return;
        }
        std::scoped_lock lock{mutex};
        if (cancelled.load(std::memory_order_acquire)) {
            operation.cancel();
            return;
        }
        operations.push_back(std::move(operation));
    }

    void cancel() noexcept
    {
        if (cancelled.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        std::vector<async_request::Operation> snapshot{};
        {
            std::scoped_lock lock{mutex};
            snapshot = operations;
        }
        for (const auto& operation : snapshot) {
            operation.cancel();
        }
    }
};

[[nodiscard]] AppRequest applicationRequest(const std::shared_ptr<Group>& group)
{
    return AppRequest{[group] { group->cancel(); }};
}

[[nodiscard]] Error makeError(const ErrorCode code, std::string message)
{
    return Error::make(code, std::move(message));
}

[[nodiscard]] foundation::result::Result<std::size_t> validatedLimit(
    const std::size_t limit,
    const std::size_t maximum = 100U)
{
    if (limit == 0U || limit > maximum) {
        return std::unexpected(makeError(
            ErrorCode::InvalidArgument,
            "Hypersnap page limit is outside the supported range."));
    }
    return limit;
}

[[nodiscard]] bool supportsAliasFallback(const ErrorCode code) noexcept
{
    return code == ErrorCode::NotFound || code == ErrorCode::Unsupported;
}

[[nodiscard]] bool isQuote(const domain::cast::Cast& cast)
{
    return std::ranges::any_of(cast.embeds, [](const auto& embed) {
        return embed.kind == domain::cast::EmbedKind::CastReference;
    });
}

void filterUserCastPage(capability::feed::FeedPage& page, const bool wantQuotes)
{
    std::unordered_set<std::string> retained{};
    std::vector<domain::cast::Cast> casts{};
    casts.reserve(page.casts.size());
    for (auto& cast : page.casts) {
        if (!cast.parent.has_value() && isQuote(cast) == wantQuotes) {
            retained.insert(cast.id.value);
            casts.push_back(std::move(cast));
        }
    }
    page.casts = std::move(casts);
    std::erase_if(page.entries, [&](const auto& entry) {
        return !retained.contains(entry.castId.value);
    });
}

void filterReplies(capability::feed::FeedPage& page)
{
    std::unordered_set<std::string> retained{};
    std::vector<domain::cast::Cast> casts{};
    casts.reserve(page.casts.size());
    for (auto& cast : page.casts) {
        if (cast.parent.has_value()) {
            retained.insert(cast.id.value);
            casts.push_back(std::move(cast));
        }
    }
    page.casts = std::move(casts);
    std::erase_if(page.entries, [&](const auto& entry) {
        return !retained.contains(entry.castId.value);
    });
}

[[nodiscard]] capability::feed::FeedPage pageFromPayload(codec::FeedPayload payload)
{
    capability::feed::FeedPage page{};
    page.casts = std::move(payload.casts);
    page.profiles = std::move(payload.profiles);
    page.entries.reserve(page.casts.size());
    for (const auto& cast : page.casts) {
        page.entries.push_back(domain::feed::FeedEntry{
            .castId = cast.id,
            .sortTime = cast.createdAt,
        });
    }
    return page;
}

constexpr std::string_view pfpUserDataType{"1"};
constexpr std::string_view displayUserDataType{"2"};
constexpr std::string_view bioUserDataType{"3"};
constexpr std::string_view urlUserDataType{"5"};
constexpr std::string_view usernameUserDataType{"6"};
constexpr std::string_view locationUserDataType{"7"};
constexpr std::string_view bannerUserDataType{"10"};

[[nodiscard]] bool isType(
    const codec::UserDataEntry& entry,
    const std::string_view numericType,
    const std::string_view symbolicType = {})
{
    return entry.type == numericType || (!symbolicType.empty() && entry.type == symbolicType);
}

[[nodiscard]] std::optional<std::string> findValue(
    const std::vector<codec::UserDataEntry>& entries,
    const std::string_view numericType,
    const std::string_view symbolicType = {})
{
    for (const auto& entry : entries) {
        if (isType(entry, numericType, symbolicType) && !entry.value.empty()) {
            return entry.value;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool remoteBanner(const std::string_view value) noexcept
{
    return value.starts_with("https://") || value.starts_with("http://") ||
        value.starts_with("ipfs://");
}

[[nodiscard]] std::optional<std::string> findBanner(
    const std::vector<codec::UserDataEntry>& entries)
{
    for (const auto& entry : entries) {
        const bool bannerType = isType(entry, bannerUserDataType, "USER_DATA_TYPE_BANNER");
        if (bannerType && remoteBanner(entry.value)) {
            return entry.value;
        }
    }
    return std::nullopt;
}

void supplementProfile(
    domain::profile::Profile& profile,
    const std::vector<codec::UserDataEntry>& entries)
{
    if (!profile.avatarUrl) {
        profile.avatarUrl = findValue(entries, pfpUserDataType, "USER_DATA_TYPE_PFP");
    }
    if (!profile.displayName) {
        if (auto value = findValue(entries, displayUserDataType, "USER_DATA_TYPE_DISPLAY")) {
            profile.displayName = domain::identity::DisplayName{std::move(*value)};
        }
    }
    if (profile.bio.empty()) {
        if (auto value = findValue(entries, bioUserDataType, "USER_DATA_TYPE_BIO")) {
            profile.bio = std::move(*value);
        }
    }
    if (!profile.profileUrl) {
        profile.profileUrl = findValue(entries, urlUserDataType, "USER_DATA_TYPE_URL");
    }
    if (!profile.username || profile.username->value.starts_with("fid:")) {
        auto username = findValue(entries, usernameUserDataType, "USER_DATA_TYPE_USERNAME");
        if (!username) {
            username = findValue(entries, usernameUserDataType, "USER_DATA_TYPE_FNAME");
        }
        if (username && !username->empty()) {
            profile.username = domain::identity::Username{std::move(*username)};
        }
    }
    if (!profile.location) {
        profile.location = findValue(entries, locationUserDataType, "USER_DATA_TYPE_LOCATION");
    }
    if (!profile.bannerUrl) {
        profile.bannerUrl = findBanner(entries);
    }
}

[[nodiscard]] std::string joinedHashes(const std::vector<codec::ReactionTarget>& targets)
{
    std::string hashes{};
    for (const auto& target : targets) {
        if (!hashes.empty()) {
            hashes.push_back(',');
        }
        hashes.append(target.castId.value);
    }
    return hashes;
}

[[nodiscard]] std::string joinedCastHashes(const std::vector<domain::cast::Cast>& casts)
{
    std::string hashes{};
    for (const auto& cast : casts) {
        if (cast.id.value.empty()) {
            continue;
        }
        if (!hashes.empty()) {
            hashes.push_back(',');
        }
        hashes.append(cast.id.value);
    }
    return hashes;
}

void mergeHydratedReplyMetrics(
    capability::feed::FeedPage& page, const codec::FeedPayload& hydrated)
{
    std::unordered_map<std::string, const domain::cast::Cast*> hydratedById{};
    hydratedById.reserve(hydrated.casts.size());
    for (const auto& cast : hydrated.casts) {
        hydratedById.insert_or_assign(cast.id.value, &cast);
    }
    for (auto& cast : page.casts) {
        const auto found = hydratedById.find(cast.id.value);
        if (found == hydratedById.end() || found->second == nullptr) {
            continue;
        }
        const auto& source = *found->second;
        cast.engagement.likes = std::max(cast.engagement.likes, source.engagement.likes);
        cast.engagement.recasts = std::max(cast.engagement.recasts, source.engagement.recasts);
        cast.engagement.replies = std::max(cast.engagement.replies, source.engagement.replies);
    }
}

[[nodiscard]] std::string batchFidBody(const domain::identity::Fid fid)
{
    return "{\"fids\":[" + std::to_string(fid.value) + "]}";
}

constexpr std::uint64_t recentWindowBlocks{500U};
constexpr std::uint64_t recentEventShift{14U};
constexpr std::string_view recentCursorPrefix{"recent:v1:"};

struct RecentCursor final {
    std::uint64_t firstEnd{};
    std::uint64_t secondEnd{};
    std::size_t offset{};
};

[[nodiscard]] std::string encodeRecentCursor(const RecentCursor cursor)
{
    return std::string{recentCursorPrefix} + std::to_string(cursor.firstEnd)
        + ":" + std::to_string(cursor.secondEnd) + ":"
        + std::to_string(cursor.offset);
}

[[nodiscard]] foundation::result::Result<RecentCursor> decodeRecentCursor(
    const std::string_view serialized)
{
    if (!serialized.starts_with(recentCursorPrefix)) {
        return std::unexpected(makeError(ErrorCode::InvalidArgument, "Invalid Recent cursor version."));
    }
    auto tail = serialized.substr(recentCursorPrefix.size());
    std::uint64_t fields[3]{};
    for (std::size_t i = 0; i < 3U; ++i) {
        const auto colon = tail.find(':');
        const auto part = i == 2U ? tail : tail.substr(0, colon);
        if (part.empty() || (i != 2U && colon == std::string_view::npos)
            || (i == 2U && colon != std::string_view::npos)) {
            return std::unexpected(makeError(ErrorCode::InvalidArgument, "Malformed Recent cursor."));
        }
        const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), fields[i]);
        if (error != std::errc{} || end != part.data() + part.size()) {
            return std::unexpected(makeError(ErrorCode::InvalidArgument, "Invalid Recent cursor value."));
        }
        if (i < 2U) { tail.remove_prefix(colon + 1U); }
    }
    if (fields[0] == 0U || fields[1] == 0U || fields[0] > 1'000'000'000U
        || fields[1] > 1'000'000'000U || fields[2] > 4000U) {
        return std::unexpected(makeError(ErrorCode::InvalidArgument, "Recent cursor out of range."));
    }
    return RecentCursor{fields[0], fields[1], static_cast<std::size_t>(fields[2])};
}

[[nodiscard]] std::vector<std::pair<std::string, std::string>> recentEventQuery(
    const std::uint64_t endBlock, const std::uint64_t shard)
{
    const auto start = endBlock > recentWindowBlocks
        ? endBlock - recentWindowBlocks : 0U;
    return {
        {"from_event_id", std::to_string(start << recentEventShift)},
        {"shard_index", std::to_string(shard)}
    };
}

struct FeedSpec final {
    capability::model::CapabilityId capabilityId{};
    std::string path{};
    std::vector<std::pair<std::string, std::string>> query{};
    std::optional<std::string> fallbackPath{};
    std::vector<std::pair<std::string, std::string>> fallbackQuery{};
    bool communityDiscoveryFallback{false};
    std::chrono::milliseconds timeout{profileFeedRequestTimeout};
    enum class Filter { None, Posts, Quotes, Replies } filter{Filter::None};
};

}

AsyncGateway::AsyncGateway(
    transport::async_http::IAsyncHttpTransport& transport,
    execution::weighted_thread_pool::WeightedThreadPoolExecutor& decodeExecutor,
    application::executor::IExecutor& continuationExecutor,
    storage::continuation::ContinuationStore& continuations,
    config::Config config)
    : m_client(transport, decodeExecutor, config),
      m_continuationExecutor(continuationExecutor),
      m_continuations(continuations),
      m_config(std::move(config))
{
}

foundation::result::Result<AppRequest> AsyncGateway::resolveIdentity(
    const std::string_view username,
    application::read::async_gateway::IdentityCompletion completion,
    const application::read::model::ReadOptions)
{
    if (username.empty() || !completion) {
        return std::unexpected(makeError(ErrorCode::InvalidArgument, "Username and completion are required."));
    }
    auto group = std::make_shared<Group>();
    auto operation = m_client.getJson(
        "/v2/farcaster/user/by-username",
        {{"username", std::string{username}}},
        [group, completion = std::move(completion)](foundation::result::Result<std::string> body) mutable {
            if (group->cancelled.load(std::memory_order_acquire)) {
                return;
            }
            if (!body) {
                completion(std::unexpected(std::move(body.error())));
                return;
            }
            auto profile = codec::decodeUser(*body);
            if (!profile) {
                completion(std::unexpected(std::move(profile.error())));
                return;
            }
            completion(profile->fid);
        },
        {},
        transport::async_http::Priority::Interactive);
    if (!operation) {
        return std::unexpected(std::move(operation.error()));
    }
    group->add(std::move(*operation));
    return applicationRequest(group);
}

foundation::result::Result<AppRequest> AsyncGateway::profile(
    const domain::identity::Fid fid,
    application::read::async_gateway::ProfileCompletion completion,
    const application::read::model::ReadOptions)
{
    if (fid.value == 0U || !completion) {
        return std::unexpected(makeError(ErrorCode::InvalidArgument, "Profile FID and completion are required."));
    }
    struct State final {
        std::mutex mutex{};
        std::optional<foundation::result::Result<domain::profile::Profile>> profile{};
        std::optional<foundation::result::Result<std::vector<codec::UserDataEntry>>> userData{};
        application::read::async_gateway::ProfileCompletion completion{};
        std::shared_ptr<Group> group{};
    };
    auto group = std::make_shared<Group>();
    auto state = std::make_shared<State>();
    state->completion = std::move(completion);
    state->group = group;

    const auto finish = [state]() {
        std::optional<foundation::result::Result<domain::profile::Profile>> profile{};
        std::optional<foundation::result::Result<std::vector<codec::UserDataEntry>>> userData{};
        application::read::async_gateway::ProfileCompletion completion{};
        {
            std::scoped_lock lock{state->mutex};
            if (!state->profile || !state->userData || !state->completion) {
                return;
            }
            profile = std::move(state->profile);
            userData = std::move(state->userData);
            completion = std::move(state->completion);
        }
        if (state->group->cancelled.load(std::memory_order_acquire)) {
            return;
        }
        auto profileResult = std::move(*profile);
        auto userDataResult = std::move(*userData);
        if (!profileResult) {
            completion(std::unexpected(std::move(profileResult.error())));
            return;
        }
        auto result = std::move(*profileResult);
        if (userDataResult) {
            supplementProfile(result, *userDataResult);
        }
        completion(std::move(result));
    };

    auto base = m_client.getJson(
        "/v2/farcaster/user",
        {{"fid", std::to_string(fid.value)}},
        [state, finish](foundation::result::Result<std::string> body) mutable {
            foundation::result::Result<domain::profile::Profile> result = body
                ? codec::decodeUser(*body)
                : foundation::result::Result<domain::profile::Profile>{std::unexpected(std::move(body.error()))};
            {
                std::scoped_lock lock{state->mutex};
                state->profile = std::move(result);
            }
            finish();
        },
        {},
        transport::async_http::Priority::Interactive);
    if (!base) {
        return std::unexpected(std::move(base.error()));
    }
    group->add(std::move(*base));

    auto supplement = m_client.getJson(
        "/v1/userDataByFid",
        {{"fid", std::to_string(fid.value)}},
        [state, finish](foundation::result::Result<std::string> body) mutable {
            foundation::result::Result<std::vector<codec::UserDataEntry>> result = body
                ? codec::decodeUserDataEntries(*body)
                : foundation::result::Result<std::vector<codec::UserDataEntry>>{
                      std::unexpected(std::move(body.error()))};
            {
                std::scoped_lock lock{state->mutex};
                state->userData = std::move(result);
            }
            finish();
        },
        request::GetOptions{.timeout = std::chrono::milliseconds{2'500}},
        transport::async_http::Priority::Visible);
    if (!supplement) {
        // UserData is enrichment only. Preserve the base profile fast path even
        // if admission pressure rejects this optional request.
        {
            std::scoped_lock lock{state->mutex};
            state->userData = foundation::result::Result<std::vector<codec::UserDataEntry>>{
                std::unexpected(std::move(supplement.error()))};
        }
        finish();
    } else {
        group->add(std::move(*supplement));
    }
    return applicationRequest(group);
}

foundation::result::Result<AppRequest> AsyncGateway::cast(
    domain::cast::CastId castId,
    application::read::async_gateway::CastCompletion completion,
    const application::read::model::ReadOptions options)
{
    if (castId.value.empty() || !completion) {
        return std::unexpected(makeError(ErrorCode::InvalidArgument, "Cast id and completion are required."));
    }
    auto group = std::make_shared<Group>();
    auto completionState = std::make_shared<application::read::async_gateway::CastCompletion>(std::move(completion));
    auto fallback = [this, group, castId, completionState, options]() mutable {
        if (group->cancelled.load(std::memory_order_acquire) || !*completionState) {
            return;
        }
        auto operation = m_client.getJson(
            "/v2/farcaster/cast",
            {{"identifier", castId.value}, {"type", "hash"}},
            [group, completionState](foundation::result::Result<std::string> body) mutable {
                if (group->cancelled.load(std::memory_order_acquire) || !*completionState) {
                    return;
                }
                auto completion = std::move(*completionState);
                if (!body) {
                    completion(std::unexpected(std::move(body.error())));
                    return;
                }
                completion(codec::decodeCast(*body));
            },
            request::GetOptions{.timeout = std::chrono::milliseconds{8'000}},
            priorityFor(options, transport::async_http::Priority::Interactive));
        if (!operation) {
            auto completion = std::move(*completionState);
            if (completion) {
                completion(std::unexpected(std::move(operation.error())));
            }
            return;
        }
        group->add(std::move(*operation));
    };

    auto conversation = m_client.getJson(
        "/v2/farcaster/cast/conversation",
        {{"identifier", castId.value}, {"type", "hash"}, {"reply_depth", "4"}},
        [group, castId, completionState, fallback](foundation::result::Result<std::string> body) mutable {
            if (group->cancelled.load(std::memory_order_acquire) || !*completionState) {
                return;
            }
            if (body) {
                auto decoded = codec::decodeConversationPayload(*body);
                if (decoded && decoded->cast.id.value == castId.value) {
                    auto completion = std::move(*completionState);
                    completion(std::move(decoded->cast));
                    return;
                }
            }
            fallback();
        },
        request::GetOptions{.timeout = std::chrono::milliseconds{8'000}},
        priorityFor(options, transport::async_http::Priority::Interactive));
    if (!conversation) {
        fallback();
    } else {
        group->add(std::move(*conversation));
    }
    return applicationRequest(group);
}

foundation::result::Result<AppRequest> AsyncGateway::recentFeed(
    application::read::model::FeedQuery query,
    application::read::async_gateway::FeedCompletion completion,
    const application::read::model::ReadOptions options)
{
    auto group = std::make_shared<Group>();
    const auto capabilityId = capability::catalog::feedRecentRead();
    const auto requestPriority = priorityFor(options, transport::async_http::Priority::Visible);
    const capability::pagination::PageRequest pageRequest{
        .limit = query.limit, .continuation = query.continuation};
    auto resultCallback = std::make_shared<
        application::read::async_gateway::FeedCompletion>(std::move(completion));
    auto deliver = [group, resultCallback](
        foundation::result::Result<capability::feed::FeedPage> page) mutable {
        if (group->cancelled.load(std::memory_order_acquire) || !*resultCallback) {
            return;
        }
        auto callback = std::move(*resultCallback);
        callback(std::move(page));
    };

    auto start = m_continuationExecutor.post(
        [this, group, capabilityId, pageRequest, requestPriority, deliver]() mutable {
            if (group->cancelled.load(std::memory_order_acquire)) { return; }
            continuation::Store store{m_continuations, m_config.provider.id};
            auto raw = store.resolve(capabilityId, pageRequest);
            if (!raw) {
                deliver(std::unexpected(std::move(raw.error())));
                return;
            }

            auto loadWindow = [this, group, capabilityId, pageRequest,
                               requestPriority, deliver](RecentCursor cursor) mutable {
                auto first = m_client.getJson(
                    "/v1/events", recentEventQuery(cursor.firstEnd, 1U),
                    [this, group, capabilityId, pageRequest, requestPriority, deliver, cursor](
                        foundation::result::Result<std::string> firstBody) mutable {
                        if (group->cancelled.load(std::memory_order_acquire)) { return; }
                        if (!firstBody) {
                            deliver(std::unexpected(std::move(firstBody.error())));
                            return;
                        }
                        auto firstEvents = codec::decodeRecentCastEvents(
                            *firstBody,
                            cursor.firstEnd > recentWindowBlocks
                                ? cursor.firstEnd - recentWindowBlocks : 0U,
                            cursor.firstEnd);
                        if (!firstEvents) {
                            deliver(std::unexpected(std::move(firstEvents.error())));
                            return;
                        }
                        auto second = m_client.getJson(
                            "/v1/events", recentEventQuery(cursor.secondEnd, 2U),
                            [this, group, capabilityId, pageRequest, requestPriority,
                             deliver, cursor, firstEvents = std::move(*firstEvents)](
                                foundation::result::Result<std::string> secondBody) mutable {
                                if (group->cancelled.load(std::memory_order_acquire)) { return; }
                                if (!secondBody) {
                                    deliver(std::unexpected(std::move(secondBody.error())));
                                    return;
                                }
                                auto secondEvents = codec::decodeRecentCastEvents(
                                    *secondBody,
                                    cursor.secondEnd > recentWindowBlocks
                                        ? cursor.secondEnd - recentWindowBlocks : 0U,
                                    cursor.secondEnd);
                                if (!secondEvents) {
                                    deliver(std::unexpected(std::move(secondEvents.error())));
                                    return;
                                }
                                firstEvents.insert(firstEvents.end(),
                                    std::make_move_iterator(secondEvents->begin()),
                                    std::make_move_iterator(secondEvents->end()));
                                std::ranges::sort(firstEvents, [](const auto& a, const auto& b) {
                                    if (a.protocolTimestamp != b.protocolTimestamp) {
                                        return a.protocolTimestamp > b.protocolTimestamp;
                                    }
                                    return a.hash < b.hash;
                                });
                                std::unordered_set<std::string> seen{};
                                std::vector<std::string> orderedHashes{};
                                orderedHashes.reserve(firstEvents.size());
                                for (const auto& event : firstEvents) {
                                    if (seen.insert(event.hash).second) {
                                        orderedHashes.push_back(event.hash);
                                    }
                                }
                                const auto begin = std::min(cursor.offset, orderedHashes.size());
                                const auto end = std::min(
                                    orderedHashes.size(), begin + pageRequest.limit);
                                std::vector<std::string> selected(
                                    orderedHashes.begin() + static_cast<std::ptrdiff_t>(begin),
                                    orderedHashes.begin() + static_cast<std::ptrdiff_t>(end));
                                std::optional<std::string> nextCursor{};
                                if (end < orderedHashes.size()) {
                                    nextCursor = encodeRecentCursor(
                                        RecentCursor{cursor.firstEnd, cursor.secondEnd, end});
                                } else if (cursor.firstEnd > recentWindowBlocks
                                           && cursor.secondEnd > recentWindowBlocks) {
                                    nextCursor = encodeRecentCursor(RecentCursor{
                                        cursor.firstEnd - recentWindowBlocks,
                                        cursor.secondEnd - recentWindowBlocks, 0U});
                                }
                                auto completePage = [this, group, capabilityId, deliver,
                                                     selected, nextCursor = std::move(nextCursor)](
                                    foundation::result::Result<codec::FeedPayload> hydrated) mutable {
                                    if (group->cancelled.load(std::memory_order_acquire)) { return; }
                                    if (!hydrated) {
                                        deliver(std::unexpected(std::move(hydrated.error())));
                                        return;
                                    }
                                    auto page = pageFromPayload(std::move(*hydrated));
                                    std::unordered_map<std::string, domain::cast::Cast> byHash{};
                                    byHash.reserve(page.casts.size());
                                    for (auto& cast : page.casts) {
                                        byHash.insert_or_assign(cast.id.value, std::move(cast));
                                    }
                                    page.casts.clear();
                                    for (const auto& hash : selected) {
                                        auto it = byHash.find(hash);
                                        if (it != byHash.end()) {
                                            page.casts.push_back(std::move(it->second));
                                        }
                                    }
                                    // The hydrated response order is not chronological.
                                    // Rebuild the canonical entries in the same order as casts.
                                    page.entries.clear();
                                    for (const auto& cast : page.casts) {
                                        page.entries.push_back(domain::feed::FeedEntry{
                                            .castId = cast.id, .sortTime = cast.createdAt});
                                    }
                                    auto bound = m_continuationExecutor.post(
                                        [this, group, capabilityId, deliver,
                                         nextCursor = std::move(nextCursor),
                                         page = std::move(page)]() mutable {
                                            if (group->cancelled.load(std::memory_order_acquire)) {
                                                return;
                                            }
                                            continuation::Store completedStore{
                                                m_continuations, m_config.provider.id};
                                            auto next = completedStore.bind(
                                                capabilityId, std::move(nextCursor));
                                            if (!next) {
                                                deliver(std::unexpected(std::move(next.error())));
                                                return;
                                            }
                                            page.next = std::move(*next);
                                            deliver(std::move(page));
                                        });
                                    if (!bound) {
                                        deliver(std::unexpected(std::move(bound.error())));
                                    }
                                };
                                if (selected.empty()) {
                                    completePage(codec::FeedPayload{});
                                    return;
                                }
                                std::string hashes{};
                                for (const auto& hash : selected) {
                                    if (!hashes.empty()) { hashes.push_back(','); }
                                    hashes.append(hash);
                                }
                                auto hydrate = m_client.getJson(
                                    "/v2/farcaster/cast/bulk", {{"hashes", hashes}},
                                    [group, completePage](
                                        foundation::result::Result<std::string> body) mutable {
                                        if (group->cancelled.load(std::memory_order_acquire)) { return; }
                                        if (!body) {
                                            completePage(std::unexpected(std::move(body.error())));
                                            return;
                                        }
                                        completePage(codec::decodeFeed(*body));
                                    },
                                    request::GetOptions{.timeout = profileFeedRequestTimeout},
                                    requestPriority);
                                if (!hydrate) {
                                    deliver(std::unexpected(std::move(hydrate.error())));
                                    return;
                                }
                                group->add(std::move(*hydrate));
                            },
                            request::GetOptions{.timeout = discoveryRequestTimeout},
                            requestPriority);
                        if (!second) {
                            deliver(std::unexpected(std::move(second.error())));
                            return;
                        }
                        group->add(std::move(*second));
                    },
                    request::GetOptions{.timeout = discoveryRequestTimeout},
                    requestPriority);
                if (!first) {
                    deliver(std::unexpected(std::move(first.error())));
                    return;
                }
                group->add(std::move(*first));
            };

            if (*raw) {
                auto cursor = decodeRecentCursor(**raw);
                if (!cursor) {
                    deliver(std::unexpected(std::move(cursor.error())));
                    return;
                }
                loadWindow(*cursor);
                return;
            }
            auto info = m_client.getJson(
                "/v1/info", {},
                [group, loadWindow, deliver](
                    foundation::result::Result<std::string> body) mutable {
                    if (group->cancelled.load(std::memory_order_acquire)) { return; }
                    if (!body) {
                        deliver(std::unexpected(std::move(body.error())));
                        return;
                    }
                    auto shards = codec::decodeRecentShardInfo(*body);
                    if (!shards) {
                        deliver(std::unexpected(std::move(shards.error())));
                        return;
                    }
                    loadWindow(RecentCursor{
                        (*shards)[0].height + 1U,
                        (*shards)[1].height + 1U, 0U});
                },
                request::GetOptions{.timeout = discoveryRequestTimeout}, requestPriority);
            if (!info) {
                deliver(std::unexpected(std::move(info.error())));
                return;
            }
            group->add(std::move(*info));
        });
    if (!start) {
        return std::unexpected(std::move(start.error()));
    }
    return applicationRequest(group);
}

foundation::result::Result<AppRequest> AsyncGateway::feed(
    application::read::model::FeedQuery query,
    application::read::async_gateway::FeedCompletion completion,
    const application::read::model::ReadOptions options)
{
    if (!completion) {
        return std::unexpected(makeError(ErrorCode::InvalidArgument, "Feed completion is required."));
    }
    auto limit = validatedLimit(query.limit);
    if (!limit) {
        return std::unexpected(std::move(limit.error()));
    }
    if (query.kind == application::read::model::FeedQuery::Kind::Recent) {
        return recentFeed(std::move(query), std::move(completion), options);
    }

    FeedSpec spec{};
    switch (query.kind) {
    case application::read::model::FeedQuery::Kind::Following:
        if (!query.fid || query.fid->value == 0U) {
            return std::unexpected(makeError(ErrorCode::InvalidArgument, "Following feed requires a FID."));
        }
        spec.capabilityId = capability::catalog::feedFollowingRead();
        spec.path = "/v2/farcaster/feed/following";
        spec.query = {{"fid", std::to_string(query.fid->value)}};
        spec.fallbackPath = "/v2/farcaster/feed";
        spec.fallbackQuery = {{"feed_type", "following"}, {"fid", std::to_string(query.fid->value)}};
        spec.timeout = followingRequestTimeout;
        break;
    case application::read::model::FeedQuery::Kind::Recent:
        spec.capabilityId = capability::catalog::feedRecentRead();
        break;
    case application::read::model::FeedQuery::Kind::Trending:
        spec.capabilityId = capability::catalog::feedTrendingRead();
        spec.path = "/v2/farcaster/feed/trending";
        spec.fallbackPath = "/v2/farcaster/feed";
        spec.fallbackQuery = {{"feed_type", "trending"}};
        spec.timeout = m_config.provider.id.value == "hypersnap-v2"
            ? trendingProbeTimeout : discoveryRequestTimeout;
        spec.communityDiscoveryFallback = m_config.provider.id.value == "hypersnap-v2";
        break;
    case application::read::model::FeedQuery::Kind::Channel:
        if (!query.channel || query.channel->value.empty()) {
            return std::unexpected(makeError(ErrorCode::InvalidArgument, "Channel feed requires a channel id."));
        }
        spec.capabilityId = capability::catalog::feedChannelRead();
        spec.path = "/v2/farcaster/feed/channels";
        spec.query = {{"channel_ids", query.channel->value}};
        spec.timeout = discoveryRequestTimeout;
        break;
    case application::read::model::FeedQuery::Kind::UserPosts:
    case application::read::model::FeedQuery::Kind::UserQuotes:
    case application::read::model::FeedQuery::Kind::UserReplies:
    case application::read::model::FeedQuery::Kind::UserLikes:
    case application::read::model::FeedQuery::Kind::UserPopular:
        if (!query.fid || query.fid->value == 0U) {
            return std::unexpected(makeError(ErrorCode::InvalidArgument, "User feed requires a FID."));
        }
        if (query.kind == application::read::model::FeedQuery::Kind::UserLikes) {
            // Implemented below after provider cursor resolution because it uses
            // reactions + one cast/bulk hydration request.
            break;
        }
        if (query.kind == application::read::model::FeedQuery::Kind::UserPopular) {
            spec.capabilityId = capability::catalog::feedUserPopularRead();
            spec.path = "/v2/farcaster/feed/user/popular";
        } else if (query.kind == application::read::model::FeedQuery::Kind::UserReplies) {
            spec.capabilityId = capability::catalog::feedUserRepliesRead();
            spec.path = "/v2/farcaster/feed/user/replies_and_recasts";
            spec.filter = FeedSpec::Filter::Replies;
        } else {
            spec.capabilityId = query.kind == application::read::model::FeedQuery::Kind::UserQuotes
                ? capability::catalog::feedUserQuotesRead()
                : capability::catalog::feedUserPostsRead();
            spec.path = "/v2/farcaster/feed/user/casts";
            spec.filter = query.kind == application::read::model::FeedQuery::Kind::UserQuotes
                ? FeedSpec::Filter::Quotes : FeedSpec::Filter::Posts;
        }
        spec.query = {{"fid", std::to_string(query.fid->value)}};
        spec.timeout = profileFeedRequestTimeout;
        break;
    }

    const auto requestPriority = priorityFor(
        options, transport::async_http::Priority::Visible);
    auto group = std::make_shared<Group>();
    const capability::pagination::PageRequest pageRequest{
        .limit = query.limit,
        .continuation = query.continuation,
    };
    const auto capabilityId = query.kind == application::read::model::FeedQuery::Kind::UserLikes
        ? capability::catalog::feedUserLikesRead() : spec.capabilityId;

    auto scheduled = m_continuationExecutor.post(
        [this, group, query = std::move(query), spec = std::move(spec), capabilityId,
         pageRequest, requestPriority, completion = std::move(completion)]() mutable {
            if (group->cancelled.load(std::memory_order_acquire)) {
                return;
            }
            continuation::Store continuations{m_continuations, m_config.provider.id};
            auto cursor = continuations.resolve(capabilityId, pageRequest);
            if (!cursor) {
                completion(std::unexpected(std::move(cursor.error())));
                return;
            }

            if (query.kind == application::read::model::FeedQuery::Kind::UserLikes) {
                std::vector<std::pair<std::string, std::string>> reactionQuery{
                    {"fid", std::to_string(query.fid->value)},
                    {"type", "likes"},
                    {"limit", std::to_string(query.limit)},
                };
                if (*cursor) {
                    reactionQuery.emplace_back("cursor", **cursor);
                }
                auto reactionsOperation = m_client.getJson(
                    "/v2/farcaster/reaction/user",
                    reactionQuery,
                    [this, group, capabilityId, requestPriority, completion = std::move(completion)](
                        foundation::result::Result<std::string> body) mutable {
                        if (group->cancelled.load(std::memory_order_acquire)) {
                            return;
                        }
                        if (!body) {
                            completion(std::unexpected(std::move(body.error())));
                            return;
                        }
                        auto reactions = codec::decodeReactionTargets(*body);
                        if (!reactions) {
                            completion(std::unexpected(std::move(reactions.error())));
                            return;
                        }
                        auto targets = std::make_shared<std::vector<codec::ReactionTarget>>(
                            std::move(reactions->targets));
                        auto providerCursor = std::make_shared<std::optional<std::string>>(
                            std::move(reactions->providerCursor));

                        auto finishEmptyOrHydrated =
                            [this, group, capabilityId, targets, providerCursor,
                             completion = std::move(completion)](
                                std::optional<codec::FeedPayload> payload,
                                std::optional<Error> error) mutable {
                                if (group->cancelled.load(std::memory_order_acquire)) {
                                    return;
                                }
                                auto status = m_continuationExecutor.post(
                                    [this, group, capabilityId, targets, providerCursor,
                                     payload = std::move(payload), error = std::move(error),
                                     completion = std::move(completion)]() mutable {
                                        if (group->cancelled.load(std::memory_order_acquire)) {
                                            return;
                                        }
                                        if (error) {
                                            completion(std::unexpected(std::move(*error)));
                                            return;
                                        }
                                        capability::feed::FeedPage page{};
                                        if (payload) {
                                            page.profiles = std::move(payload->profiles);
                                            std::unordered_map<std::string, domain::cast::Cast> byId{};
                                            byId.reserve(payload->casts.size());
                                            for (auto& cast : payload->casts) {
                                                byId.emplace(cast.id.value, std::move(cast));
                                            }
                                            page.casts.reserve(targets->size());
                                            page.entries.reserve(targets->size());
                                            for (const auto& target : *targets) {
                                                auto iterator = byId.find(target.castId.value);
                                                if (iterator == byId.end()) {
                                                    continue;
                                                }
                                                page.entries.push_back(domain::feed::FeedEntry{
                                                    .castId = iterator->second.id,
                                                    .sortTime = target.reactedAt,
                                                });
                                                page.casts.push_back(std::move(iterator->second));
                                            }
                                        }
                                        continuation::Store continuations{
                                            m_continuations, m_config.provider.id};
                                        auto next = continuations.bind(
                                            capabilityId, std::move(*providerCursor));
                                        if (!next) {
                                            completion(std::unexpected(std::move(next.error())));
                                            return;
                                        }
                                        page.next = std::move(*next);
                                        completion(std::move(page));
                                    });
                                if (!status) {
                                    completion(std::unexpected(std::move(status.error())));
                                }
                            };

                        if (targets->empty()) {
                            finishEmptyOrHydrated(codec::FeedPayload{}, std::nullopt);
                            return;
                        }
                        auto bulk = m_client.getJson(
                            "/v2/farcaster/cast/bulk",
                            {{"hashes", joinedHashes(*targets)}},
                            [finishEmptyOrHydrated](foundation::result::Result<std::string> bulkBody) mutable {
                                if (!bulkBody) {
                                    finishEmptyOrHydrated(std::nullopt, std::move(bulkBody.error()));
                                    return;
                                }
                                auto payload = codec::decodeFeed(*bulkBody);
                                if (!payload) {
                                    finishEmptyOrHydrated(std::nullopt, std::move(payload.error()));
                                    return;
                                }
                                finishEmptyOrHydrated(std::move(*payload), std::nullopt);
                            },
                            request::GetOptions{.timeout = profileFeedRequestTimeout},
                            requestPriority);
                        if (!bulk) {
                            finishEmptyOrHydrated(std::nullopt, std::move(bulk.error()));
                            return;
                        }
                        group->add(std::move(*bulk));
                    },
                    request::GetOptions{.timeout = profileFeedRequestTimeout},
                    requestPriority);
                if (!reactionsOperation) {
                    completion(std::unexpected(std::move(reactionsOperation.error())));
                    return;
                }
                group->add(std::move(*reactionsOperation));
                return;
            }

            // Build the feed request inline rather than retaining recursive
            // closure state. The preferred endpoint is the common path; alias
            // fallback is started only for NotFound/Unsupported.
            auto resolvedCursor = *cursor;
            const bool communityContinuation = spec.communityDiscoveryFallback && resolvedCursor
                && std::string_view{*resolvedCursor}.starts_with(communityCursorPrefix);
            if (communityContinuation) {
                resolvedCursor = resolvedCursor->substr(communityCursorPrefix.size());
            }
            auto preferredQuery = communityContinuation
                ? std::vector<std::pair<std::string, std::string>>{{"fid", "3"}}
                : spec.query;
            preferredQuery.emplace_back("limit", std::to_string(pageRequest.limit));
            if (resolvedCursor) {
                preferredQuery.emplace_back("cursor", *resolvedCursor);
            }
            auto preferred = m_client.getJson(
                communityContinuation ? "/v2/farcaster/feed/following" : spec.path,
                preferredQuery,
                [this, group, spec, capabilityId, pageRequest, resolvedCursor, requestPriority,
                 communityContinuation, completion = std::move(completion)](
                    foundation::result::Result<std::string> body) mutable {
                    auto process = std::make_shared<std::function<void(
                        foundation::result::Result<std::string>, bool)>>(
                        [this, group, spec, capabilityId, resolvedCursor, requestPriority,
                         completion = std::move(completion)](
                                       foundation::result::Result<std::string> selectedBody,
                                       const bool communityFallback) mutable {
                        if (group->cancelled.load(std::memory_order_acquire)) {
                            return;
                        }
                        if (!selectedBody) {
                            completion(std::unexpected(std::move(selectedBody.error())));
                            return;
                        }
                        auto payload = codec::decodeFeed(*selectedBody);
                        if (!payload) {
                            completion(std::unexpected(std::move(payload.error())));
                            return;
                        }
                        auto providerCursor = std::move(payload->providerCursor);
                        // A provider that returns the same raw cursor cannot make
                        // forward progress. Terminate pagination instead of binding
                        // a fresh opaque token to the same cursor forever.
                        if (providerCursor && resolvedCursor && *providerCursor == *resolvedCursor) {
                            providerCursor.reset();
                        }
                        if (communityFallback && providerCursor) {
                            *providerCursor = std::string{communityCursorPrefix} + *providerCursor;
                        }
                        auto page = pageFromPayload(std::move(*payload));
                        page.communityFallback = communityFallback;
                        switch (spec.filter) {
                        case FeedSpec::Filter::None:
                            break;
                        case FeedSpec::Filter::Posts:
                            filterUserCastPage(page, false);
                            break;
                        case FeedSpec::Filter::Quotes:
                            filterUserCastPage(page, true);
                            break;
                        case FeedSpec::Filter::Replies:
                            filterReplies(page);
                            break;
                        }
                        auto finishPage = [this, group, capabilityId,
                                           providerCursor = std::move(providerCursor),
                                           completion = std::move(completion)](
                                              capability::feed::FeedPage completedPage) mutable {
                            auto completionState = std::make_shared<
                                application::read::async_gateway::FeedCompletion>(std::move(completion));
                            auto status = m_continuationExecutor.post(
                                [this, group, capabilityId, providerCursor = std::move(providerCursor),
                                 page = std::move(completedPage), completionState]() mutable {
                                    if (group->cancelled.load(std::memory_order_acquire) || !*completionState) {
                                        return;
                                    }
                                    continuation::Store continuations{
                                        m_continuations, m_config.provider.id};
                                    auto next = continuations.bind(
                                        capabilityId, std::move(providerCursor));
                                    auto finalCompletion = std::move(*completionState);
                                    if (!next) {
                                        finalCompletion(std::unexpected(std::move(next.error())));
                                        return;
                                    }
                                    page.next = std::move(*next);
                                    finalCompletion(std::move(page));
                                });
                            if (!status && *completionState) {
                                auto finalCompletion = std::move(*completionState);
                                finalCompletion(std::unexpected(std::move(status.error())));
                            }
                        };

                        if (spec.filter == FeedSpec::Filter::Replies && !page.casts.empty()) {
                            const std::string hashes = joinedCastHashes(page.casts);
                            if (!hashes.empty()) {
                                auto hydration = m_client.getJson(
                                    "/v2/farcaster/cast/bulk",
                                    {{"hashes", hashes}},
                                    [group, page, finishPage](
                                        foundation::result::Result<std::string> body) mutable {
                                        if (group->cancelled.load(std::memory_order_acquire)) {
                                            return;
                                        }
                                        if (body) {
                                            auto hydrated = codec::decodeFeed(*body);
                                            if (hydrated) {
                                                mergeHydratedReplyMetrics(page, *hydrated);
                                            }
                                        }
                                        finishPage(std::move(page));
                                    },
                                    request::GetOptions{.timeout = profileFeedRequestTimeout},
                                    requestPriority);
                                if (hydration) {
                                    group->add(std::move(*hydration));
                                    return;
                                }
                            }
                        }
                        finishPage(std::move(page));
                    });

                    if (body || communityContinuation) {
                        (*process)(std::move(body), communityContinuation);
                        return;
                    }

                    // The public Hypersnap ranking endpoint currently stalls
                    // even when following and user casts return in < 1 second.
                    // On a transient ranking failure, show a *community*
                    // discovery stream (the follows of a public seed FID),
                    // never an empty fabricated "trending" result. Its cursor
                    // is marked so subsequent pages stay on the same source.
                    const bool useCommunity = spec.communityDiscoveryFallback
                        && (body.error().code == ErrorCode::Timeout
                            || body.error().code == ErrorCode::Unavailable);
                    if (useCommunity) {
                        auto community = m_client.getJson(
                            "/v2/farcaster/feed/following",
                            {{"fid", "3"}, {"limit", std::to_string(pageRequest.limit)}},
                            [process](foundation::result::Result<std::string> result) mutable {
                                (*process)(std::move(result), true);
                            },
                            request::GetOptions{.timeout = followingRequestTimeout},
                            requestPriority);
                        if (community) {
                            group->add(std::move(*community));
                        } else {
                            (*process)(std::unexpected(std::move(community.error())), false);
                        }
                        return;
                    }

                    if (!spec.fallbackPath || !supportsAliasFallback(body.error().code)) {
                        (*process)(std::move(body), false);
                        return;
                    }
                    auto fallbackQuery = spec.fallbackQuery;
                    fallbackQuery.emplace_back("limit", std::to_string(pageRequest.limit));
                    if (resolvedCursor) {
                        fallbackQuery.emplace_back("cursor", *resolvedCursor);
                    }
                    auto fallback = m_client.getJson(
                        *spec.fallbackPath,
                        fallbackQuery,
                        [process](foundation::result::Result<std::string> fallbackBody) mutable {
                            (*process)(std::move(fallbackBody), false);
                        },
                        request::GetOptions{.timeout = spec.timeout},
                        requestPriority);
                    if (fallback) {
                        group->add(std::move(*fallback));
                    } else {
                        (*process)(std::unexpected(std::move(fallback.error())), false);
                    }
                },
                request::GetOptions{
                    .timeout = communityContinuation ? followingRequestTimeout : spec.timeout},
                requestPriority);
            if (!preferred) {
                completion(std::unexpected(std::move(preferred.error())));
                return;
            }
            group->add(std::move(*preferred));
        });
    if (!scheduled) {
        return std::unexpected(std::move(scheduled.error()));
    }
    return applicationRequest(group);
}

foundation::result::Result<AppRequest> AsyncGateway::search(
    application::read::model::SearchQuery query,
    application::read::async_gateway::SearchCompletion completion,
    const application::read::model::ReadOptions)
{
    if (query.text.empty() || query.limit == 0U || query.limit > 30U || !completion) {
        return std::unexpected(makeError(ErrorCode::InvalidArgument, "Search query, limit and completion are required."));
    }
    struct State final {
        std::mutex mutex{};
        std::optional<foundation::result::Result<std::vector<domain::profile::Profile>>> profiles{};
        std::optional<foundation::result::Result<std::vector<capability::search::ChannelSuggestion>>> channels{};
        application::read::async_gateway::SearchCompletion completion{};
        std::shared_ptr<Group> group{};
    };
    auto group = std::make_shared<Group>();
    auto state = std::make_shared<State>();
    state->completion = std::move(completion);
    state->group = group;
    const auto finish = [state]() {
        std::optional<foundation::result::Result<std::vector<domain::profile::Profile>>> profiles{};
        std::optional<foundation::result::Result<std::vector<capability::search::ChannelSuggestion>>> channels{};
        application::read::async_gateway::SearchCompletion completion{};
        {
            std::scoped_lock lock{state->mutex};
            if (!state->profiles || !state->channels || !state->completion) {
                return;
            }
            profiles = std::move(state->profiles);
            channels = std::move(state->channels);
            completion = std::move(state->completion);
        }
        if (state->group->cancelled.load(std::memory_order_acquire)) {
            return;
        }
        capability::search::SearchPage page{};
        std::optional<Error> firstError{};
        auto profileResult = std::move(*profiles);
        auto channelResult = std::move(*channels);
        if (profileResult) {
            page.profiles = std::move(*profileResult);
        } else {
            firstError = std::move(profileResult.error());
        }
        if (channelResult) {
            page.channels = std::move(*channelResult);
        } else if (!firstError) {
            firstError = std::move(channelResult.error());
        }
        if (page.profiles.empty() && page.channels.empty() && firstError) {
            completion(std::unexpected(std::move(*firstError)));
        } else {
            completion(std::move(page));
        }
    };

    const std::string limit = std::to_string(query.limit);
    auto users = m_client.getJson(
        "/v2/farcaster/user/search",
        {{"q", query.text}, {"limit", limit}},
        [state, finish](foundation::result::Result<std::string> body) mutable {
            foundation::result::Result<std::vector<domain::profile::Profile>> result = body
                ? codec::decodeUsers(*body)
                : foundation::result::Result<std::vector<domain::profile::Profile>>{
                      std::unexpected(std::move(body.error()))};
            {
                std::scoped_lock lock{state->mutex};
                state->profiles = std::move(result);
            }
            finish();
        },
        request::GetOptions{.timeout = std::chrono::milliseconds{4'000}},
        transport::async_http::Priority::Interactive);
    if (!users) {
        return std::unexpected(std::move(users.error()));
    }
    group->add(std::move(*users));

    auto channels = m_client.getJson(
        "/v2/farcaster/channel/search",
        {{"q", query.text}, {"limit", limit}},
        [state, finish](foundation::result::Result<std::string> body) mutable {
            foundation::result::Result<std::vector<capability::search::ChannelSuggestion>> result = body
                ? codec::decodeSearchChannels(*body)
                : foundation::result::Result<std::vector<capability::search::ChannelSuggestion>>{
                      std::unexpected(std::move(body.error()))};
            {
                std::scoped_lock lock{state->mutex};
                state->channels = std::move(result);
            }
            finish();
        },
        request::GetOptions{.timeout = std::chrono::milliseconds{4'000}},
        transport::async_http::Priority::Interactive);
    if (!channels) {
        {
            std::scoped_lock lock{state->mutex};
            state->channels = foundation::result::Result<std::vector<capability::search::ChannelSuggestion>>{
                std::unexpected(std::move(channels.error()))};
        }
        finish();
    } else {
        group->add(std::move(*channels));
    }
    return applicationRequest(group);
}

foundation::result::Result<AppRequest> AsyncGateway::notifications(
    application::read::model::NotificationQuery query,
    application::read::async_gateway::NotificationCompletion completion,
    const application::read::model::ReadOptions)
{
    if (query.viewer.value == 0U || query.limit == 0U || query.limit > 100U || !completion) {
        return std::unexpected(makeError(ErrorCode::InvalidArgument, "Notification query is invalid."));
    }
    auto group = std::make_shared<Group>();
    auto scheduled = m_continuationExecutor.post(
        [this, group, query = std::move(query), completion = std::move(completion)]() mutable {
            continuation::Store continuations{m_continuations, m_config.provider.id};
            const auto capabilityId = capability::catalog::notificationsRead();
            const capability::pagination::PageRequest pageRequest{
                .limit = query.limit,
                .continuation = query.continuation,
            };
            auto cursor = continuations.resolve(capabilityId, pageRequest);
            if (!cursor) {
                completion(std::unexpected(std::move(cursor.error())));
                return;
            }
            std::vector<std::pair<std::string, std::string>> parameters{
                {"fid", std::to_string(query.viewer.value)},
                {"limit", std::to_string(query.limit)},
            };
            if (*cursor) {
                parameters.emplace_back("cursor", **cursor);
            }
            auto operation = m_client.getJson(
                "/v2/farcaster/notifications",
                parameters,
                [this, group, capabilityId, completion = std::move(completion)](
                    foundation::result::Result<std::string> body) mutable {
                    if (!body) {
                        completion(std::unexpected(std::move(body.error())));
                        return;
                    }
                    auto payload = codec::decodeNotifications(*body);
                    if (!payload) {
                        completion(std::unexpected(std::move(payload.error())));
                        return;
                    }
                    auto status = m_continuationExecutor.post(
                        [this, group, capabilityId, payload = std::move(*payload),
                         completion = std::move(completion)]() mutable {
                            if (group->cancelled.load(std::memory_order_acquire)) {
                                return;
                            }
                            continuation::Store continuations{m_continuations, m_config.provider.id};
                            auto next = continuations.bind(
                                capabilityId, std::move(payload.providerCursor));
                            if (!next) {
                                completion(std::unexpected(std::move(next.error())));
                                return;
                            }
                            capability::notification::NotificationPage page{};
                            page.items = std::move(payload.items);
                            page.next = std::move(*next);
                            completion(std::move(page));
                        });
                    if (!status) {
                        completion(std::unexpected(std::move(status.error())));
                    }
                },
                request::GetOptions{.timeout = std::chrono::milliseconds{10'000}},
                transport::async_http::Priority::Visible);
            if (!operation) {
                completion(std::unexpected(std::move(operation.error())));
                return;
            }
            group->add(std::move(*operation));
        });
    if (!scheduled) {
        return std::unexpected(std::move(scheduled.error()));
    }
    return applicationRequest(group);
}

foundation::result::Result<AppRequest> AsyncGateway::social(
    application::read::model::SocialQuery query,
    application::read::async_gateway::SocialCompletion completion,
    const application::read::model::ReadOptions)
{
    if (query.fid.value == 0U || query.limit == 0U || query.limit > 100U || !completion) {
        return std::unexpected(makeError(ErrorCode::InvalidArgument, "Social graph query is invalid."));
    }
    auto group = std::make_shared<Group>();
    auto scheduled = m_continuationExecutor.post(
        [this, group, query = std::move(query), completion = std::move(completion)]() mutable {
            const auto capabilityId = query.kind == application::read::model::SocialQuery::Kind::Followers
                ? capability::catalog::graphFollowersRead()
                : capability::catalog::graphFollowingRead();
            continuation::Store continuations{m_continuations, m_config.provider.id};
            const capability::pagination::PageRequest pageRequest{
                .limit = query.limit,
                .continuation = query.continuation,
            };
            auto cursor = continuations.resolve(capabilityId, pageRequest);
            if (!cursor) {
                completion(std::unexpected(std::move(cursor.error())));
                return;
            }
            std::vector<std::pair<std::string, std::string>> parameters{
                {"fid", std::to_string(query.fid.value)},
                {"limit", std::to_string(query.limit)},
            };
            if (*cursor) {
                parameters.emplace_back("cursor", **cursor);
            }
            const std::string path = query.kind == application::read::model::SocialQuery::Kind::Followers
                ? "/v2/farcaster/user/followers" : "/v2/farcaster/user/following";
            auto operation = m_client.getJson(
                path,
                parameters,
                [this, group, query, capabilityId, completion = std::move(completion)](
                    foundation::result::Result<std::string> body) mutable {
                    if (!body) {
                        completion(std::unexpected(std::move(body.error())));
                        return;
                    }
                    auto payload = codec::decodeSocial(*body);
                    if (!payload) {
                        completion(std::unexpected(std::move(payload.error())));
                        return;
                    }
                    auto status = m_continuationExecutor.post(
                        [this, group, query, capabilityId, payload = std::move(*payload),
                         completion = std::move(completion)]() mutable {
                            if (group->cancelled.load(std::memory_order_acquire)) {
                                return;
                            }
                            capability::social::SocialGraphPage page{};
                            page.profiles = std::move(payload.profiles);
                            page.relations.reserve(page.profiles.size());
                            for (const auto& profile : page.profiles) {
                                const bool followers = query.kind ==
                                    application::read::model::SocialQuery::Kind::Followers;
                                page.relations.push_back(domain::social::SocialRelation{
                                    .source = followers ? profile.fid : query.fid,
                                    .target = followers ? query.fid : profile.fid,
                                    .kind = domain::social::SocialRelationKind::Follows,
                                });
                            }
                            continuation::Store continuations{m_continuations, m_config.provider.id};
                            auto next = continuations.bind(
                                capabilityId, std::move(payload.providerCursor));
                            if (!next) {
                                completion(std::unexpected(std::move(next.error())));
                                return;
                            }
                            page.next = std::move(*next);
                            completion(std::move(page));
                        });
                    if (!status) {
                        completion(std::unexpected(std::move(status.error())));
                    }
                },
                {},
                transport::async_http::Priority::Visible);
            if (!operation) {
                completion(std::unexpected(std::move(operation.error())));
                return;
            }
            group->add(std::move(*operation));
        });
    if (!scheduled) {
        return std::unexpected(std::move(scheduled.error()));
    }
    return applicationRequest(group);
}

foundation::result::Result<AppRequest> AsyncGateway::viewerReactionsFor(
    const domain::identity::Fid viewer,
    std::vector<domain::cast::CastReference> targets,
    std::function<void(foundation::result::Result<domain::feed::ViewerReactionSnapshot>)> completion)
{
    if (viewer.value == 0U || targets.size() > 64U || !completion) {
        return std::unexpected(makeError(ErrorCode::InvalidArgument, "Viewer reaction request is invalid."));
    }
    if (targets.empty()) {
        completion(domain::feed::ViewerReactionSnapshot{});
        return AppRequest{};
    }
    auto group = std::make_shared<Group>();
    auto completionState = std::make_shared<decltype(completion)>(std::move(completion));
    auto targetState = std::make_shared<std::vector<domain::cast::CastReference>>(std::move(targets));

    auto fallback = [this, group, viewer, targetState, completionState]() mutable {
        struct State final {
            std::mutex mutex{};
            std::optional<foundation::result::Result<codec::ReactionTargetsPayload>> likes{};
            std::optional<foundation::result::Result<codec::ReactionTargetsPayload>> recasts{};
        };
        auto state = std::make_shared<State>();
        const auto finish = [group, targetState, completionState, state]() {
            std::optional<foundation::result::Result<codec::ReactionTargetsPayload>> likes{};
            std::optional<foundation::result::Result<codec::ReactionTargetsPayload>> recasts{};
            {
                std::scoped_lock lock{state->mutex};
                if (!state->likes || !state->recasts || !*completionState) {
                    return;
                }
                likes = std::move(state->likes);
                recasts = std::move(state->recasts);
            }
            if (group->cancelled.load(std::memory_order_acquire)) {
                return;
            }
            auto completion = std::move(*completionState);
            auto likeResult = std::move(*likes);
            auto recastResult = std::move(*recasts);
            if (!likeResult) {
                completion(std::unexpected(std::move(likeResult.error())));
                return;
            }
            if (!recastResult) {
                completion(std::unexpected(std::move(recastResult.error())));
                return;
            }
            std::unordered_set<std::string> liked{};
            std::unordered_set<std::string> recasted{};
            for (const auto& target : likeResult->targets) {
                liked.insert(target.castId.value);
            }
            for (const auto& target : recastResult->targets) {
                recasted.insert(target.castId.value);
            }
            domain::feed::ViewerReactionSnapshot snapshot{};
            snapshot.liked.reserve(targetState->size());
            snapshot.recasted.reserve(targetState->size());
            for (const auto& target : *targetState) {
                if (liked.contains(target.id.value)) {
                    snapshot.liked.push_back(target.id);
                }
                if (recasted.contains(target.id.value)) {
                    snapshot.recasted.push_back(target.id);
                }
            }
            completion(std::move(snapshot));
        };
        const auto fallbackLimit = viewerReactionSnapshotLimit;
        const auto startReaction = [this, group, viewer, fallbackLimit, state, finish](
                                       const char* type,
                                       const bool likes) {
            auto operation = m_client.getJson(
                "/v2/farcaster/reaction/user",
                {
                    {"fid", std::to_string(viewer.value)},
                    {"type", type},
                    {"limit", std::to_string(fallbackLimit)},
                },
                [state, finish, likes](foundation::result::Result<std::string> body) mutable {
                    foundation::result::Result<codec::ReactionTargetsPayload> result = body
                        ? codec::decodeReactionTargets(*body)
                        : foundation::result::Result<codec::ReactionTargetsPayload>{
                              std::unexpected(std::move(body.error()))};
                    {
                        std::scoped_lock lock{state->mutex};
                        if (likes) {
                            state->likes = std::move(result);
                        } else {
                            state->recasts = std::move(result);
                        }
                    }
                    finish();
                },
                request::GetOptions{.timeout = viewerReactionRequestTimeout},
                transport::async_http::Priority::Visible);
            if (!operation) {
                std::scoped_lock lock{state->mutex};
                auto errorResult = foundation::result::Result<codec::ReactionTargetsPayload>{
                    std::unexpected(std::move(operation.error()))};
                if (likes) {
                    state->likes = std::move(errorResult);
                } else {
                    state->recasts = std::move(errorResult);
                }
                finish();
                return;
            }
            group->add(std::move(*operation));
        };
        startReaction("likes", true);
        startReaction("recasts", false);
    };

    auto batch = m_client.postReadJson(
        "/v2/farcaster/batch/cast-interactions",
        batchFidBody(viewer),
        [group, viewer, targetState, completionState, fallback](
            foundation::result::Result<std::string> body) mutable {
            if (group->cancelled.load(std::memory_order_acquire) || !*completionState) {
                return;
            }
            if (body) {
                auto decoded = codec::decodeCastInteractions(*body, viewer);
                if (decoded) {
                    std::unordered_set<std::string> targetIds{};
                    targetIds.reserve(targetState->size());
                    for (const auto& target : *targetState) {
                        targetIds.insert(target.id.value);
                    }
                    std::erase_if(decoded->liked, [&](const auto& id) {
                        return !targetIds.contains(id.value);
                    });
                    std::erase_if(decoded->recasted, [&](const auto& id) {
                        return !targetIds.contains(id.value);
                    });
                    auto completion = std::move(*completionState);
                    completion(std::move(*decoded));
                    return;
                }
            }
            fallback();
        },
        request::GetOptions{.timeout = viewerReactionRequestTimeout},
        transport::async_http::Priority::Visible);
    if (!batch) {
        fallback();
    } else {
        group->add(std::move(*batch));
    }
    return applicationRequest(group);
}

foundation::result::Result<AppRequest> AsyncGateway::isFollowing(
    const domain::identity::Fid source,
    const domain::identity::Fid target,
    std::function<void(foundation::result::Result<bool>)> completion)
{
    if (!completion) {
        return std::unexpected(makeError(ErrorCode::InvalidArgument, "Following completion is required."));
    }
    if (source.value == 0U || target.value == 0U || source.value == target.value) {
        completion(false);
        return AppRequest{};
    }
    auto group = std::make_shared<Group>();
    auto operation = m_client.getJson(
        "/v1/linkById",
        {
            {"fid", std::to_string(source.value)},
            {"target_fid", std::to_string(target.value)},
            {"link_type", "follow"},
        },
        [group, completion = std::move(completion)](foundation::result::Result<std::string> body) mutable {
            if (group->cancelled.load(std::memory_order_acquire)) {
                return;
            }
            if (body) {
                completion(true);
                return;
            }
            if (body.error().code == ErrorCode::NotFound) {
                completion(false);
                return;
            }
            completion(std::unexpected(std::move(body.error())));
        },
        request::GetOptions{.timeout = std::chrono::milliseconds{4'000}},
        transport::async_http::Priority::Visible);
    if (!operation) {
        return std::unexpected(std::move(operation.error()));
    }
    group->add(std::move(*operation));
    return applicationRequest(group);
}

}
