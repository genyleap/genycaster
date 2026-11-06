/*!
 * @file        src/providers/hypersnap/feed.cppm
 * @brief       HyperSnap feed source adapter.
 *
 * @details
 * Reads and normalizes paginated feeds with provider-affine continuation
 * handling.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstddef>
#include <vector>

export module farcaster.provider.hypersnap.feed;

import farcaster.capability.feed;
import farcaster.capability.pagination;
import farcaster.domain.channel;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.feed;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.config;
import farcaster.storage.continuation;
import farcaster.transport.http;

export namespace farcaster::provider::hypersnap::feed {

/** @brief Blocking Hypersnap v2 feed adapter with provider-private cursor isolation. */
class FeedSource final : public capability::feed::IFeedSource {
public:
    /** @brief Borrows transport/continuation storage and owns provider configuration. */
    FeedSource(
        transport::http::IHttpTransport& transport,
        storage::continuation::ContinuationStore& continuations,
        config::Config config);

    [[nodiscard]] foundation::result::Result<capability::feed::FeedPage> following(
        domain::identity::Fid fid,
        const capability::pagination::PageRequest& pageRequest) override;

    [[nodiscard]] foundation::result::Result<capability::feed::FeedPage> trending(
        const capability::pagination::PageRequest& pageRequest) override;

    [[nodiscard]] foundation::result::Result<capability::feed::FeedPage> channel(
        const domain::channel::ChannelId& channelId,
        const capability::pagination::PageRequest& pageRequest) override;

    [[nodiscard]] foundation::result::Result<capability::feed::FeedPage> userPosts(
        domain::identity::Fid fid,
        const capability::pagination::PageRequest& pageRequest) override;

    [[nodiscard]] foundation::result::Result<capability::feed::FeedPage> userQuotes(
        domain::identity::Fid fid,
        const capability::pagination::PageRequest& pageRequest) override;

    [[nodiscard]] foundation::result::Result<capability::feed::FeedPage> userReplies(
        domain::identity::Fid fid,
        const capability::pagination::PageRequest& pageRequest) override;

    [[nodiscard]] foundation::result::Result<capability::feed::FeedPage> userLikes(
        domain::identity::Fid fid,
        const capability::pagination::PageRequest& pageRequest) override;

    [[nodiscard]] foundation::result::Result<capability::feed::FeedPage> userPopular(
        domain::identity::Fid fid,
        const capability::pagination::PageRequest& pageRequest) override;

    /** @brief Reads recent active Like/Recast targets for one signed-in viewer. */
    [[nodiscard]] foundation::result::Result<domain::feed::ViewerReactionSnapshot> viewerReactions(
        domain::identity::Fid fid,
        std::size_t limit = 100);

    /** @brief Resolves batch Like/Recast state for a bounded set of visible casts. */
    [[nodiscard]] foundation::result::Result<domain::feed::ViewerReactionSnapshot> viewerReactionsFor(
        domain::identity::Fid fid,
        const std::vector<domain::cast::CastReference>& targets);

private:
    transport::http::IHttpTransport& m_transport;
    storage::continuation::ContinuationStore& m_continuations;
    config::Config m_config{};
};

}
