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

constexpr std::chrono::milliseconds followingRequestTimeout{10'000};
constexpr std::chrono::milliseconds discoveryRequestTimeout{10'000};
constexpr std::chrono::milliseconds profileFeedRequestTimeout{10'000};
constexpr std::chrono::milliseconds viewerReactionRequestTimeout{7'500};
constexpr std::chrono::milliseconds replyEnrichmentRequestTimeout{4'000};

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

[[nodiscard]] foundation::result::Result<capability::feed::FeedPage> readFeedWithAliasFallback(
    transport::http::IHttpTransport& transport,
    storage::continuation::ContinuationStore& continuationStore,
    const config::Config& config,
    const capability::model::CapabilityId& capabilityId,
    const std::string_view preferredPath,
    std::vector<std::pair<std::string, std::string>> preferredQuery,
    const std::string_view fallbackPath,
    std::vector<std::pair<std::string, std::string>> fallbackQuery,
    const capability::pagination::PageRequest& pageRequest,
    const std::chrono::milliseconds requestTimeout)
{
    auto preferred = readFeed(
        transport,
        continuationStore,
        config,
        capabilityId,
        preferredPath,
        std::move(preferredQuery),
        pageRequest,
        requestTimeout);
    if (preferred) {
        return preferred;
    }

    // Hypersnap documents both explicit feed endpoints and the generic
    // /feed?feed_type=... compatibility route. A node can temporarily have one
    // surface unavailable while the other remains healthy, so retry once on the
    // alias before surfacing a discovery outage. The provider-private cursor is
    // still resolved/bound through the same canonical capability id.
    return readFeed(
        transport,
        continuationStore,
        config,
        capabilityId,
        fallbackPath,
        std::move(fallbackQuery),
        pageRequest,
        requestTimeout);
}

[[nodiscard]] bool hasUsableDiscoveryPage(const capability::feed::FeedPage& page) noexcept
{
    return !page.casts.empty();
}

[[nodiscard]] foundation::result::Result<capability::feed::FeedPage> readDiscoveryChannelsFallback(
    transport::http::IHttpTransport& transport,
    storage::continuation::ContinuationStore& continuationStore,
    const config::Config& config,
    const capability::pagination::PageRequest& pageRequest)
{
    // Channel feeds are computed directly from parent-url casts on Hypersnap and
    // remain useful even when the optional Metrics/Trending service is disabled
    // or temporarily rebuilding. Keep this as a first-page-only recovery corpus:
    // provider cursors from a trending endpoint are not interchangeable with a
    // channel-filter cursor.
    if (pageRequest.continuation) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::NotFound,
            "Discovery channel fallback does not reuse a trending continuation."));
    }

    capability::pagination::PageRequest fallbackRequest{
        .limit = std::max<std::size_t>(pageRequest.limit, 60U),
        .continuation = std::nullopt,
    };
    auto fallback = readFeed(
        transport,
        continuationStore,
        config,
        capability::catalog::feedTrendingRead(),
        "/v2/farcaster/feed/channels",
        {{"channel_ids", "base,dev,developers,programming,builders,art,nft,photography,farcaster,memes"}},
        fallbackRequest,
        discoveryRequestTimeout);
    if (fallback) {
        // Do not expose a cursor bound to a different server-side feed family.
        fallback->next.reset();
    }
    return fallback;
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
        {"type", "likes"},
        {"limit", std::to_string(*limit)},
    };
    if (*cursor) {
        query.emplace_back("cursor", **cursor);
    }

    request::Client requests{transport, config};
    auto body = requests.getJson(
        "/v2/farcaster/reaction/user", query,
        request::GetOptions{.timeout = profileFeedRequestTimeout});
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
    auto followingPage = readFeedWithAliasFallback(
        m_transport,
        m_continuations,
        m_config,
        capability::catalog::feedFollowingRead(),
        "/v2/farcaster/feed/following",
        {{"fid", std::to_string(fid.value)}},
        "/v2/farcaster/feed",
        {{"feed_type", "following"}, {"fid", std::to_string(fid.value)}},
        pageRequest,
        followingRequestTimeout);
    if (followingPage && (hasUsableDiscoveryPage(*followingPage) || pageRequest.continuation)) {
        return followingPage;
    }

    // A public node can have the social/feed projection unavailable while raw
    // channel reads continue to work. Home must degrade to useful discovery
    // rather than becoming an empty surface. Preserve a real Following result
    // whenever it contains data; only first-page failure/empty results recover.
    if (!pageRequest.continuation) {
        auto trendingPage = readFeed(
            m_transport,
            m_continuations,
            m_config,
            capability::catalog::feedFollowingRead(),
            "/v2/farcaster/feed/trending",
            {},
            pageRequest,
            discoveryRequestTimeout);
        if (trendingPage && hasUsableDiscoveryPage(*trendingPage)) {
            return trendingPage;
        }
        auto channelFallback = readDiscoveryChannelsFallback(
            m_transport, m_continuations, m_config, pageRequest);
        if (channelFallback && hasUsableDiscoveryPage(*channelFallback)) {
            return channelFallback;
        }
    }

    return followingPage;
}

foundation::result::Result<capability::feed::FeedPage> FeedSource::trending(
    const capability::pagination::PageRequest& pageRequest)
{
    auto page = readFeedWithAliasFallback(
        m_transport,
        m_continuations,
        m_config,
        capability::catalog::feedTrendingRead(),
        "/v2/farcaster/feed/trending",
        {},
        "/v2/farcaster/feed",
        {{"feed_type", "trending"}},
        pageRequest,
        discoveryRequestTimeout);
    if (page && (hasUsableDiscoveryPage(*page) || pageRequest.continuation)) {
        return page;
    }

    // /feed/for_you and /feed/topic are documented compatibility aliases of
    // trending. Continue through those aliases when the primary endpoint is
    // reachable but temporarily returns an empty projection during index rebuild.
    if (!pageRequest.continuation) {
        auto forYou = readFeed(
            m_transport,
            m_continuations,
            m_config,
            capability::catalog::feedTrendingRead(),
            "/v2/farcaster/feed/for_you",
            {},
            pageRequest,
            discoveryRequestTimeout);
        if (forYou && hasUsableDiscoveryPage(*forYou)) {
            return forYou;
        }

        auto topic = readFeed(
            m_transport,
            m_continuations,
            m_config,
            capability::catalog::feedTrendingRead(),
            "/v2/farcaster/feed/topic",
            {},
            pageRequest,
            discoveryRequestTimeout);
        if (topic && hasUsableDiscoveryPage(*topic)) {
            return topic;
        }

        auto channelFallback = readDiscoveryChannelsFallback(
            m_transport, m_continuations, m_config, pageRequest);
        if (channelFallback && hasUsableDiscoveryPage(*channelFallback)) {
            return channelFallback;
        }

        // If a compatibility endpoint succeeded but was legitimately empty,
        // prefer that successful result over surfacing a stale transport error.
        if (forYou) {
            return forYou;
        }
        if (topic) {
            return topic;
        }
    }

    return page;
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

    // The documented endpoint is reply-only. Keep the canonical parent guard so
    // a stale or mixed compatibility node cannot leak root casts into Replies.
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

    // The compatibility feed may carry an unhydrated cast snapshot whose derived
    // engagement fields are all zero. For visible reply rows, use the conversation
    // endpoint as a bounded best-effort enrichment. Besides aggregate reactions,
    // its direct_replies array gives us a reliable reply count when metrics have
    // not yet been materialized into the feed row.
    request::Client requests{m_transport, m_config};
    for (auto& cast : page->casts) {
        if (cast.engagement.replies != 0) {
            continue;
        }
        auto conversationBody = requests.getJson(
            "/v2/farcaster/cast/conversation",
            {
                {"identifier", cast.id.value},
                {"type", "hash"},
                {"reply_depth", "1"},
            },
            request::GetOptions{.timeout = replyEnrichmentRequestTimeout});
        if (!conversationBody) {
            continue;
        }
        auto conversation = codec::decodeConversationPayload(*conversationBody);
        if (!conversation || conversation->cast.id.value != cast.id.value) {
            continue;
        }
        cast.engagement.replies = std::max(
            cast.engagement.replies, conversation->cast.engagement.replies);
        cast.engagement.likes = std::max(
            cast.engagement.likes, conversation->cast.engagement.likes);
        cast.engagement.recasts = std::max(
            cast.engagement.recasts, conversation->cast.engagement.recasts);
    }
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
    if (fid.value == 0 || limit == 0 || limit > 10'000) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Viewer reaction limit must be between 1 and 10000."));
    }
    if (auto status = request::validateConfig(m_config); !status) {
        return std::unexpected(std::move(status.error()));
    }

    request::Client requests{m_transport, m_config};
    const auto readTargets = [&](const std::string_view type)
        -> foundation::result::Result<std::vector<domain::cast::CastId>> {
        auto body = requests.getJson(
            "/v2/farcaster/reaction/user",
            {
                {"fid", std::to_string(fid.value)},
                {"type", type == "Like" ? "likes" : "recasts"},
                {"limit", std::to_string(limit)},
            },
            request::GetOptions{.timeout = viewerReactionRequestTimeout});
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

foundation::result::Result<domain::feed::ViewerReactionSnapshot> FeedSource::viewerReactionsFor(
    const domain::identity::Fid fid,
    const std::vector<domain::cast::CastReference>& targets)
{
    if (fid.value == 0 || targets.size() > 16U) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Exact viewer reaction lookup requires a viewer and at most 16 cast targets."));
    }
    if (targets.empty()) {
        return domain::feed::ViewerReactionSnapshot{};
    }

    // The live Hypersnap implementation of /batch/cast-interactions currently
    // reports authored-cast interaction metadata, not the documented per-cast
    // liked/recasted booleans. The canonical v2 source of truth for a viewer's
    // active reactions is /reaction/user, so use one bounded snapshot and filter
    // it locally instead of issuing legacy reactionById probes or trusting the
    // incompatible batch response shape.
    auto observed = viewerReactions(fid, 10'000);
    if (!observed) {
        return std::unexpected(std::move(observed.error()));
    }

    domain::feed::ViewerReactionSnapshot snapshot{};
    snapshot.liked.reserve(targets.size());
    snapshot.recasted.reserve(targets.size());
    const auto contains = [](const std::vector<domain::cast::CastId>& ids,
                             const domain::cast::CastId& target) {
        return std::ranges::any_of(ids, [&target](const domain::cast::CastId& candidate) {
            return candidate.value == target.value;
        });
    };
    for (const auto& target : targets) {
        if (contains(observed->liked, target.id)) {
            snapshot.liked.push_back(target.id);
        }
        if (contains(observed->recasted, target.id)) {
            snapshot.recasted.push_back(target.id);
        }
    }
    return snapshot;
}

}
