/*!
 * @file        src/sync/checkpoint_store.cppm
 * @brief       Semantic synchronization checkpoint port.
 *
 * @details
 * Defines durable sync progress independently of provider cursor encoding
 * or transport identity.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <optional>

export module farcaster.sync.checkpoint;

import farcaster.foundation.result;
import farcaster.sync.model;

export namespace farcaster::sync::checkpoint {

/**
 * @brief Persistence port for provider-neutral semantic sync checkpoints.
 */
class ICheckpointStore {
public:
    /** @brief Destroys the persistence port. */
    virtual ~ICheckpointStore() = default;

    /**
     * @brief Advances a durable sync waterline monotonically across writers.
     * @param key Stable semantic sync key.
     * @param checkpoint Checkpoint copied by the implementation before return.
     * @return Success on advance or identical replay; Conflict on rewind;
     * InvalidArgument on invalid input, or storage failure.
     * Save entities before advancing this independently durable watermark.
     */
    [[nodiscard]] virtual foundation::result::Status saveCheckpoint(
        const model::SyncKey& key,
        const model::SyncCheckpoint& checkpoint) = 0;

    /**
     * @brief Loads a checkpoint if one exists.
     * @param key Stable semantic sync key.
     * @return Optional owned checkpoint or a structured storage failure.
     */
    [[nodiscard]] virtual foundation::result::Result<std::optional<model::SyncCheckpoint>>
    loadCheckpoint(const model::SyncKey& key) = 0;
};

}
