module;

#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

module farcaster.routing.router;

import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.routing.health;
import farcaster.routing.policy;
import farcaster.provider.model;
import farcaster.routing.registry;

namespace farcaster::routing::router {

CapabilityRouter::CapabilityRouter(
    const registry::CapabilityRegistry& registry,
    const health::HealthRegistry& health,
    policy::RoutingPolicy policy,
    PreferredRouteProviders preferredProviders) noexcept
    : m_registry(registry)
    , m_health(health)
    , m_policy(std::move(policy))
    , m_preferredProviders(std::move(preferredProviders))
{
}

void CapabilityRouter::setPreferredNetwork(const policy::PreferredNetwork network) noexcept
{
    m_preferredNetwork.store(network, std::memory_order_relaxed);
}

foundation::result::Result<ProviderRoute>
CapabilityRouter::resolve(const policy::CapabilityRequest& request) const
{
    const auto candidates = m_registry.candidatesFor(request.capabilityId);
    std::optional<ProviderRoute> best{};
    std::uint64_t bestScore{std::numeric_limits<std::uint64_t>::max()};
    bool bestMatchesPreference{false};
    std::optional<provider::model::ProviderId> coolingProvider{};
    std::chrono::milliseconds shortestCooldown{std::chrono::milliseconds::max()};

    for (const registry::RouteCandidate& candidate : candidates) {
        const auto health = m_health.find(candidate.provider.id, request.capabilityId);
        const health::CapabilityHealth* const healthPointer = health ? &*health : nullptr;

        const bool eligibleWithoutRuntimeHealth =
            m_policyEngine.isEligible(candidate, request, m_policy, nullptr);
        if (eligibleWithoutRuntimeHealth && health
            && health->circuit == health::CircuitState::Open
            && !health->probeEligible && !request.allowRecoveryProbe
            && health->cooldownRemaining < shortestCooldown) {
            coolingProvider = candidate.provider.id;
            shortestCooldown = health->cooldownRemaining;
        }

        if (!m_policyEngine.isEligible(candidate, request, m_policy, healthPointer)) {
            continue;
        }

        // Affinity is a lexicographic preference *after* capability, deployment,
        // and health eligibility. It never defeats a hard constraint or excludes
        // a compatible fallback when the preferred adapter is absent.
        const auto preferred = m_preferredNetwork.load(std::memory_order_relaxed);
        const auto& preferredId = preferred == policy::PreferredNetwork::Independent
            ? m_preferredProviders.independent.value : m_preferredProviders.hosted.value;
        const bool matchesPreference = !preferredId.empty()
            && candidate.provider.id.value == preferredId;
        const auto routeScore = m_policyEngine.score(candidate, healthPointer);
        if (!best || (matchesPreference && !bestMatchesPreference)
            || (matchesPreference == bestMatchesPreference && routeScore < bestScore)) {
            best = ProviderRoute{candidate.provider, candidate.capability};
            bestScore = routeScore;
            bestMatchesPreference = matchesPreference;
        }
    }

    if (!best) {
        if (candidates.empty()) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "No provider is registered for capability '"
                    + request.capabilityId.value + "'."));
        }

        if (coolingProvider) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "Capability: " + request.capabilityId.value
                    + "\nProvider: " + coolingProvider->value
                    + "\nResult: CircuitOpen\nCooldownRemaining: "
                    + std::to_string(shortestCooldown.count())
                    + "ms\nNormal navigation failed fast; explicit Retry may request a recovery probe."));
        }

        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Unavailable,
            "Providers are registered for capability '"
                + request.capabilityId.value
                + "', but none are eligible under the current routing policy or health state."));
    }

    return *best;
}

}
