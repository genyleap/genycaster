module;

#include <cstdint>
#include <optional>
#include <string>

export module farcaster.sync.model;

import farcaster.foundation.error;
import farcaster.foundation.time;

export namespace farcaster::sync::model {

/**
 * @brief Stable identifier for one independently synchronized read model.
 */
struct SyncKey final {
    std::string value{};
};

/**
 * @brief Persisted semantic checkpoint independent of provider cursors.
 */
struct SyncCheckpoint final {
    std::uint64_t sequence{};
    foundation::time::Timestamp updatedAt{};
};

/**
 * @brief Runtime phase of one sync coordinator instance.
 */
enum class SyncPhase : std::uint8_t {
    Idle,
    Running,
    Failed,
};

/**
 * @brief Observable sync state snapshot.
 */
struct SyncState final {
    SyncPhase phase{SyncPhase::Idle};
    std::uint64_t generation{};
    std::optional<foundation::error::Error> lastError{};
};

}
