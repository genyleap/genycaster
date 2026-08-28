module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>

module farcaster.routing.policy;

import farcaster.capability.model;
import farcaster.routing.health;
import farcaster.provider.model;
import farcaster.routing.registry;

namespace farcaster::routing::policy {
namespace {

[[nodiscard]] std::uint64_t qualityPenalty(const capability::model::CapabilityQuality quality)
{
    switch (quality) {
    case capability::model::CapabilityQuality::Native:
        return 0;
    case capability::model::CapabilityQuality::Derived:
        return 10'000;
    case capability::model::CapabilityQuality::Emulated:
        return 20'000;
    case capability::model::CapabilityQuality::Partial:
        return 30'000;
    case capability::model::CapabilityQuality::Experimental:
        return 40'000;
    case capability::model::CapabilityQuality::Unavailable:
        return std::numeric_limits<std::uint64_t>::max() / 2;
    }
    return std::numeric_limits<std::uint64_t>::max() / 2;
}

[[nodiscard]] bool qualityAtLeast(
    const capability::model::CapabilityQuality offered,
    const capability::model::CapabilityQuality minimum)
{
    return qualityPenalty(offered) <= qualityPenalty(minimum);
}

[[nodiscard]] bool supportsAccess(
    const capability::model::CapabilityDescriptor& capability,
    const capability::model::CapabilityAccess requiredAccess)
{
    for (std::size_t index = 0; index < capability.access.size(); ++index) {
        if (capability.access[index] == requiredAccess) {
            return true;
        }
    }

    return false;
}

}

bool PolicyEngine::isEligible(
    const registry::RouteCandidate& candidate,
    const CapabilityRequest& request,
    const RoutingPolicy& policy,
    const health::CapabilityHealth* const health) const
{
    if (candidate.capability.id.value.compare(request.capabilityId.value) != 0) {
        return false;
    }

    if (!qualityAtLeast(candidate.capability.quality, request.minimumQuality)
        || !qualityAtLeast(candidate.capability.quality, policy.minimumQuality)) {
        return false;
    }

    if (!supportsAccess(candidate.capability, request.requiredAccess)) {
        return false;
    }

    if (!policy.allowManagedProviders
        && candidate.provider.deployment == provider::model::ProviderDeployment::PublicManaged) {
        return false;
    }

    if (policy.mode == RoutingMode::Private
        && candidate.provider.privacy == provider::model::ProviderPrivacy::Managed) {
        return false;
    }

    if (policy.mode == RoutingMode::StrictSelfHosted
        && candidate.provider.deployment != provider::model::ProviderDeployment::SelfHosted
        && candidate.provider.deployment != provider::model::ProviderDeployment::Local) {
        return false;
    }

    if (policy.mode == RoutingMode::StrictSelfHosted
        && !candidate.capability.isSelfHostable) {
        return false;
    }

    if (health == nullptr) {
        return true;
    }
    if (health->state == health::HealthState::Unavailable
        || health->circuit == health::CircuitState::HalfOpen) {
        return false;
    }
    if (health->circuit == health::CircuitState::Open
        && !health->probeEligible
        && !request.allowRecoveryProbe) {
        return false;
    }
    return true;
}

std::uint64_t PolicyEngine::score(
    const registry::RouteCandidate& candidate,
    const health::CapabilityHealth* const health) const
{
    constexpr std::uint64_t priorityWeight{1'000'000};
    constexpr std::uint64_t suspectPenalty{100'000};
    constexpr std::uint64_t degradedPenalty{500'000};

    std::uint64_t result = static_cast<std::uint64_t>(candidate.provider.priority) * priorityWeight;
    result += qualityPenalty(candidate.capability.quality);

    if (health != nullptr) {
        if (health->state == health::HealthState::Suspect) {
            result += suspectPenalty;
        } else if (health->state == health::HealthState::Degraded) {
            result += degradedPenalty;
        }

        const auto latencyCount = health->latency.count();
        if (latencyCount > 0) {
            result += static_cast<std::uint64_t>(latencyCount);
        }
    }

    return result;
}

}
