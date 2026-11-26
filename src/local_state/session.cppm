/*!
 * @file        src/local_state/session.cppm
 * @brief       Normalized client session persistence interface.
 *
 * @details
 * Defines storage of non-secret session state without making local
 * snapshots independent authentication proof.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

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

    /** True only for a store that authenticates every persisted identity with a device key. */
    [[nodiscard]] virtual bool isDeviceBound() const noexcept { return false; }

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
