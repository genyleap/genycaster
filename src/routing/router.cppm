/*!
 * @file        src/routing/router.cppm
 * @brief       Per-operation capability router.
 *
 * @details
 * Chooses eligible providers using capability support, configured
 * preference and health/selection policy.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <atomic>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

export module farcaster.routing.router;

import farcaster.capability.model;
import farcaster.foundation.result;
import farcaster.routing.health;
import farcaster.routing.policy;
import farcaster.provider.model;
import farcaster.routing.registry;

export namespace farcaster::routing::router {

/**
 * @brief Provider-neutral selected route for one capability operation.
 */
struct ProviderRoute final {
    provider::model::ProviderDescriptor provider{};
    capability::model::CapabilityDescriptor capability{};
};

/**
 * @brief Owned preferred provider identities supplied by executable composition.
 * Empty identities disable affinity for the corresponding read route.
 */
struct PreferredRouteProviders final {
    provider::model::ProviderId independent{};
    provider::model::ProviderId hosted{};
};

/** @brief Preferred provider for a capability/access route; require forbids implicit failover. */
struct ProviderPreference final {
    provider::model::ProviderId provider{};
    bool require{false};
};

/**
 * @brief Resolves a capability independently using registry, health, and policy.
 *
 * The referenced registry and health objects must outlive the router. Resolution
 * is synchronous and performs no network or health probes on the request path.
 */
class CapabilityRouter final {
public:
    /**
     * @brief Creates a router over borrowed registry and health snapshots.
     * @param registry Registry that must outlive this router.
     * @param health Health registry that must outlive this router.
     * @param policy Owned routing policy copied into the router.
     * @param preferredProviders Optional provider identities supplied only
     *        by the executable composition; empty means no affinity.
     */
    CapabilityRouter(
        const registry::CapabilityRegistry& registry,
        const health::HealthRegistry& health,
        policy::RoutingPolicy policy,
        PreferredRouteProviders preferredProviders = {}) noexcept;

    /**
     * @brief Resolves the best currently eligible provider route.
     * @param request Operation-level capability request.
     * @return Selected route or an unavailable error when no candidate qualifies.
     */
    [[nodiscard]] foundation::result::Result<ProviderRoute>
    resolve(const policy::CapabilityRequest& request) const;

    /**
     * @brief Checks whether one explicitly bound provider is still eligible.
     * Unlike resolve(), this cannot choose another provider or apply affinity.
     * Used to enforce privacy/deployment/health constraints for durable writes.
     */
    [[nodiscard]] bool isEligibleProvider(
        const provider::model::ProviderId& providerId,
        const policy::CapabilityRequest& request) const;

    /** @brief Changes affinity for future routes atomically; does not alter active requests or signer authority. */
    void setPreferredNetwork(policy::PreferredNetwork network) noexcept;

    /** Same-network, public read pool. Never used for a write or custom pin. */
    void setPublicReadPool(provider::model::ProviderId primary,
                           std::vector<provider::model::ProviderId> alternatives);

    /**
     * @brief Selects an arbitrary registered provider for one access class.
     * @param access Read, Write, Realtime, or Historical operation access.
     * @param preference Provider identity and optional fail-closed pinning.
     * @note A preferred provider is always checked against declared capability,
     *       deployment policy and health. Changes affect future resolutions only.
     */
    void setPreferredProviderForAccess(
        capability::model::CapabilityAccess access, ProviderPreference preference);

    /** @brief Clears the access-wide affinity, restoring composition defaults. */
    void clearPreferredProviderForAccess(capability::model::CapabilityAccess access);

    /**
     * @brief Overrides the access-wide provider preference for one capability.
     * @param capabilityId Canonical operation id, never a vendor endpoint/path.
     * @param access Required access mode for this operation.
     * @param preference Provider identity and fail-closed pinning policy.
     */
    void setPreferredProviderForCapability(
        const capability::model::CapabilityId& capabilityId,
        capability::model::CapabilityAccess access,
        ProviderPreference preference);

    /** @brief Removes one operation override without affecting other capabilities. */
    void clearPreferredProviderForCapability(
        const capability::model::CapabilityId& capabilityId,
        capability::model::CapabilityAccess access);

private:
    const registry::CapabilityRegistry& m_registry;
    const health::HealthRegistry& m_health;
    policy::RoutingPolicy m_policy{};
    PreferredRouteProviders m_preferredProviders{};
    std::atomic<policy::PreferredNetwork> m_preferredNetwork{policy::PreferredNetwork::Independent};
    mutable std::shared_mutex m_preferencesMutex{};
    std::unordered_map<std::string, ProviderPreference> m_accessPreferences{};
    std::unordered_map<std::string, ProviderPreference> m_capabilityPreferences{};
    provider::model::ProviderId m_publicReadPrimary{};
    std::vector<provider::model::ProviderId> m_publicReadAlternatives{};
    policy::PolicyEngine m_policyEngine{};
};

}
