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
model::CapabilityId feedRecentRead() { return model::CapabilityId{"feed.recent.read"}; }
model::CapabilityId feedConversationsRead() { return model::CapabilityId{"feed.conversations.read"}; }
model::CapabilityId feedNftsRead() { return model::CapabilityId{"feed.nfts.read"}; }
model::CapabilityId feedDevelopersRead() { return model::CapabilityId{"feed.developers.read"}; }
model::CapabilityId feedChannelRead() { return model::CapabilityId{"feed.channel.read"}; }
model::CapabilityId feedUserPostsRead() { return model::CapabilityId{"feed.user.posts.read"}; }
model::CapabilityId feedUserQuotesRead() { return model::CapabilityId{"feed.user.quotes.read"}; }
model::CapabilityId feedUserRepliesRead() { return model::CapabilityId{"feed.user.replies.read"}; }
model::CapabilityId feedUserLikesRead() { return model::CapabilityId{"feed.user.likes.read"}; }
model::CapabilityId feedUserPopularRead() { return model::CapabilityId{"feed.user.popular.read"}; }
model::CapabilityId feedForYouRead() { return model::CapabilityId{"feed.for_you.read"}; }
model::CapabilityId searchRead() { return model::CapabilityId{"search.read"}; }
model::CapabilityId notificationsRead() { return model::CapabilityId{"notifications.read"}; }
model::CapabilityId valuationAssetPriceRead() { return model::CapabilityId{"valuation.asset_price.read"}; }
model::CapabilityId valuationFxRead() { return model::CapabilityId{"valuation.fx.read"}; }
model::CapabilityId miniAppManifestResolve() { return model::CapabilityId{"miniapp.manifest.resolve"}; }
model::CapabilityId miniAppDiscoveryRead() { return model::CapabilityId{"miniapp.discovery.read"}; }
model::CapabilityId legacyMessagingRead() { return model::CapabilityId{"messaging.legacy.read"}; }
model::CapabilityId secureMessaging() { return model::CapabilityId{"messaging.secure"}; }

}
