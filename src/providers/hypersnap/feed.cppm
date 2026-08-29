export module farcaster.provider.hypersnap.feed;

import farcaster.capability.feed;
import farcaster.capability.pagination;
import farcaster.domain.channel;
import farcaster.domain.identity;
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

private:
    transport::http::IHttpTransport& m_transport;
    storage::continuation::ContinuationStore& m_continuations;
    config::Config m_config{};
};

}
