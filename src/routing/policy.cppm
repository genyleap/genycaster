/*!
 * @file        src/routing/policy.cppm
 * @brief       Provider-neutral routing and privacy policy.
 *
 * @details
 * Defines capability request preferences, deployment constraints and
 * permitted provider selection modes.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstdint>

export module farcaster.routing.policy;

import farcaster.capability.model;
import farcaster.routing.health;
import farcaster.routing.registry;

export namespace farcaster::routing::policy {

/**
 * @brief Privacy/routing mode selected by deployment configuration.
 */
enum class RoutingMode : std::uint8_t {
    Standard,
    Private,
    StrictSelfHosted,
};

/** @brief Provider-neutral user read-route affinity, subordinate to eligibility. */
enum class PreferredNetwork : std::uint8_t {
    Independent,
    Hosted,
};

/**
 * @brief Provider-neutral routing policy constraints.
 */
struct RoutingPolicy final {
    RoutingMode mode{RoutingMode::Standard};
    bool allowManagedProviders{true};
    capability::model::CapabilityQuality minimumQuality{
        capability::model::CapabilityQuality::Experimental};
};

/**
 * @brief Operation-level routing request evaluated against registry and health state.
 */
struct CapabilityRequest final {
    capability::model::CapabilityId capabilityId{};
    capability::model::CapabilityAccess requiredAccess{capability::model::CapabilityAccess::Read};
    capability::model::CapabilityQuality minimumQuality{
        capability::model::CapabilityQuality::Experimental};
    bool allowRecoveryProbe{false};
};

/**
 * @brief Stateless policy evaluator used by the capability router.
 */
class PolicyEngine final {
public:
    /**
     * @brief Tests whether a route candidate satisfies capability and deployment policy.
     * @param candidate Provider capability candidate.
     * @param request Requested capability constraints.
     * @param policy Deployment routing policy.
     * @param health Optional borrowed health snapshot valid for the duration of the call.
     * @return True when the candidate may be considered for selection.
     */
    [[nodiscard]] bool isEligible(
        const registry::RouteCandidate& candidate,
        const CapabilityRequest& request,
        const RoutingPolicy& policy,
        const health::CapabilityHealth* health) const;

    /**
     * @brief Produces a deterministic lower-is-better route score.
     * @param candidate Eligible provider capability candidate.
     * @param health Optional borrowed health snapshot valid for the duration of the call.
     * @return Deterministic route score; lower values are preferred.
     */
    [[nodiscard]] std::uint64_t score(
        const registry::RouteCandidate& candidate,
        const health::CapabilityHealth* health) const;
};

}
