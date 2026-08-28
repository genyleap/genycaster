module;

#include <cstdint>
#include <expected>
#include <utility>

module farcaster.sync.coordinator;

import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.sync.model;

namespace farcaster::sync::coordinator {

foundation::result::Result<std::uint64_t> SyncCoordinator::begin()
{
    if (m_state.phase == model::SyncPhase::Running) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Conflict,
            "Sync is already running."));
    }

    ++m_state.generation;
    m_state.phase = model::SyncPhase::Running;
    m_state.lastError.reset();
    return m_state.generation;
}

foundation::result::Status SyncCoordinator::succeed(const std::uint64_t generation)
{
    if (m_state.phase != model::SyncPhase::Running || generation != m_state.generation) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Conflict,
            "Cannot complete a stale or inactive sync generation."));
    }

    m_state.phase = model::SyncPhase::Idle;
    m_state.lastError.reset();
    return {};
}

foundation::result::Status SyncCoordinator::fail(
    const std::uint64_t generation,
    foundation::error::Error error)
{
    if (m_state.phase != model::SyncPhase::Running || generation != m_state.generation) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Conflict,
            "Cannot fail a stale or inactive sync generation."));
    }

    m_state.phase = model::SyncPhase::Failed;
    m_state.lastError = std::move(error);
    return {};
}

model::SyncState SyncCoordinator::state() const
{
    return m_state;
}

}
