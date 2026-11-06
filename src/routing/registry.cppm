/*!
 * @file        src/routing/registry.cppm
 * @brief       Registered provider capability inventory.
 *
 * @details
 * Tracks provider instances and their operation support without imposing
 * one global default backend.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <optional>
#include <vector>

export module farcaster.routing.registry;

import farcaster.capability.model;
import farcaster.foundation.result;
import farcaster.provider.model;

export namespace farcaster::routing::registry {

/**
 * @brief Registered provider and its operation-level capability declarations.
 */
struct ProviderRegistration final {
    provider::model::ProviderDescriptor provider{};
    std::vector<capability::model::CapabilityDescriptor> capabilities{};
};

/**
 * @brief Candidate provider implementation for a requested capability.
 */
struct RouteCandidate final {
    provider::model::ProviderDescriptor provider{};
    capability::model::CapabilityDescriptor capability{};
};

/**
 * @brief In-memory registry of provider capability declarations.
 *
 * The registry owns provider-neutral descriptors only. Concrete provider
 * objects and SDK types remain in adapter/composition code.
 */
class CapabilityRegistry final {
public:
    /**
     * @brief Registers or atomically replaces one provider declaration.
     * @param registration Owned descriptor moved into the registry on success.
     * @return Success or a validation/conflict error.
     */
    [[nodiscard]] foundation::result::Status
    registerProvider(ProviderRegistration registration);

    /**
     * @brief Removes a provider declaration if present.
     * @param providerId Stable provider identifier.
     */
    void removeProvider(const provider::model::ProviderId& providerId);

    /**
     * @brief Returns candidates that declare the requested capability id.
     * @param capabilityId Operation-level capability identifier.
     * @return Owned candidate descriptors in registration order.
     */
    [[nodiscard]] std::vector<RouteCandidate>
    candidatesFor(const capability::model::CapabilityId& capabilityId) const;

    /**
     * @brief Looks up provider metadata by id.
     * @param providerId Stable provider identifier.
     * @return Provider metadata when registered.
     */
    [[nodiscard]] std::optional<provider::model::ProviderDescriptor>
    provider(const provider::model::ProviderId& providerId) const;

private:
    std::vector<ProviderRegistration> m_registrations{};
};

}
