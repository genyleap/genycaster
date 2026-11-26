module;

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <mutex>
#include <shared_mutex>
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
namespace {

[[nodiscard]] std::string accessKey(const capability::model::CapabilityAccess access)
{
    return std::to_string(static_cast<int>(access));
}

[[nodiscard]] std::string capabilityKey(
    const capability::model::CapabilityId& capabilityId,
    const capability::model::CapabilityAccess access)
{
    return accessKey(access) + ":" + capabilityId.value;
}

} // namespace

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

void CapabilityRouter::setPublicReadPool(provider::model::ProviderId primary,
    std::vector<provider::model::ProviderId> alternatives)
{
    std::unique_lock lock{m_preferencesMutex};
    m_publicReadPrimary = std::move(primary);
    m_publicReadAlternatives = std::move(alternatives);
}

void CapabilityRouter::setPreferredProviderForAccess(
    const capability::model::CapabilityAccess access, ProviderPreference preference)
{
    std::unique_lock lock{m_preferencesMutex};
    const auto key = accessKey(access);
    if (preference.provider.value.empty()) {
        m_accessPreferences.erase(key);
    } else {
        m_accessPreferences.insert_or_assign(key, std::move(preference));
    }
}

void CapabilityRouter::clearPreferredProviderForAccess(
    const capability::model::CapabilityAccess access)
{
    std::unique_lock lock{m_preferencesMutex};
    m_accessPreferences.erase(accessKey(access));
}

void CapabilityRouter::setPreferredProviderForCapability(
    const capability::model::CapabilityId& capabilityId,
    const capability::model::CapabilityAccess access,
    ProviderPreference preference)
{
    std::unique_lock lock{m_preferencesMutex};
    const auto key = capabilityKey(capabilityId, access);
    if (preference.provider.value.empty()) {
        m_capabilityPreferences.erase(key);
    } else {
        m_capabilityPreferences.insert_or_assign(key, std::move(preference));
    }
}

void CapabilityRouter::clearPreferredProviderForCapability(
    const capability::model::CapabilityId& capabilityId,
    const capability::model::CapabilityAccess access)
{
    std::unique_lock lock{m_preferencesMutex};
    m_capabilityPreferences.erase(capabilityKey(capabilityId, access));
}

bool CapabilityRouter::isEligibleProvider(
    const provider::model::ProviderId& providerId,
    const policy::CapabilityRequest& request) const
{
    for (const registry::RouteCandidate& candidate : m_registry.candidatesFor(request.capabilityId)) {
        if (candidate.provider.id.value != providerId.value) continue;
        const auto health = m_health.find(candidate.provider.id, request.capabilityId);
        return m_policyEngine.isEligible(
            candidate, request, m_policy, health ? &*health : nullptr);
    }
    return false;
}

foundation::result::Result<ProviderRoute>
CapabilityRouter::resolve(const policy::CapabilityRequest& request) const
{
    const auto candidates = m_registry.candidatesFor(request.capabilityId);
    std::optional<ProviderPreference> explicitPreference{};
    bool specificPreference{false};
    provider::model::ProviderId publicReadPrimary{};
    std::vector<provider::model::ProviderId> publicReadAlternatives{};
    {
        std::shared_lock lock{m_preferencesMutex};
        publicReadPrimary = m_publicReadPrimary;
        publicReadAlternatives = m_publicReadAlternatives;
        const auto specific = m_capabilityPreferences.find(
            capabilityKey(request.capabilityId, request.requiredAccess));
        if (specific != m_capabilityPreferences.end()) {
            explicitPreference = specific->second;
            specificPreference = true;
        } else {
            const auto general = m_accessPreferences.find(accessKey(request.requiredAccess));
            if (general != m_accessPreferences.end()) {
                explicitPreference = general->second;
            }
        }
    }
    const auto network = m_preferredNetwork.load(std::memory_order_relaxed);
    const std::string preferredId = explicitPreference
        ? explicitPreference->provider.value
        : network == policy::PreferredNetwork::Independent
            ? m_preferredProviders.independent.value
            : m_preferredProviders.hosted.value;
    const bool sameNetworkPool = request.requiredAccess == capability::model::CapabilityAccess::Read
        && !specificPreference && explicitPreference && explicitPreference->require
        && !publicReadPrimary.value.empty() && preferredId == publicReadPrimary.value;
    std::optional<ProviderRoute> best{};
    std::uint64_t bestScore{std::numeric_limits<std::uint64_t>::max()};
    bool bestMatchesPreference{false};
    std::optional<provider::model::ProviderId> coolingProvider{};
    std::chrono::milliseconds shortestCooldown{std::chrono::milliseconds::max()};

    for (const registry::RouteCandidate& candidate : candidates) {
        if (explicitPreference && explicitPreference->require
            && candidate.provider.id.value != preferredId) {
            if (!sameNetworkPool || std::none_of(publicReadAlternatives.cbegin(),
                    publicReadAlternatives.cend(), [&](const auto& peer) {
                        return peer.value == candidate.provider.id.value;
                    })) continue;
        }
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
        if (explicitPreference && explicitPreference->require) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "Selected provider '" + preferredId + "' cannot satisfy capability '"
                    + request.capabilityId.value
                    + "' under the current access, health, or deployment policy."
                      " Strict routing forbids implicit fallback."));
        }
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
