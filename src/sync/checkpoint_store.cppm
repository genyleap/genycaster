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
     * @brief Saves the latest checkpoint for a sync key.
     * @param key Stable semantic sync key.
     * @param checkpoint Checkpoint copied by the implementation before return.
     * @return Success or a structured storage failure.
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
