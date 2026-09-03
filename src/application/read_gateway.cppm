module;

#include <expected>

#include <string_view>

export module farcaster.application.read.gateway;

import farcaster.application.read.model;
import farcaster.capability.feed;
import farcaster.capability.search;
import farcaster.capability.notification;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.result;
import farcaster.foundation.error;

export namespace farcaster::application::read::gateway {

/**
 * @brief Blocking provider-neutral gateway used only on an I/O executor.
 *
 * Routing and concrete capability adapters remain behind this boundary. The
 * referenced implementation must outlive all in-flight read jobs.
 */
class IReadGateway {
public:
    /** @brief Destroys the gateway boundary. */
    virtual ~IReadGateway() = default;
    /** @brief Resolves a username to a canonical FID. */
    [[nodiscard]] virtual foundation::result::Result<domain::identity::Fid>
    resolveIdentity(std::string_view username, model::ReadOptions options = {}) = 0;
    /** @brief Reads a canonical profile. */
    [[nodiscard]] virtual foundation::result::Result<domain::profile::Profile>
    profile(domain::identity::Fid fid, model::ReadOptions options = {}) = 0;
    /** @brief Reads a canonical cast. */
    [[nodiscard]] virtual foundation::result::Result<domain::cast::Cast>
    cast(const domain::cast::CastId& castId, model::ReadOptions options = {}) = 0;
    /** @brief Reads one canonical feed page for the requested operation. */
    [[nodiscard]] virtual foundation::result::Result<capability::feed::FeedPage>
    feed(const model::FeedQuery& query, model::ReadOptions options = {}) = 0;
    /** @brief Reads one provider-neutral interactive search snapshot. */
    [[nodiscard]] virtual foundation::result::Result<capability::search::SearchPage>
    search(const model::SearchQuery& query, model::ReadOptions options = {}) = 0;
    /** @brief Reads one canonical notification page. */
    [[nodiscard]] virtual foundation::result::Result<capability::notification::NotificationPage>
    notifications(const model::NotificationQuery& query, model::ReadOptions options = {})
    {
        static_cast<void>(query);
        static_cast<void>(options);
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Unsupported,
            "Notification reads are not implemented by this gateway."));
    }
    /** @brief Reads one canonical social graph page. */
    [[nodiscard]] virtual foundation::result::Result<capability::social::SocialGraphPage>
    social(const model::SocialQuery& query, model::ReadOptions options = {}) = 0;
};

}
