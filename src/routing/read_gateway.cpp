module;

#include <chrono>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.routing.read.gateway;

import farcaster.application.read.gateway;
import farcaster.application.read.model;
import farcaster.capability.catalog;
import farcaster.capability.cast;
import farcaster.capability.feed;
import farcaster.capability.identity;
import farcaster.capability.model;
import farcaster.capability.pagination;
import farcaster.capability.profile;
import farcaster.capability.search;
import farcaster.capability.notification;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.model;
import farcaster.routing.health;
import farcaster.routing.policy;
import farcaster.routing.router;

namespace farcaster::routing::read::gateway {
namespace {

[[nodiscard]] policy::CapabilityRequest requestFor(
    capability::model::CapabilityId id,
    const application::read::model::ReadOptions options)
{
    return policy::CapabilityRequest{
        .capabilityId = std::move(id),
        .requiredAccess = capability::model::CapabilityAccess::Read,
        .minimumQuality = capability::model::CapabilityQuality::Experimental,
        .allowRecoveryProbe = options.allowRecoveryProbe,
    };
}

[[nodiscard]] std::string_view errorName(const foundation::error::ErrorCode code) noexcept
{
    using foundation::error::ErrorCode;
    switch (code) {
    case ErrorCode::InvalidArgument: return "InvalidArgument";
    case ErrorCode::NotFound: return "NotFound";
    case ErrorCode::Unavailable: return "Unavailable";
    case ErrorCode::Unauthorized: return "Unauthorized";
    case ErrorCode::RateLimited: return "RateLimited";
    case ErrorCode::Timeout: return "Timeout";
    case ErrorCode::Conflict: return "Conflict";
    case ErrorCode::Unsupported: return "Unsupported";
    case ErrorCode::StorageFailure: return "StorageFailure";
    case ErrorCode::TransportFailure: return "TransportFailure";
    case ErrorCode::ProtocolFailure: return "ProtocolFailure";
    case ErrorCode::InternalFailure: return "InternalFailure";
    }
    return "InternalFailure";
}

template <typename Operation>
[[nodiscard]] auto executeObserved(
    health::HealthRegistry& healthRegistry,
    const router::ProviderRoute& route,
    const capability::model::CapabilityId& capabilityId,
    const application::read::model::ReadOptions options,
    Operation operation) -> decltype(operation())
{
    const auto mode = options.allowRecoveryProbe
        ? health::AttemptMode::ManualRecoveryProbe : health::AttemptMode::Normal;
    auto admission = healthRegistry.beginAttempt(route.provider.id, capabilityId, mode);
    if (!admission) {
        return std::unexpected(std::move(admission.error()));
    }

    const auto startedAt = healthRegistry.now();
    auto result = operation();
    const auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
        healthRegistry.now() - startedAt);
    if (result) {
        healthRegistry.recordSuccess(route.provider.id, capabilityId, latency);
        return result;
    }

    foundation::error::Error error = std::move(result.error());
    if (health::isRetryableFailure(error.code)) {
        healthRegistry.recordFailure(route.provider.id, capabilityId, error.code, latency);
    }

    std::string diagnostic = "Capability: " + capabilityId.value;
    diagnostic.append("\nProvider: ");
    diagnostic.append(route.provider.id.value);
    diagnostic.append("\nLatency: ");
    diagnostic.append(std::to_string(latency.count()));
    diagnostic.append("ms\nResult: ");
    diagnostic.append(errorName(error.code));
    diagnostic.append("\n");
    diagnostic.append(error.message);
    error.message = std::move(diagnostic);
    return std::unexpected(std::move(error));
}

[[nodiscard]] foundation::error::Error missingSource(const std::string_view operation)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::Unavailable,
        "Selected route has no bound " + std::string{operation} + " source.");
}

}

RoutedReadGateway::RoutedReadGateway(
    const router::CapabilityRouter& router,
    health::HealthRegistry& health,
    std::vector<SourceBinding> bindings) noexcept
    : m_router(router), m_health(health), m_bindings(std::move(bindings))
{
}

foundation::result::Result<const SourceBinding*> RoutedReadGateway::bindingFor(
    const provider::model::ProviderId& providerId) const
{
    for (const auto& binding : m_bindings) {
        if (binding.providerId.value == providerId.value) {
            return &binding;
        }
    }
    return std::unexpected(foundation::error::Error::make(
        foundation::error::ErrorCode::Unavailable,
        "Selected provider route has no runtime source binding."));
}

