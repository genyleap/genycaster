#include <cassert>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

import farcaster.capability.catalog;
import farcaster.capability.model;
import farcaster.capability.write_submit;
import farcaster.domain.write;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.local.write;
import farcaster.provider.model;
import farcaster.routing.health;
import farcaster.routing.policy;
import farcaster.routing.registry;
import farcaster.routing.router;
import farcaster.routing.write.gateway;

namespace {
using namespace farcaster;
using capability::model::CapabilityAccess;
using provider::model::ProviderId;

class MemoryRouteBindings final : public local::write::IWriteRouteBindings {
public:
    std::unordered_map<std::string, local::write::WriteRouteBinding> assigned{};
    foundation::result::Result<std::optional<local::write::WriteRouteBinding>> find(
        const domain::write::IdempotencyKey& key) override
    {
        if (const auto entry = assigned.find(key.value); entry != assigned.end())
            return std::optional<local::write::WriteRouteBinding>{entry->second};
        return std::optional<local::write::WriteRouteBinding>{};
    }
    foundation::result::Result<local::write::WriteRouteBinding> bindOnce(
        const domain::write::IdempotencyKey& key,
        local::write::WriteRouteBinding binding) override
    {
        const auto [item, inserted] = assigned.try_emplace(key.value, binding);
        if (!inserted && (item->second.providerId != binding.providerId
            || item->second.endpointIdentity != binding.endpointIdentity))
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Conflict, "Cannot change durable assignment."));
        return item->second;
    }
};

class FakeWriter final : public capability::write::IWriteSubmitter {
public:
    std::size_t sends{};
    std::size_t checks{};
    foundation::result::Status submit(const domain::write::SignedOperation&) override
    {
        ++sends;
        return {};
    }
    foundation::result::Result<capability::write::ConfirmationState> reconcile(
        const domain::write::SignedOperation&) override
    {
        ++checks;
        return capability::write::ConfirmationState::Absent;
    }
};

[[nodiscard]] routing::registry::ProviderRegistration writeProvider(
    std::string id, const std::uint16_t priority)
{
    return {
        .provider = {
            .id = ProviderId{std::move(id)},
            .displayName = "Generic protocol writer",
            .deployment = provider::model::ProviderDeployment::SelfHosted,
            .privacy = provider::model::ProviderPrivacy::Private,
            .priority = priority,
        },
        .capabilities = {capability::model::CapabilityDescriptor{
            .id = capability::catalog::protocolMessageSubmit(),
            .version = {1, 0},
            .origin = capability::model::CapabilityOrigin::Protocol,
            .quality = capability::model::CapabilityQuality::Native,
            .access = {CapabilityAccess::Write},
            .isSelfHostable = true,
        }},
    };
}

[[nodiscard]] domain::write::SignedOperation operation(const std::string& key)
{
    return {.idempotencyKey = domain::write::IdempotencyKey{key}};
}

void testWriteAssignmentsSurvivePreferenceChangesAndRouterRecreation()
{
    routing::registry::CapabilityRegistry registry{};
    assert(registry.registerProvider(writeProvider("operator-a", 1)));
    assert(registry.registerProvider(writeProvider("operator-b", 2)));
    routing::health::HealthRegistry health{};
    routing::router::CapabilityRouter router{
        registry, health, routing::policy::RoutingPolicy{}};
    MemoryRouteBindings durable{};
    FakeWriter a{}, b{};
    const auto first = operation("canonical-1");
    const auto next = operation("canonical-2");
    {
        routing::write::RoutedWriteSubmitter routed{router, durable, {
            {.providerId = ProviderId{"operator-a"}, .endpointIdentity = "type|endpoint-a", .submitter = &a},
            {.providerId = ProviderId{"operator-b"}, .endpointIdentity = "type|endpoint-b", .submitter = &b},
        }};
        assert(!routed.reconcile(first)); // Ambiguous legacy attempt: never guess.
        assert(routed.submit(first));
        assert(a.sends == 1 && b.sends == 0);
        assert(durable.assigned.at(first.idempotencyKey.value).providerId == "operator-a");
        assert(durable.assigned.at(first.idempotencyKey.value).endpointIdentity == "type|endpoint-a");

        router.setPreferredProviderForAccess(CapabilityAccess::Write,
            {.provider = ProviderId{"operator-b"}, .require = true});
        assert(routed.submit(first)); // Same message must remain on operator A.
        assert(a.sends == 2 && b.sends == 0);
        assert(routed.reconcile(first));
        assert(a.checks == 1 && b.checks == 0);
        assert(routed.submit(next));
        assert(b.sends == 1);
    }

    // Recreate a router and gateway as on process restart. The binding is durable.
    routing::router::CapabilityRouter afterRestart{
        registry, health, routing::policy::RoutingPolicy{}};
    routing::write::RoutedWriteSubmitter recovered{afterRestart, durable, {
        {.providerId = ProviderId{"operator-a"}, .endpointIdentity = "type|endpoint-a", .submitter = &a},
        {.providerId = ProviderId{"operator-b"}, .endpointIdentity = "type|endpoint-b", .submitter = &b},
    }};
    assert(recovered.reconcile(next));
    assert(b.checks == 1);
    assert(recovered.submit(first));
    assert(a.sends == 3);

    // Reusing the same provider ID with a changed underlying endpoint is not
    // a compatible restart: existing messages must never be sent there.
    routing::write::RoutedWriteSubmitter retargeted{afterRestart, durable, {
        {.providerId = ProviderId{"operator-a"},
         .endpointIdentity = "type|a-new-endpoint", .submitter = &a},
        {.providerId = ProviderId{"operator-b"},
         .endpointIdentity = "type|endpoint-b", .submitter = &b},
    }};
    assert(!retargeted.submit(first));
    assert(!retargeted.reconcile(first));
    assert(a.sends == 3 && a.checks == 1);
}

