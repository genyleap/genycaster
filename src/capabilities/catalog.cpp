module;

#include <string>

module farcaster.capability.catalog;

namespace farcaster::capability::catalog {

model::CapabilityId identityFidResolve() { return model::CapabilityId{"identity.fid.resolve"}; }
model::CapabilityId castRead() { return model::CapabilityId{"cast.read"}; }
model::CapabilityId profileRead() { return model::CapabilityId{"profile.read"}; }
model::CapabilityId graphFollowersRead() { return model::CapabilityId{"graph.followers.read"}; }
model::CapabilityId graphFollowingRead() { return model::CapabilityId{"graph.following.read"}; }
model::CapabilityId feedFollowingRead() { return model::CapabilityId{"feed.following.read"}; }
model::CapabilityId feedTrendingRead() { return model::CapabilityId{"feed.trending.read"}; }
model::CapabilityId feedChannelRead() { return model::CapabilityId{"feed.channel.read"}; }
model::CapabilityId feedForYouRead() { return model::CapabilityId{"feed.for_you.read"}; }
model::CapabilityId miniAppManifestResolve() { return model::CapabilityId{"miniapp.manifest.resolve"}; }
model::CapabilityId miniAppDiscoveryRead() { return model::CapabilityId{"miniapp.discovery.read"}; }
model::CapabilityId legacyMessagingRead() { return model::CapabilityId{"messaging.legacy.read"}; }
model::CapabilityId secureMessaging() { return model::CapabilityId{"messaging.secure"}; }

}
