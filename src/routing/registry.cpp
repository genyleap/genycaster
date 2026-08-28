module;

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

module farcaster.routing.registry;

import farcaster.capability.model;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.model;

namespace farcaster::routing::registry {

foundation::result::Status CapabilityRegistry::registerProvider(ProviderRegistration registration)
{
    if (registration.provider.id.value.empty()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Provider id must not be empty."));
    }

    std::unordered_set<std::string> capabilityIds{};
    for (const capability::model::CapabilityDescriptor& descriptor : registration.capabilities) {
        if (descriptor.id.value.empty()) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Capability id must not be empty."));
        }
        if (!capabilityIds.insert(descriptor.id.value).second) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Conflict,
                "Provider registration contains duplicate capability ids."));
        }
    }

    for (std::size_t index = 0; index < m_registrations.size(); ++index) {
        ProviderRegistration& current = m_registrations[index];
        if (current.provider.id.value.compare(registration.provider.id.value) == 0) {
            current = std::move(registration);
            return {};
        }
    }

    m_registrations.push_back(std::move(registration));
    return {};
}

void CapabilityRegistry::removeProvider(const provider::model::ProviderId& providerId)
{
    std::vector<ProviderRegistration> remaining{};
    remaining.reserve(m_registrations.size());

    for (std::size_t index = 0; index < m_registrations.size(); ++index) {
        ProviderRegistration& registration = m_registrations[index];
        if (registration.provider.id.value.compare(providerId.value) != 0) {
            remaining.push_back(std::move(registration));
        }
    }

    m_registrations = std::move(remaining);
}

std::vector<RouteCandidate>
CapabilityRegistry::candidatesFor(const capability::model::CapabilityId& capabilityId) const
{
    std::vector<RouteCandidate> result{};

    for (std::size_t registrationIndex = 0;
         registrationIndex < m_registrations.size();
         ++registrationIndex) {
        const ProviderRegistration& registration = m_registrations[registrationIndex];

        for (std::size_t capabilityIndex = 0;
             capabilityIndex < registration.capabilities.size();
             ++capabilityIndex) {
            const capability::model::CapabilityDescriptor& descriptor =
                registration.capabilities[capabilityIndex];

            if (descriptor.id.value.compare(capabilityId.value) == 0) {
                result.push_back(RouteCandidate{registration.provider, descriptor});
                break;
            }
        }
    }

    return result;
}

std::optional<provider::model::ProviderDescriptor>
CapabilityRegistry::provider(const provider::model::ProviderId& providerId) const
{
    for (std::size_t index = 0; index < m_registrations.size(); ++index) {
        const ProviderRegistration& registration = m_registrations[index];
        if (registration.provider.id.value.compare(providerId.value) == 0) {
            return registration.provider;
        }
    }

    return std::nullopt;
}

}
