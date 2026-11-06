/*!
 * @file        src/sync/coordinator.cppm
 * @brief       Serialized sync operation coordinator.
 *
 * @details
 * Maintains generation, in-progress and cancellation state for a single
 * authoritative synchronization cycle.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstdint>

export module farcaster.sync.coordinator;

import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.sync.model;

export namespace farcaster::sync::coordinator {

/**
 * @brief Small authoritative state machine for a serialized sync operation.
 *
 * The coordinator performs no networking or persistence. A later sync engine
 * composes it with capability sources and persistence ports on a dedicated sync
 * executor.
 */
class SyncCoordinator final {
public:
    /**
     * @brief Begins a new generation unless one is already running.
     * @return New generation number or a conflict error.
     */
    [[nodiscard]] foundation::result::Result<std::uint64_t> begin();

    /**
     * @brief Completes the currently running generation.
     * @param generation Generation returned by begin().
     * @return Success or a conflict error for stale/inactive generations.
     */
    [[nodiscard]] foundation::result::Status succeed(std::uint64_t generation);

    /**
     * @brief Marks the currently running generation as failed.
     * @param generation Generation returned by begin().
     * @param error Structured failure retained in the state snapshot.
     * @return Success or a conflict error for stale/inactive generations.
     */
    [[nodiscard]] foundation::result::Status fail(
        std::uint64_t generation,
        foundation::error::Error error);

    /**
     * @brief Returns the current immutable state snapshot.
     * @return Copy of the current sync state.
     */
    [[nodiscard]] model::SyncState state() const;

private:
    model::SyncState m_state{};
};

}