void testMissingAdaptersFailClosedAndNeverBind()
{
    routing::registry::CapabilityRegistry registry{};
    assert(registry.registerProvider(writeProvider("writer-a", 1)));
    assert(registry.registerProvider(writeProvider("writer-b", 2)));
    routing::health::HealthRegistry health{};
    routing::router::CapabilityRouter router{
        registry, health, routing::policy::RoutingPolicy{}};
    MemoryRouteBindings durable{};
    FakeWriter b{};
    routing::write::RoutedWriteSubmitter missingPreferred{router, durable, {
        {.providerId = ProviderId{"writer-b"}, .endpointIdentity = "type|endpoint-b", .submitter = &b},
    }};
    const auto first = operation("canonical-3");
    assert(!missingPreferred.submit(first));
    assert(durable.assigned.empty() && b.sends == 0);

    router.setPreferredProviderForAccess(CapabilityAccess::Write,
        {.provider = ProviderId{"writer-b"}, .require = true});
    assert(missingPreferred.submit(first));
    assert(b.sends == 1);
    health.update({
        .providerId = ProviderId{"writer-b"},
        .capabilityId = capability::catalog::protocolMessageSubmit(),
        .state = routing::health::HealthState::Unavailable,
    });
    assert(!missingPreferred.submit(first));
    assert(b.sends == 1);
    // Bound operations must fail rather than be moved to a newly preferred writer.
    routing::write::RoutedWriteSubmitter noOriginal{router, durable, {}};
    assert(!noOriginal.reconcile(first));
    assert(!noOriginal.submit(first));
    assert(b.sends == 1);

    router.setPreferredProviderForAccess(CapabilityAccess::Write,
        {.provider = ProviderId{"unknown-id"}, .require = true});
    assert(!missingPreferred.submit(operation("canonical-4")));
    assert(durable.assigned.size() == 1);
}

void testBoundManagedWriterRespectsNewStrictSelfHostedPolicy()
{
    routing::registry::CapabilityRegistry registry{};
    auto managed = writeProvider("managed-old", 1);
    managed.provider.deployment = provider::model::ProviderDeployment::PublicManaged;
    managed.provider.privacy = provider::model::ProviderPrivacy::Managed;
    assert(registry.registerProvider(std::move(managed)));
    assert(registry.registerProvider(writeProvider("self-hosted-new", 2)));
    routing::health::HealthRegistry health{};
    MemoryRouteBindings durable{};
    FakeWriter managedWriter{}, independentWriter{};
    const auto signedMessage = operation("legacy-canonical-hash");
    routing::router::CapabilityRouter standard{
        registry, health, routing::policy::RoutingPolicy{}};
    {
        routing::write::RoutedWriteSubmitter writer{standard, durable, {
            {.providerId = ProviderId{"managed-old"},
             .endpointIdentity = "contract|managed-origin", .submitter = &managedWriter},
            {.providerId = ProviderId{"self-hosted-new"},
             .endpointIdentity = "contract|independent-origin", .submitter = &independentWriter},
        }};
        assert(writer.submit(signedMessage));
        assert(managedWriter.sends == 1);
    }
    routing::router::CapabilityRouter strict{registry, health,
        routing::policy::RoutingPolicy{
            .mode = routing::policy::RoutingMode::StrictSelfHosted,
            .allowManagedProviders = false,
        }};
    strict.setPreferredProviderForAccess(CapabilityAccess::Write,
        {.provider = ProviderId{"self-hosted-new"}, .require = true});
    routing::write::RoutedWriteSubmitter restored{strict, durable, {
        {.providerId = ProviderId{"managed-old"},
         .endpointIdentity = "contract|managed-origin", .submitter = &managedWriter},
        {.providerId = ProviderId{"self-hosted-new"},
         .endpointIdentity = "contract|independent-origin", .submitter = &independentWriter},
    }};
    assert(!restored.submit(signedMessage));
    assert(!restored.reconcile(signedMessage));
    assert(managedWriter.sends == 1);
    assert(independentWriter.sends == 0);
}

} // namespace

int main()
{
    testWriteAssignmentsSurvivePreferenceChangesAndRouterRecreation();
    testMissingAdaptersFailClosedAndNeverBind();
    testBoundManagedWriterRespectsNewStrictSelfHostedPolicy();
    return 0;
}
