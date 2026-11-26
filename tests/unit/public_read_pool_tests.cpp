#include <cassert>
#include <chrono>
#include <string>

import farcaster.capability.model;
import farcaster.foundation.error;
import farcaster.provider.model;
import farcaster.routing.health;
import farcaster.routing.policy;
import farcaster.routing.registry;
import farcaster.routing.router;

int main()
{
    using namespace farcaster;
    using namespace capability::model;
    using namespace provider::model;
    using namespace routing;
    const CapabilityId readId{"test.read"};
    const CapabilityId writeId{"test.write"};
    const ProviderId primary{"primary"}, slow{"peer-slow"}, fast{"peer-fast"}, unrelated{"private"};
    registry::CapabilityRegistry registry;
    const auto add = [&](const ProviderId& id) {
        assert(registry.registerProvider({
            .provider = {.id = id, .displayName = id.value, .priority = 100},
            .capabilities = {
                {.id = readId, .quality = CapabilityQuality::Native, .access = {CapabilityAccess::Read}},
                {.id = writeId, .quality = CapabilityQuality::Native, .access = {CapabilityAccess::Write}},
            },
        }));
    };
    add(primary); add(slow); add(fast); add(unrelated);
    health::HealthRegistry health;
    router::CapabilityRouter router{registry, health, {}};
    router.setPublicReadPool(primary, {slow, fast});
    router.setPreferredProviderForAccess(CapabilityAccess::Read, {.provider = primary, .require = true});
    router.setPreferredProviderForAccess(CapabilityAccess::Write, {.provider = primary, .require = true});
    const policy::CapabilityRequest read{.capabilityId = readId, .requiredAccess = CapabilityAccess::Read};
    const policy::CapabilityRequest write{.capabilityId = writeId, .requiredAccess = CapabilityAccess::Write};
    for (const auto& peer : {slow, fast, unrelated}) {
        health.update({.providerId = peer, .capabilityId = readId, .state = health::HealthState::Healthy,
            .latency = std::chrono::milliseconds{peer.value == slow.value ? 500 : peer.value == fast.value ? 80 : 1}});
    }
    assert(router.resolve(read)->provider.id.value == primary.value);
    health.recordFailure(primary, readId, foundation::error::ErrorCode::Timeout, std::chrono::milliseconds{1000});
    assert(router.resolve(read)->provider.id.value == primary.value);
    health.recordFailure(primary, readId, foundation::error::ErrorCode::Timeout, std::chrono::milliseconds{1000});
    assert(router.resolve(read)->provider.id.value == fast.value);
    assert(router.resolve(write)->provider.id.value == primary.value);
    health.update({.providerId = fast, .capabilityId = readId, .state = health::HealthState::Unavailable});
    assert(router.resolve(read)->provider.id.value == slow.value);
    router.setPreferredProviderForCapability(readId, CapabilityAccess::Read, {.provider = primary, .require = true});
    assert(!router.resolve(read)); // capability-specific pin must never spill over
    router.clearPreferredProviderForCapability(readId, CapabilityAccess::Read);
    health.update({.providerId = primary, .capabilityId = writeId, .state = health::HealthState::Unavailable});
    assert(!router.resolve(write)); // no writer failover even though peers support it
    health.update({.providerId = primary, .capabilityId = readId, .state = health::HealthState::Healthy});
    assert(router.resolve(read)->provider.id.value == primary.value);
}
