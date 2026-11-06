/*!
 * @file        src/routing/signer_gateway.cppm
 * @brief       Routed signer registry operations.
 *
 * @details
 * Selects the configured signer-management adapter while preserving
 * authority and endpoint semantics.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Oct 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <memory>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module farcaster.routing.signer.gateway;

import farcaster.capability.signer;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.provider.model;
import farcaster.routing.router;

export namespace farcaster::routing::signer {

/**
 * @brief Registered signer-management adapter instance, borrowing the
 * configured port. An instance is eligible only when its provider declares
 * the protocol.signer.manage write capability.
 */
struct SourceBinding final {
    provider::model::ProviderId providerId{};
    capability::signer::ISignerRegistry* registry{};
};

/**
 * @brief Routes signer inspection and KEY_ADD/KEY_REMOVE via the selected
 * capability-compatible writer, independent of adapter/protocol vendor.
 *
 * Bind all sources and the router during composition, before exposing the
 * application to asynchronous work. Sources and the borrowed router must
 * outlive this registry and any in-flight operation. No automatic failover
 * happens after route selection or within one registration/revocation call.
 */
class RoutedSignerRegistry final : public capability::signer::ISignerRegistry {
public:
    /** @brief Creates an unbound registry for composition-time injection. */
    RoutedSignerRegistry() = default;

    /** @brief Binds a provider-neutral router before application startup. */
    void bindRouter(const router::CapabilityRouter& router) noexcept;

    /** @brief Registers an adapter instance during composition only. */
    void addSource(SourceBinding binding);

    /** @brief Inspects the signer through the currently selected writer route. */
    [[nodiscard]] foundation::result::Result<capability::signer::RegistrySnapshot> inspect(
        domain::identity::Fid fid,
        const domain::signer::PublicKey& publicKey,
        std::optional<domain::identity::Fid> requestFid) override;

    /** @brief Registers via the selected compatible key-management provider. */
    [[nodiscard]] foundation::result::Result<domain::signer::Registration> registerGasless(
        const capability::signer::KeyAddRequest& request,
        const capability::signer::KeyAddAuthorization& authorization,
        capability::signer::INativeSigner& proofSigner) override;

    /** @brief Revokes via the selected compatible key-management provider. */
    [[nodiscard]] foundation::result::Status selfRevoke(
        domain::identity::Fid fid,
        domain::identity::Fid requestFid,
        std::uint32_t nonce,
        farcaster::foundation::time::Timestamp deadline,
        capability::signer::INativeSigner& signer) override;

private:
    [[nodiscard]] foundation::result::Result<capability::signer::ISignerRegistry*>
    selectedRegistry() const;

    const router::CapabilityRouter* m_router{};
    std::vector<SourceBinding> m_sources{};
};

} // namespace farcaster::routing::signer