foundation::result::Result<domain::identity::Fid> RoutedReadGateway::resolveIdentity(
    const std::string_view username,
    const application::read::model::ReadOptions options)
{
    const auto capabilityId = capability::catalog::identityFidResolve();
    auto route = m_router.resolve(requestFor(capabilityId, options));
    if (!route) {
        return std::unexpected(std::move(route.error()));
    }
    auto binding = bindingFor(route->provider.id);
    if (!binding) {
        return std::unexpected(std::move(binding.error()));
    }
    if ((*binding)->identity == nullptr) {
        return std::unexpected(missingSource("identity"));
    }
    return executeObserved(m_health, *route, capabilityId, options,
        [&] { return (*binding)->identity->resolveUsername(username); });
}

foundation::result::Result<domain::profile::Profile> RoutedReadGateway::profile(
    const domain::identity::Fid fid,
    const application::read::model::ReadOptions options)
{
    const auto capabilityId = capability::catalog::profileRead();
    auto route = m_router.resolve(requestFor(capabilityId, options));
    if (!route) {
        return std::unexpected(std::move(route.error()));
    }
    auto binding = bindingFor(route->provider.id);
    if (!binding) {
        return std::unexpected(std::move(binding.error()));
    }
    if ((*binding)->profile == nullptr) {
        return std::unexpected(missingSource("profile"));
    }
    return executeObserved(m_health, *route, capabilityId, options,
        [&] { return (*binding)->profile->get(fid); });
}

foundation::result::Result<domain::cast::Cast> RoutedReadGateway::cast(
    const domain::cast::CastId& castId,
    const application::read::model::ReadOptions options)
{
    const auto capabilityId = capability::catalog::castRead();
    auto route = m_router.resolve(requestFor(capabilityId, options));
    if (!route) {
        return std::unexpected(std::move(route.error()));
    }
    auto binding = bindingFor(route->provider.id);
    if (!binding) {
        return std::unexpected(std::move(binding.error()));
    }
    if ((*binding)->cast == nullptr) {
        return std::unexpected(missingSource("cast"));
    }
    return executeObserved(m_health, *route, capabilityId, options,
        [&] { return (*binding)->cast->get(castId); });
}

foundation::result::Result<capability::feed::FeedPage> RoutedReadGateway::feed(
    const application::read::model::FeedQuery& query,
    const application::read::model::ReadOptions options)
{
    capability::model::CapabilityId capabilityId = capability::catalog::feedTrendingRead();
    if (query.kind == application::read::model::FeedQuery::Kind::Following) {
        capabilityId = capability::catalog::feedFollowingRead();
    } else if (query.kind == application::read::model::FeedQuery::Kind::Channel) {
        capabilityId = capability::catalog::feedChannelRead();
    } else if (query.kind == application::read::model::FeedQuery::Kind::UserPosts) {
        capabilityId = capability::catalog::feedUserPostsRead();
    } else if (query.kind == application::read::model::FeedQuery::Kind::UserQuotes) {
        capabilityId = capability::catalog::feedUserQuotesRead();
    } else if (query.kind == application::read::model::FeedQuery::Kind::UserReplies) {
        capabilityId = capability::catalog::feedUserRepliesRead();
    } else if (query.kind == application::read::model::FeedQuery::Kind::UserLikes) {
        capabilityId = capability::catalog::feedUserLikesRead();
    } else if (query.kind == application::read::model::FeedQuery::Kind::UserPopular) {
        capabilityId = capability::catalog::feedUserPopularRead();
    }
    auto route = m_router.resolve(requestFor(capabilityId, options));
    if (!route) {
        return std::unexpected(std::move(route.error()));
    }
    auto binding = bindingFor(route->provider.id);
    if (!binding) {
        return std::unexpected(std::move(binding.error()));
    }
    if ((*binding)->feed == nullptr) {
        return std::unexpected(missingSource("feed"));
    }
    const capability::pagination::PageRequest pageRequest{
        .limit = query.limit,
        .continuation = query.continuation,
    };
    if (query.kind == application::read::model::FeedQuery::Kind::Following) {
        return executeObserved(m_health, *route, capabilityId, options,
            [&] { return (*binding)->feed->following(*query.fid, pageRequest); });
    }
    if (query.kind == application::read::model::FeedQuery::Kind::Channel) {
        return executeObserved(m_health, *route, capabilityId, options,
            [&] { return (*binding)->feed->channel(*query.channel, pageRequest); });
    }
    if (query.kind == application::read::model::FeedQuery::Kind::UserPosts) {
        return executeObserved(m_health, *route, capabilityId, options,
            [&] { return (*binding)->feed->userPosts(*query.fid, pageRequest); });
    }
    if (query.kind == application::read::model::FeedQuery::Kind::UserQuotes) {
        return executeObserved(m_health, *route, capabilityId, options,
            [&] { return (*binding)->feed->userQuotes(*query.fid, pageRequest); });
    }
    if (query.kind == application::read::model::FeedQuery::Kind::UserReplies) {
        return executeObserved(m_health, *route, capabilityId, options,
            [&] { return (*binding)->feed->userReplies(*query.fid, pageRequest); });
    }
    if (query.kind == application::read::model::FeedQuery::Kind::UserLikes) {
        return executeObserved(m_health, *route, capabilityId, options,
            [&] { return (*binding)->feed->userLikes(*query.fid, pageRequest); });
    }
    if (query.kind == application::read::model::FeedQuery::Kind::UserPopular) {
        return executeObserved(m_health, *route, capabilityId, options,
            [&] { return (*binding)->feed->userPopular(*query.fid, pageRequest); });
    }
    return executeObserved(m_health, *route, capabilityId, options,
        [&] { return (*binding)->feed->trending(pageRequest); });
}

