module;

#include <utility>
#include <vector>

module farcaster.provider.hypersnap.registration;

import farcaster.capability.catalog;
import farcaster.capability.model;

namespace farcaster::provider::hypersnap::registration {
namespace {

[[nodiscard]] capability::model::CapabilityDescriptor protocolRead(
    capability::model::CapabilityId id)
{
    return capability::model::CapabilityDescriptor{
        .id = std::move(id),
        .version = {1, 0},
        .origin = capability::model::CapabilityOrigin::Protocol,
        .quality = capability::model::CapabilityQuality::Native,
        .access = {capability::model::CapabilityAccess::Read},
        .isSelfHostable = true,
    };
}

[[nodiscard]] capability::model::CapabilityDescriptor derivedRead(
    capability::model::CapabilityId id)
{
    return capability::model::CapabilityDescriptor{
        .id = std::move(id),
        .version = {1, 0},
        .origin = capability::model::CapabilityOrigin::Derived,
        .quality = capability::model::CapabilityQuality::Native,
        .access = {capability::model::CapabilityAccess::Read},
        .isSelfHostable = true,
    };
}

}

std::vector<capability::model::CapabilityDescriptor> capabilities()
{
    return {
        protocolRead(capability::catalog::identityFidResolve()),
        protocolRead(capability::catalog::profileRead()),
        protocolRead(capability::catalog::castRead()),
        protocolRead(capability::catalog::graphFollowersRead()),
        protocolRead(capability::catalog::graphFollowingRead()),
        derivedRead(capability::catalog::feedFollowingRead()),
        derivedRead(capability::catalog::feedTrendingRead()),
        derivedRead(capability::catalog::feedChannelRead()),
        derivedRead(capability::catalog::feedUserPostsRead()),
        derivedRead(capability::catalog::feedUserQuotesRead()),
        derivedRead(capability::catalog::feedUserRepliesRead()),
        derivedRead(capability::catalog::feedUserLikesRead()),
        derivedRead(capability::catalog::feedUserPopularRead()),
    };
}

}
