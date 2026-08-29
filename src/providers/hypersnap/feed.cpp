module;

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <expected>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

module farcaster.provider.hypersnap.feed;

import farcaster.domain.cast;
import farcaster.domain.channel;
import farcaster.domain.identity;
import farcaster.capability.catalog;
import farcaster.capability.model;
import farcaster.domain.feed;
import farcaster.foundation.error;
import farcaster.provider.hypersnap.codec;
import farcaster.provider.hypersnap.continuation;
import farcaster.provider.hypersnap.request;

namespace farcaster::provider::hypersnap::feed {
namespace {

constexpr std::chrono::milliseconds followingRequestTimeout{5'000};
constexpr std::chrono::milliseconds discoveryRequestTimeout{6'000};
constexpr std::chrono::milliseconds profileFeedRequestTimeout{6'000};

[[nodiscard]] foundation::result::Result<std::size_t> validatedLimit(
    const capability::pagination::PageRequest& pageRequest)
{
    if (pageRequest.limit == 0 || pageRequest.limit > 100) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Hypersnap v2 page limit must be between 1 and 100."));
    }
    return pageRequest.limit;
}

[[nodiscard]] foundation::result::Result<capability::feed::FeedPage> readFeed(
    transport::http::IHttpTransport& transport,
    storage::continuation::ContinuationStore& continuationStore,
    const config::Config& config,
    const capability::model::CapabilityId& capabilityId,
    const std::string_view path,
    std::vector<std::pair<std::string, std::string>> query,
    const capability::pagination::PageRequest& pageRequest,
    const std::chrono::milliseconds requestTimeout)
{
    if (auto status = request::validateConfig(config); !status) {
        return std::unexpected(std::move(status.error()));
    }
    auto limit = validatedLimit(pageRequest);
    if (!limit) {
        return std::unexpected(std::move(limit.error()));
    }

    continuation::Store continuations{continuationStore, config.provider.id};
    auto cursor = continuations.resolve(capabilityId, pageRequest);
    if (!cursor) {
        return std::unexpected(std::move(cursor.error()));
    }
    query.emplace_back("limit", std::to_string(*limit));
    if (*cursor) {
        query.emplace_back("cursor", **cursor);
    }

    request::Client requests{transport, config};
    auto body = requests.getJson(
        path,
        query,
        request::GetOptions{.timeout = requestTimeout});
    if (!body) {
        return std::unexpected(std::move(body.error()));
    }
    auto payload = codec::decodeFeed(*body);
    if (!payload) {
        return std::unexpected(std::move(payload.error()));
    }

    capability::feed::FeedPage page{};
    page.casts = std::move(payload->casts);
    page.profiles = std::move(payload->profiles);
    page.entries.reserve(page.casts.size());
    for (const auto& cast : page.casts) {
        page.entries.push_back(domain::feed::FeedEntry{
            .castId = cast.id,
            .sortTime = cast.createdAt,
        });
    }
    auto next = continuations.bind(capabilityId, std::move(payload->providerCursor));
    if (!next) {
        return std::unexpected(std::move(next.error()));
    }
    page.next = std::move(*next);
    return page;
}

[[nodiscard]] bool isQuote(const domain::cast::Cast& cast)
{
    return std::ranges::any_of(cast.embeds, [](const auto& embed) {
        return embed.kind == domain::cast::EmbedKind::CastReference;
    });
}

[[nodiscard]] capability::feed::FeedPage filtered(
    capability::feed::FeedPage page,
    const bool wantQuotes)
{
    std::unordered_set<std::string> retained{};
    std::vector<domain::cast::Cast> casts{};
    casts.reserve(page.casts.size());
    for (auto& cast : page.casts) {
        const bool matches = !cast.parent.has_value() && (isQuote(cast) == wantQuotes);
        if (!matches) { continue; }
        retained.insert(cast.id.value);
        casts.push_back(std::move(cast));
    }
    page.casts = std::move(casts);
    std::erase_if(page.entries, [&](const auto& entry) {
        return !retained.contains(entry.castId.value);
    });
    return page;
}

[[nodiscard]] foundation::result::Result<capability::feed::FeedPage> readLikedFeed(
    transport::http::IHttpTransport& transport,
    storage::continuation::ContinuationStore& continuationStore,
    const config::Config& config,
    const domain::identity::Fid fid,
    const capability::pagination::PageRequest& pageRequest)
{
    if (auto status = request::validateConfig(config); !status) {
        return std::unexpected(std::move(status.error()));
    }
    auto limit = validatedLimit(pageRequest);
    if (!limit) {
        return std::unexpected(std::move(limit.error()));
    }
    const auto capabilityId = capability::catalog::feedUserLikesRead();
    continuation::Store continuations{continuationStore, config.provider.id};
    auto cursor = continuations.resolve(capabilityId, pageRequest);
    if (!cursor) {
        return std::unexpected(std::move(cursor.error()));
    }

    std::vector<std::pair<std::string, std::string>> query{
        {"fid", std::to_string(fid.value)},
        {"reaction_type", "Like"},
        {"pageSize", std::to_string(*limit)},
        {"reverse", "true"},
    };
    if (*cursor) {
        query.emplace_back("pageToken", **cursor);
    }

    request::Client requests{transport, config};
    auto body = requests.getJson(
        "/v1/reactionsByFid", query, request::GetOptions{.timeout = profileFeedRequestTimeout});
    if (!body) {
        return std::unexpected(std::move(body.error()));
    }
    auto reactions = codec::decodeReactionTargets(*body);
    if (!reactions) {
        return std::unexpected(std::move(reactions.error()));
    }

    capability::feed::FeedPage page{};
    std::unordered_set<std::uint64_t> seenProfiles{};
    for (const auto& target : reactions->targets) {
        auto castBody = requests.getJson(
            "/v2/farcaster/cast",
            {{"identifier", target.castId.value}, {"type", "hash"}},
            request::GetOptions{.timeout = profileFeedRequestTimeout});
        if (!castBody) {
            if (castBody.error().code == foundation::error::ErrorCode::NotFound) {
                continue;
            }
            return std::unexpected(std::move(castBody.error()));
        }
        auto cast = codec::decodeCastPayload(*castBody);
        if (!cast) {
            return std::unexpected(std::move(cast.error()));
        }
        page.entries.push_back(domain::feed::FeedEntry{
            .castId = cast->cast.id,
            .sortTime = target.reactedAt,
        });
        if (seenProfiles.insert(cast->author.fid.value).second) {
            page.profiles.push_back(std::move(cast->author));
        }
        page.casts.push_back(std::move(cast->cast));
    }
    auto next = continuations.bind(capabilityId, std::move(reactions->providerCursor));
    if (!next) {
        return std::unexpected(std::move(next.error()));
    }
    page.next = std::move(*next);
    return page;
}

}

FeedSource::FeedSource(
    transport::http::IHttpTransport& transport,
    storage::continuation::ContinuationStore& continuations,
    config::Config config)
    : m_transport(transport), m_continuations(continuations), m_config(std::move(config))
{
}

foundation::result::Result<capability::feed::FeedPage> FeedSource::following(
    const domain::identity::Fid fid,
    const capability::pagination::PageRequest& pageRequest)
{
    return readFeed(
        m_transport,
        m_continuations,
        m_config,
        capability::catalog::feedFollowingRead(),
        "/v2/farcaster/feed/following",
        {{"fid", std::to_string(fid.value)}},
        pageRequest,
        followingRequestTimeout);
}

foundation::result::Result<capability::feed::FeedPage> FeedSource::trending(
    const capability::pagination::PageRequest& pageRequest)
{
    return readFeed(
        m_transport,
        m_continuations,
        m_config,
        capability::catalog::feedTrendingRead(),
        "/v2/farcaster/feed/trending",
        {},
        pageRequest,
        discoveryRequestTimeout);
}

foundation::result::Result<capability::feed::FeedPage> FeedSource::channel(
    const domain::channel::ChannelId& channelId,
    const capability::pagination::PageRequest& pageRequest)
{
    if (channelId.value.empty()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Channel id must not be empty."));
    }
    return readFeed(
        m_transport,
        m_continuations,
        m_config,
        capability::catalog::feedChannelRead(),
        "/v2/farcaster/feed/channels",
        {{"channel_ids", channelId.value}},
        pageRequest,
        discoveryRequestTimeout);
}

foundation::result::Result<capability::feed::FeedPage> FeedSource::userPosts(
    const domain::identity::Fid fid,
    const capability::pagination::PageRequest& pageRequest)
{
    auto page = readFeed(
        m_transport, m_continuations, m_config, capability::catalog::feedUserPostsRead(),
        "/v2/farcaster/feed/user/casts", {{"fid", std::to_string(fid.value)}},
        pageRequest, profileFeedRequestTimeout);
    if (!page) { return std::unexpected(std::move(page.error())); }
    return filtered(std::move(*page), false);
}

foundation::result::Result<capability::feed::FeedPage> FeedSource::userQuotes(
    const domain::identity::Fid fid,
    const capability::pagination::PageRequest& pageRequest)
{
    auto page = readFeed(
        m_transport, m_continuations, m_config, capability::catalog::feedUserQuotesRead(),
        "/v2/farcaster/feed/user/casts", {{"fid", std::to_string(fid.value)}},
        pageRequest, profileFeedRequestTimeout);
    if (!page) { return std::unexpected(std::move(page.error())); }
    return filtered(std::move(*page), true);
}

foundation::result::Result<capability::feed::FeedPage> FeedSource::userReplies(
    const domain::identity::Fid fid,
    const capability::pagination::PageRequest& pageRequest)
{
    auto page = readFeed(
        m_transport, m_continuations, m_config, capability::catalog::feedUserRepliesRead(),
        "/v2/farcaster/feed/user/replies_and_recasts", {{"fid", std::to_string(fid.value)}},
        pageRequest, profileFeedRequestTimeout);
    if (!page) {
        return std::unexpected(std::move(page.error()));
    }

    // Hypersnap's compatibility endpoint intentionally combines replies and
    // recasts. The Profile "Replies" surface must remain semantically exact,
    // so retain only CastAdd messages that have a parent cast. Recasts are
    // reactions and belong to a separate interaction surface, not Replies.
    std::unordered_set<std::string> retained{};
    std::vector<domain::cast::Cast> replies{};
    replies.reserve(page->casts.size());
    for (auto& cast : page->casts) {
        if (!cast.parent.has_value()) {
            continue;
        }
        retained.insert(cast.id.value);
        replies.push_back(std::move(cast));
    }
    page->casts = std::move(replies);
    std::erase_if(page->entries, [&](const auto& entry) {
        return !retained.contains(entry.castId.value);
    });
    return page;
}

foundation::result::Result<capability::feed::FeedPage> FeedSource::userLikes(
    const domain::identity::Fid fid,
    const capability::pagination::PageRequest& pageRequest)
{
    return readLikedFeed(m_transport, m_continuations, m_config, fid, pageRequest);
}

foundation::result::Result<capability::feed::FeedPage> FeedSource::userPopular(
    const domain::identity::Fid fid,
    const capability::pagination::PageRequest& pageRequest)
{
    return readFeed(
        m_transport, m_continuations, m_config, capability::catalog::feedUserPopularRead(),
        "/v2/farcaster/feed/user/popular", {{"fid", std::to_string(fid.value)}},
        pageRequest, profileFeedRequestTimeout);
}

foundation::result::Result<domain::feed::ViewerReactionSnapshot> FeedSource::viewerReactions(
    const domain::identity::Fid fid,
    const std::size_t limit)
{
    if (fid.value == 0 || limit == 0 || limit > 100) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Viewer reaction limit must be between 1 and 100."));
    }
    if (auto status = request::validateConfig(m_config); !status) {
        return std::unexpected(std::move(status.error()));
    }

    request::Client requests{m_transport, m_config};
    const auto readTargets = [&](const std::string_view type)
        -> foundation::result::Result<std::vector<domain::cast::CastId>> {
        auto body = requests.getJson(
            "/v1/reactionsByFid",
            {
                {"fid", std::to_string(fid.value)},
                {"reaction_type", std::string{type}},
                {"pageSize", std::to_string(limit)},
                {"reverse", "true"},
            },
            request::GetOptions{.timeout = std::chrono::milliseconds{2'000}});
        if (!body) {
            return std::unexpected(std::move(body.error()));
        }
        auto payload = codec::decodeReactionTargets(*body);
        if (!payload) {
            return std::unexpected(std::move(payload.error()));
        }
        std::vector<domain::cast::CastId> ids{};
        ids.reserve(payload->targets.size());
        for (auto& target : payload->targets) {
            ids.push_back(std::move(target.castId));
        }
        return ids;
    };

    auto liked = readTargets("Like");
    if (!liked) {
        return std::unexpected(std::move(liked.error()));
    }
    auto recasted = readTargets("Recast");
    if (!recasted) {
        return std::unexpected(std::move(recasted.error()));
    }
    return domain::feed::ViewerReactionSnapshot{
        .liked = std::move(*liked),
        .recasted = std::move(*recasted),
    };
}

}
