module;

export module farcaster.provider.hypersnap.notification;

import farcaster.capability.notification;
import farcaster.capability.pagination;
import farcaster.domain.identity;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.config;
import farcaster.storage.continuation;
import farcaster.transport.http;

export namespace farcaster::provider::hypersnap::notification {

/** @brief Hypersnap v2 in-app notification adapter with provider-private cursor isolation. */
class NotificationSource final : public capability::notification::INotificationSource {
public:
    /** @brief Borrows transport/continuation storage and owns provider configuration. */
    NotificationSource(
        transport::http::IHttpTransport& transport,
        storage::continuation::ContinuationStore& continuations,
        config::Config config);

    [[nodiscard]] foundation::result::Result<capability::notification::NotificationPage> notifications(
        domain::identity::Fid viewer,
        const capability::pagination::PageRequest& pageRequest) override;

private:
    transport::http::IHttpTransport& m_transport;
    storage::continuation::ContinuationStore& m_continuations;
    config::Config m_config{};
};

}
