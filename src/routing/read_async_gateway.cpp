module;

#include <atomic>
#include <chrono>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
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

struct ObservedAttempt final {
    health::HealthRegistry* health{};
    provider::model::ProviderId providerId{};
    CapabilityId capabilityId{};
    health::AttemptAdmission admission{};
    health::MonotonicTimePoint startedAt{};
    std::atomic_bool cancelled{false};
    std::atomic_bool finalized{false};

    [[nodiscard]] bool beginCompletion() noexcept
    {
        if (cancelled.load(std::memory_order_acquire)) {
            return false;
        }
        return !finalized.exchange(true, std::memory_order_acq_rel);
    }

    void cancel()
    {
        cancelled.store(true, std::memory_order_release);
        if (finalized.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        health->abandonAttempt(providerId, capabilityId, admission);
    }
};

[[nodiscard]] policy::CapabilityRequest requestFor(
    CapabilityId id,
    const application::read::model::ReadOptions options)
{
    return policy::CapabilityRequest{
        .capabilityId = std::move(id),
        .requiredAccess = capability::model::CapabilityAccess::Read,
        .minimumQuality = capability::model::CapabilityQuality::Experimental,
        .allowRecoveryProbe = options.allowRecoveryProbe
            && options.intent == application::read::model::ReadIntent::UserVisible,
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
    case Kind::Recent: return capability::catalog::feedRecentRead();
    case Kind::Conversations: return capability::catalog::feedConversationsRead();
    case Kind::Nfts: return capability::catalog::feedNftsRead();
    case Kind::Developers: return capability::catalog::feedDevelopersRead();
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
    if (options.intent == application::read::model::ReadIntent::Prefetch) {
        const auto providerId = route.provider.id;
        const auto startedAt = healthRegistry.now();
        auto cancelled = std::make_shared<std::atomic_bool>(false);
        auto prefetchCompletion = [
            cancelled,
            &healthRegistry,
            providerId,
            capabilityId,
            startedAt,
            completion = std::move(completion)](auto result) mutable {
            if (cancelled->load(std::memory_order_acquire)) {
                return;
            }
            if (result) {
                completion(std::move(result));
                return;
            }
            const auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                healthRegistry.now() - startedAt);
            completion(std::unexpected(observedError(
                std::move(result.error()), providerId, capabilityId, latency)));
        };
        auto request = starter(std::move(prefetchCompletion));
        if (!request) {
            const auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                healthRegistry.now() - startedAt);
            return std::unexpected(observedError(
                std::move(request.error()), providerId, capabilityId, latency));
        }
        AppRequest sourceRequest = std::move(*request);
        return AppRequest{[cancelled, sourceRequest = std::move(sourceRequest)]() mutable {
            cancelled->store(true, std::memory_order_release);
            sourceRequest.cancel();
        }};
    }

    const auto mode = options.allowRecoveryProbe
        ? health::AttemptMode::ManualRecoveryProbe
        : health::AttemptMode::Normal;
    auto admission = healthRegistry.beginAttempt(route.provider.id, capabilityId, mode);
    if (!admission) {
        return std::unexpected(std::move(admission.error()));
    }

    const auto providerId = route.provider.id;
    const auto startedAt = healthRegistry.now();
    auto attempt = std::make_shared<ObservedAttempt>();
    attempt->health = &healthRegistry;
    attempt->providerId = providerId;
    attempt->capabilityId = capabilityId;
    attempt->admission = *admission;
    attempt->startedAt = startedAt;
    auto observedCompletion = [
        attempt,
        completion = std::move(completion)](auto result) mutable {
        if (!attempt->beginCompletion()) {
            return;
        }
        const auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            attempt->health->now() - attempt->startedAt);
        if (result) {
            attempt->health->recordSuccess(
                attempt->providerId, attempt->capabilityId, latency, attempt->admission);
            completion(std::move(result));
            return;
        }

        Error error = std::move(result.error());
        if (health::isRetryableFailure(error.code)) {
            attempt->health->recordFailure(
                attempt->providerId, attempt->capabilityId, error.code, latency,
                attempt->admission);
        } else {
            attempt->health->abandonAttempt(
                attempt->providerId, attempt->capabilityId, attempt->admission);
        }
        completion(std::unexpected(observedError(
            std::move(error), attempt->providerId, attempt->capabilityId, latency)));
    };

    auto request = starter(std::move(observedCompletion));
    if (request) {
        AppRequest sourceRequest = std::move(*request);
        return AppRequest{[attempt, sourceRequest = std::move(sourceRequest)]() mutable {
            // Claim cancellation before touching the source handle so an
            // immediate transport callback cannot race in and mutate health.
            attempt->cancel();
            sourceRequest.cancel();
        }};
    }

    const bool alreadyFinalized = attempt->finalized.exchange(
        true, std::memory_order_acq_rel);
    const auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
        healthRegistry.now() - startedAt);
    Error error = std::move(request.error());
    if (!alreadyFinalized) {
        if (health::isRetryableFailure(error.code)) {
            healthRegistry.recordFailure(
                providerId, capabilityId, error.code, latency, *admission);
        } else {
            healthRegistry.abandonAttempt(providerId, capabilityId, *admission);
        }
    }
    return std::unexpected(observedError(
        std::move(error), providerId, capabilityId, latency));
}

}

