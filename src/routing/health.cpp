module;

#include <cstddef>
#include <chrono>
#include <expected>
#include <optional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

module farcaster.routing.health;

import farcaster.capability.model;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.model;

namespace farcaster::routing::health {
namespace {

[[nodiscard]] std::chrono::milliseconds normalizedLatency(
    const std::chrono::milliseconds latency) noexcept
{
    return latency < std::chrono::milliseconds::zero()
        ? std::chrono::milliseconds::zero() : latency;
}

void recordObservation(
    CapabilityHealth& health,
    const std::chrono::milliseconds latency,
    const bool failed)
{
    constexpr double historyWeight{0.8};
    constexpr double sampleWeight{0.2};
    const auto sample = normalizedLatency(latency);
    if (health.observedAttempts == 0) {
        health.latency = sample;
        health.errorRate = failed ? 1.0 : 0.0;
    } else {
        const auto weightedLatency =
            (health.latency.count() * 4 + sample.count()) / 5;
        health.latency = std::chrono::milliseconds{weightedLatency};
        health.errorRate = health.errorRate * historyWeight
            + (failed ? sampleWeight : 0.0);
    }
    ++health.observedAttempts;
}

}

MonotonicTimePoint SteadyClock::now() const noexcept
{
    return std::chrono::steady_clock::now();
}

HealthRegistry::HealthRegistry() noexcept = default;

HealthRegistry::HealthRegistry(
    const IMonotonicClock& clock,
    CircuitBreakerPolicy policy) noexcept
    : m_clock(&clock)
    , m_policy(std::move(policy))
{
    if (m_policy.failureThreshold == 0) {
        m_policy.failureThreshold = 1;
    }
    if (m_policy.cooldown < std::chrono::milliseconds::zero()) {
        m_policy.cooldown = std::chrono::milliseconds::zero();
    }
}

void HealthRegistry::update(CapabilityHealth health)
{
    std::scoped_lock lock{m_mutex};
    for (std::size_t index = 0; index < m_health.size(); ++index) {
        CapabilityHealth& current = m_health[index];
        const bool sameProvider =
            current.providerId.value.compare(health.providerId.value) == 0;
        const bool sameCapability =
            current.capabilityId.value.compare(health.capabilityId.value) == 0;

        if (sameProvider && sameCapability) {
            current = std::move(health);
            return;
        }
    }

    m_health.push_back(std::move(health));
}

std::optional<CapabilityHealth> HealthRegistry::find(
    const provider::model::ProviderId& providerId,
    const capability::model::CapabilityId& capabilityId) const
{
    std::scoped_lock lock{m_mutex};
    for (std::size_t index = 0; index < m_health.size(); ++index) {
        const CapabilityHealth& health = m_health[index];
        const bool sameProvider =
            health.providerId.value.compare(providerId.value) == 0;
        const bool sameCapability =
            health.capabilityId.value.compare(capabilityId.value) == 0;

        if (sameProvider && sameCapability) {
            CapabilityHealth snapshot = health;
            snapshot.cooldownRemaining = std::chrono::milliseconds::zero();
            snapshot.probeEligible = false;
            if (snapshot.circuit == CircuitState::Open && snapshot.cooldownUntil) {
                const auto currentTime = now();
                if (currentTime >= *snapshot.cooldownUntil) {
                    snapshot.probeEligible = true;
                } else {
                    snapshot.cooldownRemaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                        *snapshot.cooldownUntil - currentTime);
                }
            }
            return snapshot;
        }
    }

    return std::nullopt;
}

CapabilityHealth* HealthRegistry::findMutable(
    const provider::model::ProviderId& providerId,
    const capability::model::CapabilityId& capabilityId) noexcept
{
    for (std::size_t index = 0; index < m_health.size(); ++index) {
        CapabilityHealth& health = m_health[index];
        if (health.providerId.value == providerId.value
            && health.capabilityId.value == capabilityId.value) {
            return &health;
        }
    }
    return nullptr;
}

