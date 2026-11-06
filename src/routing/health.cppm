/*!
 * @file        src/routing/health.cppm
 * @brief       Per-capability provider health tracking.
 *
 * @details
 * Models circuit admission, failure observations and cooldown timing for
 * eligible operation routes.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

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

/**
 * @brief Admission lease for one routed source attempt.
 *
 * A non-zero probeId identifies the exact half-open recovery probe admitted by
 * the registry. The identifier is process-local and exists only so cancellation
 * can release that probe without accidentally releasing a newer probe for the
 * same provider/capability pair.
 */
struct AttemptAdmission final {
    bool isProbe{false};
    std::uint64_t probeId{0};
};

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

    /**
     * @brief Releases an admitted recovery probe that ended without a source outcome.
     *
     * Cancellation and other neutral terminal paths must call this for the
     * admission they received. The previous Open state and cooldown are
     * preserved; no latency/error observation is recorded.
     */
    void abandonAttempt(
        const provider::model::ProviderId& providerId,
        const capability::model::CapabilityId& capabilityId,
        AttemptAdmission admission);

    /**
     * @brief Records a successful source call and closes the capability circuit when authoritative.
     * @param admission Admission lease associated with the completed call.
     */
    void recordSuccess(
        const provider::model::ProviderId& providerId,
        const capability::model::CapabilityId& capabilityId,
        std::chrono::milliseconds latency,
        AttemptAdmission admission = {});

    /**
     * @brief Records one retryable source failure and advances the capability circuit when authoritative.
     * @param admission Admission lease associated with the completed call.
     */
    void recordFailure(
        const provider::model::ProviderId& providerId,
        const capability::model::CapabilityId& capabilityId,
        foundation::error::ErrorCode error,
        std::chrono::milliseconds latency,
        AttemptAdmission admission = {});

    /** @brief Returns current injected monotonic time for attempt latency measurement. */
    [[nodiscard]] MonotonicTimePoint now() const noexcept;

private:
    struct ActiveProbe final {
        provider::model::ProviderId providerId{};
        capability::model::CapabilityId capabilityId{};
        std::uint64_t id{0};
    };

    [[nodiscard]] CapabilityHealth* findMutable(
        const provider::model::ProviderId& providerId,
        const capability::model::CapabilityId& capabilityId) noexcept;
    [[nodiscard]] CapabilityHealth& ensure(
        const provider::model::ProviderId& providerId,
        const capability::model::CapabilityId& capabilityId);
    void clearActiveProbe(
        const provider::model::ProviderId& providerId,
        const capability::model::CapabilityId& capabilityId) noexcept;
    [[nodiscard]] bool hasActiveProbe(
        const provider::model::ProviderId& providerId,
        const capability::model::CapabilityId& capabilityId,
        std::uint64_t probeId) const noexcept;

    SteadyClock m_defaultClock{};
    const IMonotonicClock* m_clock{&m_defaultClock};
    CircuitBreakerPolicy m_policy{};
    mutable std::mutex m_mutex{};
    std::vector<CapabilityHealth> m_health{};
    std::vector<ActiveProbe> m_activeProbes{};
    std::uint64_t m_nextProbeId{1};
};

/** @brief Classifies failures that indicate a transient capability-source outage. */
[[nodiscard]] bool isRetryableFailure(foundation::error::ErrorCode code) noexcept;

}
