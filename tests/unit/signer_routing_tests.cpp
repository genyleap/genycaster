#include <cassert>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <utility>

import farcaster.capability.catalog;
import farcaster.capability.model;
import farcaster.capability.signer;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.provider.model;
import farcaster.routing.health;
import farcaster.routing.policy;
import farcaster.routing.registry;
import farcaster.routing.router;
import farcaster.routing.signer.gateway;

namespace {
using namespace farcaster;

class ProbeSigner final : public capability::signer::INativeSigner {
public:
    [[nodiscard]] domain::signer::SignerId signerId() const override { return {"fake-key"}; }
    [[nodiscard]] foundation::result::Result<domain::signer::PublicKey>
    publicKey() const override { return domain::signer::PublicKey{"fake-pubkey"}; }
    [[nodiscard]] foundation::result::Result<domain::signer::Signature>
    sign(const std::string&) override { return domain::signer::Signature{"fake-signature"}; }
};

class RegistryProbe final : public capability::signer::ISignerRegistry {
public:
    int reads{0};
    int adds{0};
    int revokes{0};
    [[nodiscard]] foundation::result::Result<capability::signer::RegistrySnapshot>
    inspect(domain::identity::Fid, const domain::signer::PublicKey&,
            std::optional<domain::identity::Fid>) override
    {
        ++reads;
        return capability::signer::RegistrySnapshot{};
    }

    [[nodiscard]] foundation::result::Result<domain::signer::Registration>
    registerGasless(const capability::signer::KeyAddRequest&,
                    const capability::signer::KeyAddAuthorization&,
                    capability::signer::INativeSigner&) override
    {
        ++adds;
        return domain::signer::Registration{};
    }

    [[nodiscard]] foundation::result::Status selfRevoke(
        domain::identity::Fid, domain::identity::Fid, std::uint32_t,
        foundation::time::Timestamp, capability::signer::INativeSigner&) override
    {
        ++revokes;
        return {};
    }
};

[[nodiscard]] routing::registry::ProviderRegistration provider(
    const std::string& id, const std::uint16_t priority,
    const bool signerCapable = true)
{
    routing::registry::ProviderRegistration result{
        .provider = {
            .id = provider::model::ProviderId{id},
            .displayName = id,
            .deployment = provider::model::ProviderDeployment::SelfHosted,
            .privacy = provider::model::ProviderPrivacy::Private,
            .priority = priority,
        },
    };
    result.capabilities.push_back(capability::model::CapabilityDescriptor{
        .id = capability::catalog::protocolMessageSubmit(),
        .version = {1, 0}, .origin = capability::model::CapabilityOrigin::Protocol,
        .quality = capability::model::CapabilityQuality::Native,
        .access = {capability::model::CapabilityAccess::Write},
        .isSelfHostable = true,
    });
    if (signerCapable) {
        result.capabilities.push_back(capability::model::CapabilityDescriptor{
            .id = capability::catalog::protocolSignerManage(),
            .version = {1, 0}, .origin = capability::model::CapabilityOrigin::Protocol,
            .quality = capability::model::CapabilityQuality::Native,
            .access = {capability::model::CapabilityAccess::Write},
            .isSelfHostable = true,
        });
    }
    return result;
}

void verifySignerOperationsFollowSelectedWriter()
{
    routing::registry::CapabilityRegistry registry{};
    assert(registry.registerProvider(provider("operator-first", 1)));
    assert(registry.registerProvider(provider("operator-second", 5)));
    assert(registry.registerProvider(provider("writer-only", 0, false)));
    routing::health::HealthRegistry health{};
    routing::router::CapabilityRouter router{
        registry, health, routing::policy::RoutingPolicy{}};
    RegistryProbe first{}, second{};
    routing::signer::RoutedSignerRegistry routed{};
    ProbeSigner key{};
    const domain::identity::Fid fid{42};
    const domain::signer::PublicKey publicKey{"pub"};

    // Fail closed if initial composition has not yet bound the router.
    assert(!routed.inspect(fid, publicKey, std::nullopt));
    routed.addSource({
        .providerId = provider::model::ProviderId{"operator-first"},
        .registry = &first,
    });
    routed.addSource({
        .providerId = provider::model::ProviderId{"operator-second"},
        .registry = &second,
    });
    routed.bindRouter(router);
    assert(routed.inspect(fid, publicKey, std::nullopt));
    assert(first.reads == 1 && second.reads == 0);
    assert(routed.registerGasless({}, {}, key));
    assert(first.adds == 1 && second.adds == 0);
    assert(routed.selfRevoke(fid, fid, 1, foundation::time::Timestamp{}, key));
    assert(first.revokes == 1 && second.revokes == 0);

    router.setPreferredProviderForAccess(capability::model::CapabilityAccess::Write,
        {.provider = provider::model::ProviderId{"operator-second"}, .require = true});
    assert(routed.inspect(fid, publicKey, std::nullopt));
    assert(routed.registerGasless({}, {}, key));
    assert(routed.selfRevoke(fid, fid, 2, foundation::time::Timestamp{}, key));
    assert(second.reads == 1 && second.adds == 1 && second.revokes == 1);
    assert(first.reads == 1 && first.adds == 1 && first.revokes == 1);

    // A generic signed-message writer does not become a signer-capable
    // provider just because it advertises protocol.message.submit.
    router.setPreferredProviderForAccess(capability::model::CapabilityAccess::Write,
        {.provider = provider::model::ProviderId{"writer-only"}, .require = true});
    assert(!routed.inspect(fid, publicKey, std::nullopt));
    assert(!routed.registerGasless({}, {}, key));
    assert(!routed.selfRevoke(fid, fid, 3, foundation::time::Timestamp{}, key));
    assert(second.adds == 1 && second.revokes == 1);

    // Health/deployment restrictions outrank explicit user preference.
    router.setPreferredProviderForAccess(capability::model::CapabilityAccess::Write,
        {.provider = provider::model::ProviderId{"operator-second"}, .require = true});
    health.update({
        .providerId = provider::model::ProviderId{"operator-second"},
        .capabilityId = capability::catalog::protocolSignerManage(),
        .state = routing::health::HealthState::Unavailable,
    });
    assert(!routed.inspect(fid, publicKey, std::nullopt));
    assert(!routed.registerGasless({}, {}, key));
    assert(!routed.selfRevoke(fid, fid, 4, foundation::time::Timestamp{}, key));
    assert(first.adds == 1 && second.adds == 1);
}

void verifyMissingAdapterNeverUsesAnotherSigner()
{
    routing::registry::CapabilityRegistry registry{};
    assert(registry.registerProvider(provider("selected", 1)));
    assert(registry.registerProvider(provider("fallback", 2)));
    routing::health::HealthRegistry health{};
    routing::router::CapabilityRouter router{registry, health, {}};
    RegistryProbe fallback{};
    routing::signer::RoutedSignerRegistry routed{};
    routed.addSource({.providerId = provider::model::ProviderId{"fallback"},
                     .registry = &fallback});
    routed.bindRouter(router);
    assert(!routed.inspect(domain::identity::Fid{42}, {"key"}, std::nullopt));
    assert(fallback.reads == 0);
}

} // namespace

int main()
{
    verifySignerOperationsFollowSelectedWriter();
    verifyMissingAdapterNeverUsesAnotherSigner();
    return 0;
}