CapabilityHealth& HealthRegistry::ensure(
    const provider::model::ProviderId& providerId,
    const capability::model::CapabilityId& capabilityId)
{
    if (CapabilityHealth* const existing = findMutable(providerId, capabilityId)) {
        return *existing;
    }
    m_health.push_back(CapabilityHealth{
        .providerId = providerId,
        .capabilityId = capabilityId,
    });
    return m_health.back();
}

foundation::result::Result<AttemptAdmission> HealthRegistry::beginAttempt(
    const provider::model::ProviderId& providerId,
    const capability::model::CapabilityId& capabilityId,
    const AttemptMode mode)
{
    std::scoped_lock lock{m_mutex};
    CapabilityHealth* const health = findMutable(providerId, capabilityId);
    if (health == nullptr) {
        return AttemptAdmission{};
    }
    if (health->state == HealthState::Unavailable) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Unavailable,
            "Capability '" + capabilityId.value + "' is administratively unavailable for provider '"
                + providerId.value + "'."));
    }
    if (health->circuit == CircuitState::HalfOpen) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Unavailable,
            "Capability '" + capabilityId.value + "' already has a recovery probe in progress for provider '"
                + providerId.value + "'."));
    }
    if (health->circuit != CircuitState::Open) {
        return AttemptAdmission{};
    }

    const bool cooldownExpired = !health->cooldownUntil || now() >= *health->cooldownUntil;
    const bool manualProbe = mode == AttemptMode::ManualRecoveryProbe;
    if (!cooldownExpired && !manualProbe) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            *health->cooldownUntil - now());
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Unavailable,
            "Circuit open for capability '" + capabilityId.value + "' on provider '"
                + providerId.value + "'; retry after " + std::to_string(remaining.count()) + " ms."));
    }

    health->circuit = CircuitState::HalfOpen;
    return AttemptAdmission{.isProbe = true};
}

void HealthRegistry::recordSuccess(
    const provider::model::ProviderId& providerId,
    const capability::model::CapabilityId& capabilityId,
    const std::chrono::milliseconds latency)
{
    std::scoped_lock lock{m_mutex};
    CapabilityHealth& health = ensure(providerId, capabilityId);
    recordObservation(health, latency, false);
    health.state = HealthState::Healthy;
    health.consecutiveFailures = 0;
    health.circuit = CircuitState::Closed;
    health.lastError.reset();
    health.lastSuccessfulAt = now();
    health.cooldownUntil.reset();
}

void HealthRegistry::recordFailure(
    const provider::model::ProviderId& providerId,
    const capability::model::CapabilityId& capabilityId,
    const foundation::error::ErrorCode error,
    const std::chrono::milliseconds latency)
{
    if (!isRetryableFailure(error)) {
        return;
    }

    std::scoped_lock lock{m_mutex};
    CapabilityHealth& health = ensure(providerId, capabilityId);
    const bool probeFailed = health.circuit == CircuitState::HalfOpen;
    recordObservation(health, latency, true);
    ++health.consecutiveFailures;
    health.lastError = error;

    if (probeFailed || health.consecutiveFailures >= m_policy.failureThreshold) {
        health.state = HealthState::Degraded;
        health.circuit = CircuitState::Open;
        health.cooldownUntil = now() + m_policy.cooldown;
        return;
    }

    health.state = HealthState::Suspect;
    health.circuit = CircuitState::Closed;
}

MonotonicTimePoint HealthRegistry::now() const noexcept
{
    return m_clock->now();
}

bool isRetryableFailure(const foundation::error::ErrorCode code) noexcept
{
    using foundation::error::ErrorCode;
    return code == ErrorCode::Timeout
        || code == ErrorCode::TransportFailure
        || code == ErrorCode::Unavailable
        || code == ErrorCode::RateLimited;
}

}
