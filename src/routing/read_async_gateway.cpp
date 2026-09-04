module;

#include <chrono>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.routing.read.async_gateway;

import farcaster.application.read.async_gateway;
import farcaster.application.read.model;
import farcaster.capability.catalog;
import farcaster.capability.model;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.model;
import farcaster.routing.health;
import farcaster.routing.policy;
import farcaster.routing.router;

namespace farcaster::routing::read::async_gateway {
namespace {

using AppRequest = application::read::async_gateway::Request;
using CapabilityId = capability::model::CapabilityId;
using Error = foundation::error::Error;
using ErrorCode = foundation::error::ErrorCode;

[[nodiscard]] policy::CapabilityRequest requestFor(
    CapabilityId id,
    const application::read::model::ReadOptions options)
{
    return policy::CapabilityRequest{
        .capabilityId = std::move(id),
        .requiredAccess = capability::model::CapabilityAccess::Read,
        .minimumQuality = capability::model::CapabilityQuality::Experimental,
        .allowRecoveryProbe = options.allowRecoveryProbe,
    };
}

[[nodiscard]] std::string_view errorName(const ErrorCode code) noexcept
{
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

[[nodiscard]] Error observedError(
    Error error,
    const provider::model::ProviderId& providerId,
    const CapabilityId& capabilityId,
    const std::chrono::milliseconds latency)
{
    std::string diagnostic = "Capability: " + capabilityId.value;
    diagnostic.append("\nProvider: ");
    diagnostic.append(providerId.value);
    diagnostic.append("\nLatency: ");
    diagnostic.append(std::to_string(latency.count()));
    diagnostic.append("ms\nResult: ");
    diagnostic.append(errorName(error.code));
    diagnostic.append("\n");
    diagnostic.append(error.message);
    error.message = std::move(diagnostic);
    return error;
}

[[nodiscard]] CapabilityId feedCapabilityId(
    const application::read::model::FeedQuery::Kind kind)
{
    using Kind = application::read::model::FeedQuery::Kind;
    switch (kind) {
    case Kind::Following: return capability::catalog::feedFollowingRead();
    case Kind::Trending: return capability::catalog::feedTrendingRead();
    case Kind::Channel: return capability::catalog::feedChannelRead();
    case Kind::UserPosts: return capability::catalog::feedUserPostsRead();
    case Kind::UserQuotes: return capability::catalog::feedUserQuotesRead();
    case Kind::UserReplies: return capability::catalog::feedUserRepliesRead();
    case Kind::UserLikes: return capability::catalog::feedUserLikesRead();
    case Kind::UserPopular: return capability::catalog::feedUserPopularRead();
    }
    return capability::catalog::feedTrendingRead();
}

template <typename Completion, typename Starter>
[[nodiscard]] foundation::result::Result<AppRequest> startObserved(
    health::HealthRegistry& healthRegistry,
    const router::ProviderRoute& route,
    const CapabilityId& capabilityId,
    const application::read::model::ReadOptions options,
    Completion completion,
    Starter starter)
{
    const auto mode = options.allowRecoveryProbe
        ? health::AttemptMode::ManualRecoveryProbe
        : health::AttemptMode::Normal;
    auto admission = healthRegistry.beginAttempt(route.provider.id, capabilityId, mode);
    if (!admission) {
        return std::unexpected(std::move(admission.error()));
    }

    const auto providerId = route.provider.id;
    const auto startedAt = healthRegistry.now();
    auto observedCompletion = [
        health = &healthRegistry,
        providerId,
        capabilityId,
        startedAt,
        completion = std::move(completion)](auto result) mutable {
        const auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            health->now() - startedAt);
        if (result) {
            health->recordSuccess(providerId, capabilityId, latency);
            completion(std::move(result));
            return;
        }

        Error error = std::move(result.error());
        if (health::isRetryableFailure(error.code)) {
            health->recordFailure(providerId, capabilityId, error.code, latency);
        }
        completion(std::unexpected(observedError(
            std::move(error), providerId, capabilityId, latency)));
    };

    auto request = starter(std::move(observedCompletion));
    if (request) {
        return request;
    }

    const auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
        healthRegistry.now() - startedAt);
    Error error = std::move(request.error());
    if (health::isRetryableFailure(error.code)) {
        healthRegistry.recordFailure(providerId, capabilityId, error.code, latency);
    }
    return std::unexpected(observedError(
        std::move(error), providerId, capabilityId, latency));
}

}