foundation::result::Result<capability::search::SearchPage> RoutedReadGateway::search(
    const application::read::model::SearchQuery& query,
    const application::read::model::ReadOptions options)
{
    const auto capabilityId = capability::catalog::searchRead();
    auto route = m_router.resolve(requestFor(capabilityId, options));
    if (!route) {
        return std::unexpected(std::move(route.error()));
    }
    auto binding = bindingFor(route->provider.id);
    if (!binding) {
        return std::unexpected(std::move(binding.error()));
    }
    if ((*binding)->search == nullptr) {
        return std::unexpected(missingSource("search"));
    }
    return executeObserved(m_health, *route, capabilityId, options,
        [&] { return (*binding)->search->search(query.text, query.limit); });
}

foundation::result::Result<capability::notification::NotificationPage> RoutedReadGateway::notifications(
    const application::read::model::NotificationQuery& query,
    const application::read::model::ReadOptions options)
{
    const auto capabilityId = capability::catalog::notificationsRead();
    auto route = m_router.resolve(requestFor(capabilityId, options));
    if (!route) {
        return std::unexpected(std::move(route.error()));
    }
    auto binding = bindingFor(route->provider.id);
    if (!binding) {
        return std::unexpected(std::move(binding.error()));
    }
    if ((*binding)->notifications == nullptr) {
        return std::unexpected(missingSource("notifications"));
    }
    const capability::pagination::PageRequest pageRequest{
        .limit = query.limit,
        .continuation = query.continuation,
    };
    return executeObserved(m_health, *route, capabilityId, options,
        [&] { return (*binding)->notifications->notifications(query.viewer, pageRequest); });
}

foundation::result::Result<capability::social::SocialGraphPage> RoutedReadGateway::social(
    const application::read::model::SocialQuery& query,
    const application::read::model::ReadOptions options)
{
    const auto capabilityId = query.kind == application::read::model::SocialQuery::Kind::Followers
        ? capability::catalog::graphFollowersRead()
        : capability::catalog::graphFollowingRead();
    auto route = m_router.resolve(requestFor(capabilityId, options));
    if (!route) {
        return std::unexpected(std::move(route.error()));
    }
    auto binding = bindingFor(route->provider.id);
    if (!binding) {
        return std::unexpected(std::move(binding.error()));
    }
    if ((*binding)->social == nullptr) {
        return std::unexpected(missingSource("social"));
    }
    const capability::pagination::PageRequest pageRequest{
        .limit = query.limit,
        .continuation = query.continuation,
    };
    if (query.kind == application::read::model::SocialQuery::Kind::Followers) {
        return executeObserved(m_health, *route, capabilityId, options,
            [&] { return (*binding)->social->followers(query.fid, pageRequest); });
    }
    return executeObserved(m_health, *route, capabilityId, options,
        [&] { return (*binding)->social->following(query.fid, pageRequest); });
}

}
