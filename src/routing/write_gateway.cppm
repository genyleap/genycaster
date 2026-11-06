/*!
 * @file        src/routing/write_gateway.cppm
 * @brief       Routed signed-message submission.
 *
 * @details
 * Preserves the selected writer and durable route identity for safe
 * reconciliation and idempotent writes.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Oct 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;
#include <vector>
#include <string>

export module farcaster.routing.write.gateway;

import farcaster.capability.write_submit;
import farcaster.domain.write;
import farcaster.foundation.result;
import farcaster.local.write;
import farcaster.provider.model;
import farcaster.routing.router;

export namespace farcaster::routing::write {

/** @brief A registered writer implementation; the borrowed port must outlive the router. */
struct SourceBinding final {
    provider::model::ProviderId providerId{};
    // Stable non-secret contract + exact submit/reconciliation endpoint identity.
    // Changing the URL behind the same provider ID must never move in-flight writes.
    std::string endpointIdentity{};
    capability::write::IWriteSubmitter* submitter{};
};

/**
 * @brief Durable provider-neutral router for signed protocol operations.
 *
 * The first submission durably pins its canonical message hash to an eligible
 * provider before any network I/O. All subsequent submit/reconcile calls reuse
 * that exact provider, regardless of changed preferences or health. A legacy
 * attempted message with no binding cannot be reconciled by guessing a node.
 *
 * This object and its borrowed router, store and adapters must outlive all
 * in-flight calls. The application invokes it only from an I/O executor.
 */
class RoutedWriteSubmitter final : public capability::write::IWriteSubmitter {
public:
    RoutedWriteSubmitter(
        const router::CapabilityRouter& router,
        local::write::IWriteRouteBindings& bindings,
        std::vector<SourceBinding> sources) noexcept;

    [[nodiscard]] foundation::result::Status submit(
        const domain::write::SignedOperation& operation) override;

    [[nodiscard]] foundation::result::Result<capability::write::ConfirmationState> reconcile(
        const domain::write::SignedOperation& operation) override;

private:
    [[nodiscard]] foundation::result::Result<capability::write::IWriteSubmitter*>
    boundSource(const domain::write::SignedOperation& operation, bool mayBind);

    const router::CapabilityRouter& m_router;
    local::write::IWriteRouteBindings& m_bindings;
    std::vector<SourceBinding> m_sources{};
};

} // namespace farcaster::routing::write
