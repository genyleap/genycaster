module;

#include <atomic>

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

    /** @brief Changes affinity for future routes atomically; does not alter active requests or signer authority. */
    void setPreferredNetwork(policy::PreferredNetwork network) noexcept;

private:
    const registry::CapabilityRegistry& m_registry;
    const health::HealthRegistry& m_health;
    policy::RoutingPolicy m_policy{};
    PreferredRouteProviders m_preferredProviders{};
    std::atomic<policy::PreferredNetwork> m_preferredNetwork{policy::PreferredNetwork::Independent};
    policy::PolicyEngine m_policyEngine{};
};

}
