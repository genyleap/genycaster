module;

#include <optional>

export module farcaster.local.session;

import farcaster.domain.session;
import farcaster.foundation.result;

export namespace farcaster::local::session {

/** @brief Durable local boundary for the normalized client login session. */
class IClientSessionStore {
public:
    /** @brief Destroys the store. */
    virtual ~IClientSessionStore() = default;

    /** @brief Loads the current normalized session, when one exists. */
    [[nodiscard]] virtual foundation::result::Result<std::optional<domain::session::ClientSession>>
    load() = 0;

    /** @brief Replaces the current normalized session atomically. */
    [[nodiscard]] virtual foundation::result::Status save(
        const domain::session::ClientSession& session) = 0;

    /** @brief Removes the current session without touching public read cache. */
    [[nodiscard]] virtual foundation::result::Status clear() = 0;
};

}
