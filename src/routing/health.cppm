module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <mutex>
#include <vector>

export module farcaster.routing.health;

import farcaster.capability.model;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.model;

export namespace farcaster::routing::health {

/**
 * @brief Runtime health classification for one provider capability.
 */
enum class HealthState : std::uint8_t {
    Unknown,
    Healthy,
    Suspect,
    Degraded,
    Unavailable,
};

/** @brief Circuit admission state for one provider capability. */
enum class CircuitState : std::uint8_t { Closed, Open, HalfOpen };

/** @brief Provider-neutral request intent evaluated by circuit admission. */
enum class AttemptMode : std::uint8_t { Normal, ManualRecoveryProbe };

using MonotonicTimePoint = std::chrono::steady_clock::time_point;

/** @brief Injected monotonic time source used for deterministic cooldowns. */
class IMonotonicClock {
public:
    virtual ~IMonotonicClock() = default;
    /** @brief Returns the current monotonic time. */
    [[nodiscard]] virtual MonotonicTimePoint now() const noexcept = 0;
};

/** @brief Production monotonic clock backed by `std::chrono::steady_clock`. */
class SteadyClock final : public IMonotonicClock {
public:
    /** @inheritdoc */
    [[nodiscard]] MonotonicTimePoint now() const noexcept override;
};

/** @brief Deterministic thresholds applied independently to every capability circuit. */
struct CircuitBreakerPolicy final {
    std::size_t failureThreshold{2};
    std::chrono::milliseconds cooldown{30'000};
};

/**
 * @brief Latest health signal for a provider capability pair.
 */
struct CapabilityHealth final {
    provider::model::ProviderId providerId{};
    capability::model::CapabilityId capabilityId{};
    HealthState state{HealthState::Unknown};
    std::chrono::milliseconds latency{0};
    double errorRate{0.0};
    std::size_t observedAttempts{0};
    std::size_t consecutiveFailures{0};
    CircuitState circuit{CircuitState::Closed};
    std::optional<foundation::error::ErrorCode> lastError{};
    std::optional<MonotonicTimePoint> lastSuccessfulAt{};
    std::optional<MonotonicTimePoint> cooldownUntil{};
    std::chrono::milliseconds cooldownRemaining{0};
    bool probeEligible{false};
};

/** @brief Admission result describing whether the accepted call is a probe. */
struct AttemptAdmission final { bool isProbe{false}; };

/**
 * @brief Thread-safe runtime health and circuit state for routed reads.
 *
 * Admission, health snapshots, and outcome recording are synchronized so
 * independent live reads may execute concurrently on a bounded worker pool.
 * Network health probes remain outside the router itself.
 */
class HealthRegistry final {
public:
    /** @brief Creates a registry using the production steady clock and default policy. */
    HealthRegistry() noexcept;

    /**
     * @brief Creates a registry using a borrowed monotonic clock and owned policy.
     * @param clock Clock that must outlive the registry.
     * @param policy Circuit thresholds normalized to safe non-negative values.
     */
    explicit HealthRegistry(
        const IMonotonicClock& clock,
        CircuitBreakerPolicy policy = {}) noexcept;

    /**
     * @brief Inserts or replaces the latest health sample.
     * @param health Owned health sample moved into the registry.
     */
    void update(CapabilityHealth health);

    /**
     * @brief Returns the latest health sample if one has been observed.
     * @param providerId Stable provider identifier.
     * @param capabilityId Operation-level capability identifier.
     * @return Latest sample when available.
     */
    [[nodiscard]] std::optional<CapabilityHealth> find(
        const provider::model::ProviderId& providerId,
        const capability::model::CapabilityId& capabilityId) const;

    /**
     * @brief Atomically admits a normal attempt or one bounded recovery probe.
     * @return Admission metadata, or a fast Unavailable failure while the circuit is open.
     */
    [[nodiscard]] foundation::result::Result<AttemptAdmission> beginAttempt(
        const provider::model::ProviderId& providerId,
        const capability::model::CapabilityId& capabilityId,
        AttemptMode mode);

    /** @brief Records a successful source call and closes the capability circuit. */
    void recordSuccess(
        const provider::model::ProviderId& providerId,
        const capability::model::CapabilityId& capabilityId,
        std::chrono::milliseconds latency);

    /** @brief Records one retryable source failure and advances the capability circuit. */
    void recordFailure(
        const provider::model::ProviderId& providerId,
        const capability::model::CapabilityId& capabilityId,
        foundation::error::ErrorCode error,
        std::chrono::milliseconds latency);

    /** @brief Returns current injected monotonic time for attempt latency measurement. */
    [[nodiscard]] MonotonicTimePoint now() const noexcept;

private:
    [[nodiscard]] CapabilityHealth* findMutable(
        const provider::model::ProviderId& providerId,
        const capability::model::CapabilityId& capabilityId) noexcept;
    [[nodiscard]] CapabilityHealth& ensure(
        const provider::model::ProviderId& providerId,
        const capability::model::CapabilityId& capabilityId);

    SteadyClock m_defaultClock{};
    const IMonotonicClock* m_clock{&m_defaultClock};
    CircuitBreakerPolicy m_policy{};
    mutable std::mutex m_mutex{};
    std::vector<CapabilityHealth> m_health{};
};

/** @brief Classifies failures that indicate a transient capability-source outage. */
[[nodiscard]] bool isRetryableFailure(foundation::error::ErrorCode code) noexcept;

}
