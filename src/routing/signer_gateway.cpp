module;

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <utility>

module farcaster.routing.signer.gateway;

import farcaster.capability.catalog;
import farcaster.capability.model;
import farcaster.foundation.error;
import farcaster.routing.policy;

namespace farcaster::routing::signer {

void RoutedSignerRegistry::bindRouter(const router::CapabilityRouter& router) noexcept
{
    m_router = &router;
}

void RoutedSignerRegistry::addSource(SourceBinding binding)
{
    m_sources.push_back(std::move(binding));
}

foundation::result::Result<capability::signer::ISignerRegistry*>
RoutedSignerRegistry::selectedRegistry() const
{
    if (m_router == nullptr) {
        return std::unexpected(farcaster::foundation::error::Error::make(
            farcaster::foundation::error::ErrorCode::Unavailable,
            "Signer routing is not initialized; no request was submitted."));
    }
    const auto selected = m_router->resolve(policy::CapabilityRequest{
        .capabilityId = capability::catalog::protocolSignerManage(),
        .requiredAccess = capability::model::CapabilityAccess::Write,
        .minimumQuality = capability::model::CapabilityQuality::Experimental,
    });
    if (!selected) {
        return std::unexpected(std::move(selected.error()));
    }
    for (const auto& source : m_sources) {
        if (source.providerId.value == selected->provider.id.value && source.registry) {
            return source.registry;
        }
    }
    return std::unexpected(farcaster::foundation::error::Error::make(
        farcaster::foundation::error::ErrorCode::Unavailable,
        "The selected signer-management provider has no registered adapter; "
        "no inspection or key mutation was sent to a different provider."));
}

foundation::result::Result<capability::signer::RegistrySnapshot>
RoutedSignerRegistry::inspect(
    const domain::identity::Fid fid,
    const domain::signer::PublicKey& publicKey,
    const std::optional<domain::identity::Fid> requestFid)
{
    auto source = selectedRegistry();
    if (!source) return std::unexpected(std::move(source.error()));
    return (*source)->inspect(fid, publicKey, requestFid);
}

foundation::result::Result<domain::signer::Registration>
RoutedSignerRegistry::registerGasless(
    const capability::signer::KeyAddRequest& request,
    const capability::signer::KeyAddAuthorization& authorization,
    capability::signer::INativeSigner& proofSigner)
{
    auto source = selectedRegistry();
    if (!source) return std::unexpected(std::move(source.error()));
    return (*source)->registerGasless(request, authorization, proofSigner);
}

foundation::result::Status RoutedSignerRegistry::selfRevoke(
    const domain::identity::Fid fid,
    const domain::identity::Fid requestFid,
    const std::uint32_t nonce,
    const farcaster::foundation::time::Timestamp deadline,
    capability::signer::INativeSigner& signer)
{
    auto source = selectedRegistry();
    if (!source) return std::unexpected(std::move(source.error()));
    return (*source)->selfRevoke(fid, requestFid, nonce, deadline, signer);
}

} // namespace farcaster::routing::signer
