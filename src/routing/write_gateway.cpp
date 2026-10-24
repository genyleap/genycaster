module;
#include <expected>
#include <string>
#include <utility>
#include <vector>

module farcaster.routing.write.gateway;

import farcaster.capability.catalog;
import farcaster.capability.model;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.model;
import farcaster.routing.policy;

namespace farcaster::routing::write {
namespace {
[[nodiscard]] foundation::error::Error unavailable(std::string description)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::Unavailable, std::move(description));
}
} // namespace

RoutedWriteSubmitter::RoutedWriteSubmitter(
    const router::CapabilityRouter& router,
    local::write::IWriteRouteBindings& bindings,
    std::vector<SourceBinding> sources) noexcept
    : m_router(router), m_bindings(bindings), m_sources(std::move(sources))
{
}

foundation::result::Result<capability::write::IWriteSubmitter*>
RoutedWriteSubmitter::boundSource(
    const domain::write::SignedOperation& operation,
    const bool mayBind)
{
    if (operation.idempotencyKey.value.empty()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Canonical write must have a non-empty idempotency key."));
    }
    auto existing = m_bindings.find(operation.idempotencyKey);
    if (!existing) return std::unexpected(std::move(existing.error()));

    local::write::WriteRouteBinding binding{};
    if (*existing) {
        binding = **existing;
    } else {
        if (!mayBind) {
            return std::unexpected(unavailable(
                "Pending canonical write has no durable provider binding. "
                "Reconciliation cannot safely assume a different node; no resubmission was made."));
        }
        auto selected = m_router.resolve(policy::CapabilityRequest{
            .capabilityId = capability::catalog::protocolMessageSubmit(),
            .requiredAccess = capability::model::CapabilityAccess::Write,
            .minimumQuality = capability::model::CapabilityQuality::Experimental,
        });
        if (!selected) return std::unexpected(std::move(selected.error()));
        binding.providerId = selected->provider.id.value;

        // Validate that an actual adapter exists *before* creating a binding;
        // a stale registry registration must not permanently poison an outbox.
        bool hasSource = false;
        for (const SourceBinding& candidate : m_sources) {
            if (candidate.providerId.value == binding.providerId && candidate.submitter
                && !candidate.endpointIdentity.empty()) {
                hasSource = true;
                binding.endpointIdentity = candidate.endpointIdentity;
                break;
            }
        }
        if (!hasSource) {
            return std::unexpected(unavailable(
                "Selected write provider has no active adapter; write was not transmitted."));
        }
        // Durable assignment happens before the first call to post(). Even if
        // this process crashes, restart/retry can never move the same message.
        auto bound = m_bindings.bindOnce(operation.idempotencyKey, std::move(binding));
        if (!bound) return std::unexpected(std::move(bound.error()));
        binding = std::move(*bound);
    }

    // Pinning forbids reassignment, but does not exempt old destinations from
    // current strict self-hosted/privacy policy or health eligibility.
    if (!m_router.isEligibleProvider(
            provider::model::ProviderId{binding.providerId},
            policy::CapabilityRequest{
                .capabilityId = capability::catalog::protocolMessageSubmit(),
                .requiredAccess = capability::model::CapabilityAccess::Write,
                .minimumQuality = capability::model::CapabilityQuality::Experimental,
            })) {
        return std::unexpected(unavailable(
            "Bound write provider is not eligible under current health/deployment policy. "
            "Write remains bound and was not sent to another provider."));
    }
    for (const SourceBinding& source : m_sources) {
        if (source.providerId.value == binding.providerId && source.submitter
            && source.endpointIdentity == binding.endpointIdentity) {
            return source.submitter;
        }
    }
    return std::unexpected(unavailable(
        "The previously bound write provider or exact endpoint identity is unavailable. "
        "Retaining the message without switching destinations."));
}

foundation::result::Status RoutedWriteSubmitter::submit(
    const domain::write::SignedOperation& operation)
{
    auto source = boundSource(operation, true);
    if (!source) return std::unexpected(std::move(source.error()));
    return (*source)->submit(operation);
}

foundation::result::Result<capability::write::ConfirmationState>
RoutedWriteSubmitter::reconcile(const domain::write::SignedOperation& operation)
{
    auto source = boundSource(operation, false);
    if (!source) return std::unexpected(std::move(source.error()));
    return (*source)->reconcile(operation);
}

} // namespace farcaster::routing::write