RoutedReadGateway::RoutedReadGateway(
    const router::CapabilityRouter& router,
    health::HealthRegistry& health,
    std::vector<SourceBinding> bindings) noexcept
    : m_router(router),
      m_health(health),
      m_bindings(std::move(bindings))
{
}

foundation::result::Result<const SourceBinding*> RoutedReadGateway::bindingFor(
    const provider::model::ProviderId& providerId) const
{
    for (const auto& binding : m_bindings) {
        if (binding.providerId.value == providerId.value) {
            if (binding.gateway == nullptr) {
                return std::unexpected(Error::make(
                    ErrorCode::Unavailable,
                    "Selected provider route has no asynchronous gateway instance."));
            }
            return &binding;
        }
    }
    return std::unexpected(Error::make(
        ErrorCode::Unavailable,
        "Selected provider route has no asynchronous runtime source binding."));
}

foundation::result::Result<AppRequest> RoutedReadGateway::resolveIdentity(
    const std::string_view username,
    application::read::async_gateway::IdentityCompletion completion,
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
    return startObserved(m_health, *route, capabilityId, options, std::move(completion),
        [gateway = (*binding)->gateway, username, options](auto observed) {
            return gateway->resolveIdentity(username, std::move(observed), options);
        });
}

foundation::result::Result<AppRequest> RoutedReadGateway::profile(
    const domain::identity::Fid fid,
    application::read::async_gateway::ProfileCompletion completion,
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
    return startObserved(m_health, *route, capabilityId, options, std::move(completion),
        [gateway = (*binding)->gateway, fid, options](auto observed) {
            return gateway->profile(fid, std::move(observed), options);
        });
}

foundation::result::Result<AppRequest> RoutedReadGateway::cast(
    domain::cast::CastId castId,
    application::read::async_gateway::CastCompletion completion,
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
    return startObserved(m_health, *route, capabilityId, options, std::move(completion),
        [gateway = (*binding)->gateway, castId = std::move(castId), options](auto observed) mutable {
            return gateway->cast(std::move(castId), std::move(observed), options);
        });
}

foundation::result::Result<AppRequest> RoutedReadGateway::feed(
    application::read::model::FeedQuery query,
    application::read::async_gateway::FeedCompletion completion,
    const application::read::model::ReadOptions options)
{
    const auto capabilityId = feedCapabilityId(query.kind);
    auto route = m_router.resolve(requestFor(capabilityId, options));
    if (!route) {
        return std::unexpected(std::move(route.error()));
    }
    auto binding = bindingFor(route->provider.id);
    if (!binding) {
        return std::unexpected(std::move(binding.error()));
    }
    return startObserved(m_health, *route, capabilityId, options, std::move(completion),
        [gateway = (*binding)->gateway, query = std::move(query), options](auto observed) mutable {
            return gateway->feed(std::move(query), std::move(observed), options);
        });
}

foundation::result::Result<AppRequest> RoutedReadGateway::search(
    application::read::model::SearchQuery query,
    application::read::async_gateway::SearchCompletion completion,
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
    return startObserved(m_health, *route, capabilityId, options, std::move(completion),
        [gateway = (*binding)->gateway, query = std::move(query), options](auto observed) mutable {
            return gateway->search(std::move(query), std::move(observed), options);
        });
}

foundation::result::Result<AppRequest> RoutedReadGateway::notifications(
    application::read::model::NotificationQuery query,
    application::read::async_gateway::NotificationCompletion completion,
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
    return startObserved(m_health, *route, capabilityId, options, std::move(completion),
        [gateway = (*binding)->gateway, query = std::move(query), options](auto observed) mutable {
            return gateway->notifications(std::move(query), std::move(observed), options);
        });
}

foundation::result::Result<AppRequest> RoutedReadGateway::social(
    application::read::model::SocialQuery query,
    application::read::async_gateway::SocialCompletion completion,
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
    return startObserved(m_health, *route, capabilityId, options, std::move(completion),
        [gateway = (*binding)->gateway, query = std::move(query), options](auto observed) mutable {
            return gateway->social(std::move(query), std::move(observed), options);
        });
}

}
