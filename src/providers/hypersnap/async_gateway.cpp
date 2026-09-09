module;

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <functional>
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

constexpr std::chrono::milliseconds followingRequestTimeout{5'000};
constexpr std::chrono::milliseconds discoveryRequestTimeout{6'000};
constexpr std::chrono::milliseconds profileFeedRequestTimeout{6'000};
constexpr std::chrono::milliseconds viewerReactionRequestTimeout{7'500};
// The reaction snapshot is filtered locally, so a bounded but broad window must
// cover reactions on casts the viewer engaged with recently. Mirrors the exact
// blocking source (feed.cpp viewerReactionsFor) so async reads cannot silently
// drop older reactions that /reaction/user would still report.
constexpr std::size_t viewerReactionSnapshotLimit{10'000};

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

struct FeedSpec final {
    capability::model::CapabilityId capabilityId{};
    std::string path{};
    std::vector<std::pair<std::string, std::string>> query{};
    std::optional<std::string> fallbackPath{};
    std::vector<std::pair<std::string, std::string>> fallbackQuery{};
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
    const application::read::model::ReadOptions)
{
    if (castId.value.empty() || !completion) {
        return std::unexpected(makeError(ErrorCode::InvalidArgument, "Cast id and completion are required."));
    }
    auto group = std::make_shared<Group>();
    auto completionState = std::make_shared<application::read::async_gateway::CastCompletion>(std::move(completion));
    auto fallback = [this, group, castId, completionState]() mutable {
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
            transport::async_http::Priority::Interactive);
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
        transport::async_http::Priority::Interactive);
    if (!conversation) {
        fallback();
    } else {
        group->add(std::move(*conversation));
    }
    return applicationRequest(group);
}

foundation::result::Result<AppRequest> AsyncGateway::feed(
    application::read::model::FeedQuery query,
    application::read::async_gateway::FeedCompletion completion,
    const application::read::model::ReadOptions)
{
    if (!completion) {
        return std::unexpected(makeError(ErrorCode::InvalidArgument, "Feed completion is required."));
    }
    auto limit = validatedLimit(query.limit);
    if (!limit) {
        return std::unexpected(std::move(limit.error()));
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
    case application::read::model::FeedQuery::Kind::Trending:
        spec.capabilityId = capability::catalog::feedTrendingRead();
        spec.path = "/v2/farcaster/feed/trending";
        spec.fallbackPath = "/v2/farcaster/feed";
        spec.fallbackQuery = {{"feed_type", "trending"}};
        spec.timeout = discoveryRequestTimeout;
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

    auto group = std::make_shared<Group>();
    const capability::pagination::PageRequest pageRequest{
        .limit = query.limit,
        .continuation = query.continuation,
    };
    const auto capabilityId = query.kind == application::read::model::FeedQuery::Kind::UserLikes
        ? capability::catalog::feedUserLikesRead() : spec.capabilityId;

    auto scheduled = m_continuationExecutor.post(
        [this, group, query = std::move(query), spec = std::move(spec), capabilityId,
         pageRequest, completion = std::move(completion)]() mutable {
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
                    [this, group, capabilityId, completion = std::move(completion)](
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
                            transport::async_http::Priority::Visible);
                        if (!bulk) {
                            finishEmptyOrHydrated(std::nullopt, std::move(bulk.error()));
                            return;
                        }
                        group->add(std::move(*bulk));
                    },
                    request::GetOptions{.timeout = profileFeedRequestTimeout},
                    transport::async_http::Priority::Visible);
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
            const auto resolvedCursor = *cursor;
            auto preferredQuery = spec.query;
            preferredQuery.emplace_back("limit", std::to_string(pageRequest.limit));
            if (resolvedCursor) {
                preferredQuery.emplace_back("cursor", *resolvedCursor);
            }
            auto preferred = m_client.getJson(
                spec.path,
                preferredQuery,
                [this, group, spec, capabilityId, pageRequest, resolvedCursor,
                 completion = std::move(completion)](
                    foundation::result::Result<std::string> body) mutable {
                    auto process = [this, group, spec, capabilityId, resolvedCursor,
                                    completion = std::move(completion)](
                                       foundation::result::Result<std::string> selectedBody) mutable {
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
                        auto page = pageFromPayload(std::move(*payload));
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
                                    transport::async_http::Priority::Visible);
                                if (hydration) {
                                    group->add(std::move(*hydration));
                                    return;
                                }
                            }
                        }
                        finishPage(std::move(page));
                    };

                    if (body || !spec.fallbackPath ||
                        !supportsAliasFallback(body.error().code)) {
                        process(std::move(body));
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
                        [process = std::move(process)](
                            foundation::result::Result<std::string> fallbackBody) mutable {
                            process(std::move(fallbackBody));
                        },
                        request::GetOptions{.timeout = spec.timeout},
                        transport::async_http::Priority::Visible);
                    if (!fallback) {
                        process(foundation::result::Result<std::string>{
                            std::unexpected(std::move(fallback.error()))});
                        return;
                    }
                    group->add(std::move(*fallback));
                },
                request::GetOptions{.timeout = spec.timeout},
                transport::async_http::Priority::Visible);
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