RoutedReadGateway::RoutedReadGateway(
    const router::CapabilityRouter& router,
    health::HealthRegistry& health,
    std::vector<SourceBinding> bindings,
    const bool allowCompatibleProfileEnrichment) noexcept
    : m_router(router),
      m_health(health),
      m_bindings(std::move(bindings)),
      m_allowCompatibleProfileEnrichment(allowCompatibleProfileEnrichment)
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
    // Enrich *only* absent metadata, and only after explicit user consent.
    // The selected provider remains authoritative for identity, biography
    // and cast content. Never send a FID to another provider in strict mode.
    application::read::async_gateway::IAsyncReadGateway* supplemental = nullptr;
    if (m_allowCompatibleProfileEnrichment && !(*binding)->providesProfileCounts) {
        for (const auto& candidate : m_bindings) {
            if (candidate.providesProfileCounts && candidate.gateway
                && candidate.providerId.value != route->provider.id.value
                && m_router.isEligibleProvider(candidate.providerId,
                                               requestFor(capabilityId, options))) {
                supplemental = candidate.gateway;
                break;
            }
        }
    }
    if (!supplemental) {
        return startObserved(m_health, *route, capabilityId, options, std::move(completion),
            [gateway = (*binding)->gateway, fid, options](auto observed) {
                return gateway->profile(fid, std::move(observed), options);
            });
    }
    struct Enrichment final {
        std::mutex mutex{};
        std::atomic_bool cancelled{false};
        std::atomic_bool delivered{false};
        std::optional<AppRequest> primary{};
        std::optional<AppRequest> secondary{};
        void cancel() {
            cancelled.store(true, std::memory_order_release);
            std::scoped_lock lock{mutex};
            if (primary) primary->cancel();
            if (secondary) secondary->cancel();
        }
    };
    auto state = std::make_shared<Enrichment>();
    auto finish = std::make_shared<application::read::async_gateway::ProfileCompletion>(
        std::move(completion));
    const auto deliver = [state, finish](
        foundation::result::Result<domain::profile::Profile> result) {
        if (!state->cancelled.load(std::memory_order_acquire)
            && !state->delivered.exchange(true, std::memory_order_acq_rel))
            (*finish)(std::move(result));
    };
    auto started = startObserved(m_health, *route, capabilityId, options,
        [state, deliver, supplemental, fid, options](
            foundation::result::Result<domain::profile::Profile> raw) mutable {
            if (state->cancelled.load(std::memory_order_acquire)) return;
            if (!raw || raw->socialCountsKnown)
                return deliver(std::move(raw));
            auto base = std::make_shared<domain::profile::Profile>(std::move(*raw));
            auto fallback = supplemental->profile(fid,
                [state, deliver, base, fid](
                    foundation::result::Result<domain::profile::Profile> indexed) mutable {
                    if (state->cancelled.load(std::memory_order_acquire)) return;
                    if (indexed && indexed->fid.value == fid.value
                        && indexed->socialCountsKnown) {
                        base->followerCount = indexed->followerCount;
                        base->followingCount = indexed->followingCount;
                        base->socialCountsKnown = true;
                    }
                    deliver(std::move(*base));
                }, options);
            if (!fallback) return deliver(std::move(*base));
            std::scoped_lock lock{state->mutex};
            if (state->cancelled.load(std::memory_order_acquire))
                fallback->cancel();
            else state->secondary = std::move(*fallback);
        },
        [gateway = (*binding)->gateway, fid, options](auto observed) {
            return gateway->profile(fid, std::move(observed), options);
        });
    if (!started) return started;
    {
        std::scoped_lock lock{state->mutex};
        state->primary = std::move(*started);
    }
    return AppRequest{[state] { state->cancel(); }};
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
    auto request = requestFor(capabilityId, options);
    // Raw Following derives a complete timeline by scanning every followee
    // and intentionally rejects graphs beyond its bounded fan-out. When the
    // operator/user has authorized compatible read fallback, prefer a native
    // indexed Following capability instead of retrying a bounded raw scan.
    // A strict provider selection is never bypassed: resolve() still enforces
    // explicit affinity, deployment and provider health constraints.
    if (query.kind == application::read::model::FeedQuery::Kind::Following
        && m_allowCompatibleProfileEnrichment) {
        request.minimumQuality = capability::model::CapabilityQuality::Native;
    }
    auto route = m_router.resolve(request);
    if (!route && request.minimumQuality == capability::model::CapabilityQuality::Native) {
        // Keep raw-only deployments functional, including their continuations.
        // The selected provider still validates ownership of every cursor.
        route = m_router.resolve(requestFor(capabilityId, options));
    }
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
