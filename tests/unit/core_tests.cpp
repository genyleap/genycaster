#include <cassert>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import farcaster.application.configuration;
import farcaster.application.read.async_gateway;
import farcaster.application.read.model;
import farcaster.capability.catalog;
import farcaster.capability.feed;
import farcaster.capability.model;
import farcaster.capability.notification;
import farcaster.capability.pagination;
import farcaster.capability.search;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.channel;
import farcaster.domain.feed;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.domain.valuation;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.routing.health;
import farcaster.routing.policy;
import farcaster.provider.model;
import farcaster.routing.registry;
import farcaster.routing.read.async_gateway;
import farcaster.routing.read.gateway;
import farcaster.routing.router;
import farcaster.sync.coordinator;
import farcaster.sync.model;

namespace {

[[nodiscard]] bool expectedBoolean(const char* const name)
{
    const char* const rawValue = std::getenv(name);
    assert(rawValue != nullptr);
    const std::string_view value{rawValue};
    return value == "ON" || value == "1" || value == "TRUE";
}

[[nodiscard]] std::string_view expectedString(const char* const name)
{
    const char* const rawValue = std::getenv(name);
    assert(rawValue != nullptr);
    return rawValue;
}

[[nodiscard]] std::uint64_t expectedUnsigned(const char* const name)
{
    const auto text = expectedString(name);
    std::uint64_t value{};
    const auto* const begin = text.data();
    const auto* const end = begin + text.size();
    const auto [position, error] = std::from_chars(begin, end, value);
    assert(error == std::errc{});
    assert(position == end);
    return value;
}


[[nodiscard]] farcaster::capability::model::CapabilityDescriptor feedCapability(
    const farcaster::capability::model::CapabilityQuality quality,
    const bool isSelfHostable)
{
    using namespace farcaster;
    return capability::model::CapabilityDescriptor{
        .id = capability::catalog::feedFollowingRead(),
        .version = capability::model::CapabilityVersion{1, 0},
        .origin = capability::model::CapabilityOrigin::Protocol,
        .quality = quality,
        .access = {capability::model::CapabilityAccess::Read},
        .isSelfHostable = isSelfHostable,
    };
}

[[nodiscard]] farcaster::capability::model::CapabilityDescriptor feedCapabilityFor(
    farcaster::capability::model::CapabilityId id,
    const farcaster::capability::model::CapabilityOrigin origin)
{
    using namespace farcaster;
    return capability::model::CapabilityDescriptor{
        .id = std::move(id),
        .version = capability::model::CapabilityVersion{1, 0},
        .origin = origin,
        .quality = capability::model::CapabilityQuality::Native,
        .access = {capability::model::CapabilityAccess::Read},
        .isSelfHostable = true,
    };
}

void testValuationContract()
{
    using namespace farcaster;
    const domain::valuation::AssetPriceQuote quote{
        .asset = domain::valuation::AssetId{"GENY"},
        .quoteCurrency = domain::valuation::FiatCurrency{"USD"},
        .pricePerAsset = domain::valuation::DecimalValue{"0.00060"},
        .observedAt = foundation::time::Timestamp{},
        .source = "test",
    };
    assert(quote.asset.symbol == "GENY");
    assert(quote.quoteCurrency.code == "USD");
    assert(quote.pricePerAsset.text == "0.00060");
    assert(capability::catalog::valuationAssetPriceRead().value == "valuation.asset_price.read");
    assert(capability::catalog::valuationFxRead().value == "valuation.fx.read");
}

void testConfiguredFeatureSet()
{
    using namespace farcaster;
    const auto features = application::configuration::configuredFeatures();
    assert(features.hasSocial == expectedBoolean("FARCASTER_EXPECT_SOCIAL"));
    assert(features.hasFeed == expectedBoolean("FARCASTER_EXPECT_FEED"));
    assert(features.hasChannels == expectedBoolean("FARCASTER_EXPECT_CHANNELS"));
    assert(features.hasMiniApps == expectedBoolean("FARCASTER_EXPECT_MINIAPPS"));
    assert(features.hasChat == expectedBoolean("FARCASTER_EXPECT_CHAT"));
    assert(features.hasSearch == expectedBoolean("FARCASTER_EXPECT_SEARCH"));
    assert(features.hasNotifications == expectedBoolean("FARCASTER_EXPECT_NOTIFICATIONS"));
    assert(features.hasWallet == expectedBoolean("FARCASTER_EXPECT_WALLET"));

    const auto deploymentProfile = application::configuration::configuredDeploymentProfile();
    const auto expectsSelfHosted = expectedBoolean("FARCASTER_EXPECT_SELF_HOSTED_ONLY");
    assert((deploymentProfile == application::configuration::DeploymentProfile::StrictSelfHosted)
        == expectsSelfHosted);

    const auto endpoints = application::configuration::configuredDeploymentEndpoints();
    assert(endpoints.siwfDomain == expectedString("FARCASTER_EXPECT_SIWF_DOMAIN"));
    assert(endpoints.siwfUri == expectedString("FARCASTER_EXPECT_SIWF_URI"));
    assert(endpoints.snapchainBaseUrl == expectedString("FARCASTER_EXPECT_SNAPCHAIN_BASE_URL"));
    assert(endpoints.mediaGatewayBaseUrl == expectedString("FARCASTER_EXPECT_MEDIA_GATEWAY_BASE_URL"));

    const auto identity = application::configuration::configuredDeploymentIdentity();
    assert(identity.appFid == expectedUnsigned("FARCASTER_EXPECT_APP_FID"));
    assert(identity.appName == expectedString("FARCASTER_EXPECT_APP_NAME"));
}

void testCapabilityRouting()
{
    using namespace farcaster;

    routing::registry::CapabilityRegistry registry{};
    const auto primaryStatus = registry.registerProvider(routing::registry::ProviderRegistration{
        .provider = provider::model::ProviderDescriptor{
            .id = provider::model::ProviderId{"primary"},
            .displayName = "Primary",
            .deployment = provider::model::ProviderDeployment::PublicManaged,
            .privacy = provider::model::ProviderPrivacy::Standard,
            .priority = 10,
        },
        .capabilities = {feedCapability(capability::model::CapabilityQuality::Native, false)},
    });
    assert(primaryStatus);

    const auto selfHostedStatus = registry.registerProvider(routing::registry::ProviderRegistration{
        .provider = provider::model::ProviderDescriptor{
            .id = provider::model::ProviderId{"self-hosted"},
            .displayName = "Self Hosted",
            .deployment = provider::model::ProviderDeployment::SelfHosted,
            .privacy = provider::model::ProviderPrivacy::Private,
            .priority = 20,
        },
        .capabilities = {feedCapability(capability::model::CapabilityQuality::Native, true)},
    });
    assert(selfHostedStatus);

    routing::health::HealthRegistry health{};
    routing::router::CapabilityRouter standardRouter{
        registry,
        health,
        routing::policy::RoutingPolicy{
            .mode = routing::policy::RoutingMode::Standard,
            .allowManagedProviders = true,
            .minimumQuality = capability::model::CapabilityQuality::Experimental,
        },
    };

    const routing::policy::CapabilityRequest request{
        .capabilityId = capability::catalog::feedFollowingRead(),
        .requiredAccess = capability::model::CapabilityAccess::Read,
        .minimumQuality = capability::model::CapabilityQuality::Partial,
    };

    auto firstRoute = standardRouter.resolve(request);
    assert(firstRoute);
    assert(firstRoute->provider.id.value == "primary");

    health.update(routing::health::CapabilityHealth{
        .providerId = provider::model::ProviderId{"primary"},
        .capabilityId = capability::catalog::feedFollowingRead(),
        .state = routing::health::HealthState::Unavailable,
        .latency = std::chrono::milliseconds{120},
        .errorRate = 1.0,
    });

    auto failedOverRoute = standardRouter.resolve(request);
    assert(failedOverRoute);
    assert(failedOverRoute->provider.id.value == "self-hosted");

    health.update(routing::health::CapabilityHealth{
        .providerId = provider::model::ProviderId{"primary"},
        .capabilityId = capability::catalog::feedFollowingRead(),
        .state = routing::health::HealthState::Healthy,
        .latency = std::chrono::milliseconds{15},
        .errorRate = 0.0,
    });

    const auto recoveredHealth = health.find(
        provider::model::ProviderId{"primary"},
        capability::catalog::feedFollowingRead());
    assert(recoveredHealth);
    assert(recoveredHealth->state == routing::health::HealthState::Healthy);

    auto recoveredRoute = standardRouter.resolve(request);
    assert(recoveredRoute);
    assert(recoveredRoute->provider.id.value == "primary");

    routing::router::CapabilityRouter strictRouter{
        registry,
        health,
        routing::policy::RoutingPolicy{
            .mode = routing::policy::RoutingMode::StrictSelfHosted,
            .allowManagedProviders = false,
            .minimumQuality = capability::model::CapabilityQuality::Experimental,
        },
    };
    auto strictRoute = strictRouter.resolve(request);
    assert(strictRoute);
    assert(strictRoute->provider.id.value == "self-hosted");

    // User affinity selects only eligible registered providers and must not
    // override deployment restrictions, runtime health or absence of an adapter.
    routing::registry::CapabilityRegistry networkRegistry{};
    const auto feed = feedCapability(capability::model::CapabilityQuality::Native, true);
    assert(networkRegistry.registerProvider(routing::registry::ProviderRegistration{
        .provider = provider::model::ProviderDescriptor{
            .id = provider::model::ProviderId{"test-independent"},
            .displayName = "Independent read adapter",
            .deployment = provider::model::ProviderDeployment::SelfHosted,
            .privacy = provider::model::ProviderPrivacy::Private,
            .priority = 1,
        },
        .capabilities = {feed},
    }));
    assert(networkRegistry.registerProvider(routing::registry::ProviderRegistration{
        .provider = provider::model::ProviderDescriptor{
            .id = provider::model::ProviderId{"test-hosted"},
            .displayName = "Farcaster",
            .deployment = provider::model::ProviderDeployment::PublicManaged,
            .privacy = provider::model::ProviderPrivacy::Managed,
            .priority = 2,
        },
        .capabilities = {feed},
    }));
    routing::health::HealthRegistry networkHealth{};
    routing::router::CapabilityRouter networkRouter{
        networkRegistry, networkHealth, routing::policy::RoutingPolicy{},
        routing::router::PreferredRouteProviders{
            .independent = provider::model::ProviderId{"test-independent"},
            .hosted = provider::model::ProviderId{"test-hosted"},
        },
    };
    const auto independentRoute = networkRouter.resolve(request);
    assert(independentRoute);
    assert(independentRoute->provider.id.value == "test-independent");
    networkRouter.setPreferredNetwork(routing::policy::PreferredNetwork::Hosted);
    const auto farcasterRoute = networkRouter.resolve(request);
    assert(farcasterRoute);
    assert(farcasterRoute->provider.id.value == "test-hosted");
    // No composition-provided identities means score-only routing; changing
    // the mode alone must not smuggle backend-name knowledge into the core.
    routing::router::CapabilityRouter unconfiguredAffinity{
        networkRegistry, networkHealth, routing::policy::RoutingPolicy{},
    };
    unconfiguredAffinity.setPreferredNetwork(routing::policy::PreferredNetwork::Hosted);
    const auto scoreOnlyRoute = unconfiguredAffinity.resolve(request);
    assert(scoreOnlyRoute);
    assert(scoreOnlyRoute->provider.id.value == "test-independent");
    networkHealth.update(routing::health::CapabilityHealth{
        .providerId = provider::model::ProviderId{"test-hosted"},
        .capabilityId = capability::catalog::feedFollowingRead(),
        .state = routing::health::HealthState::Unavailable,
    });
    const auto healthyFallback = networkRouter.resolve(request);
    assert(healthyFallback);
    assert(healthyFallback->provider.id.value == "test-independent");
    routing::router::CapabilityRouter strictNetworkRouter{
        networkRegistry, networkHealth,
        routing::policy::RoutingPolicy{
            .mode = routing::policy::RoutingMode::StrictSelfHosted,
            .allowManagedProviders = false,
        },
        routing::router::PreferredRouteProviders{
            .independent = provider::model::ProviderId{"test-independent"},
            .hosted = provider::model::ProviderId{"test-hosted"},
        },
    };
    strictNetworkRouter.setPreferredNetwork(routing::policy::PreferredNetwork::Hosted);
    const auto constrainedRoute = strictNetworkRouter.resolve(request);
    assert(constrainedRoute);
    assert(constrainedRoute->provider.id.value == "test-independent");
}

void testRoutingFailureDiagnostics()
{
    using namespace farcaster;

    routing::registry::CapabilityRegistry emptyRegistry{};
    routing::health::HealthRegistry emptyHealth{};
    routing::router::CapabilityRouter emptyRouter{
        emptyRegistry,
        emptyHealth,
        routing::policy::RoutingPolicy{},
    };
    const routing::policy::CapabilityRequest request{
        .capabilityId = capability::catalog::feedTrendingRead(),
        .requiredAccess = capability::model::CapabilityAccess::Read,
        .minimumQuality = capability::model::CapabilityQuality::Experimental,
    };

    const auto missingRoute = emptyRouter.resolve(request);
    assert(!missingRoute);
    assert(missingRoute.error().message.find("No provider is registered") != std::string::npos);
    assert(missingRoute.error().message.find("feed.trending.read") != std::string::npos);

    routing::registry::CapabilityRegistry policyRegistry{};
    assert(policyRegistry.registerProvider(routing::registry::ProviderRegistration{
        .provider = provider::model::ProviderDescriptor{
            .id = provider::model::ProviderId{"managed-only"},
            .displayName = "Managed Only",
            .deployment = provider::model::ProviderDeployment::PublicManaged,
            .privacy = provider::model::ProviderPrivacy::Standard,
            .priority = 1,
        },
        .capabilities = {capability::model::CapabilityDescriptor{
            .id = capability::catalog::feedTrendingRead(),
            .version = capability::model::CapabilityVersion{1, 0},
            .origin = capability::model::CapabilityOrigin::Derived,
            .quality = capability::model::CapabilityQuality::Native,
            .access = {capability::model::CapabilityAccess::Read},
            .isSelfHostable = true,
        }},
    }));
    routing::router::CapabilityRouter strictRouter{
        policyRegistry,
        emptyHealth,
        routing::policy::RoutingPolicy{
            .mode = routing::policy::RoutingMode::StrictSelfHosted,
            .allowManagedProviders = false,
            .minimumQuality = capability::model::CapabilityQuality::Experimental,
        },
    };

    const auto ineligibleRoute = strictRouter.resolve(request);
    assert(!ineligibleRoute);
    assert(ineligibleRoute.error().message.find("none are eligible") != std::string::npos);
    assert(ineligibleRoute.error().message.find("feed.trending.read") != std::string::npos);
}

void testSyncCoordinator()
{
    using namespace farcaster;

    sync::coordinator::SyncCoordinator coordinator{};
    const auto generation = coordinator.begin();
    assert(generation);
    assert(*generation == 1);
    assert(coordinator.state().phase == sync::model::SyncPhase::Running);

    const auto duplicateBegin = coordinator.begin();
    assert(!duplicateBegin);
    assert(duplicateBegin.error().code == foundation::error::ErrorCode::Conflict);

    const auto staleSuccess = coordinator.succeed(99);
    assert(!staleSuccess);

    const auto success = coordinator.succeed(*generation);
    assert(success);
    assert(coordinator.state().phase == sync::model::SyncPhase::Idle);

    const auto secondGeneration = coordinator.begin();
    assert(secondGeneration);
    const auto failure = coordinator.fail(
        *secondGeneration,
        foundation::error::Error::make(foundation::error::ErrorCode::Unavailable, "offline"));
    assert(failure);
    assert(coordinator.state().phase == sync::model::SyncPhase::Failed);
    assert(coordinator.state().lastError.has_value());
}

class FakeFeedSource final : public farcaster::capability::feed::IFeedSource {
public:
    class Clock final : public farcaster::routing::health::IMonotonicClock {
    public:
        [[nodiscard]] farcaster::routing::health::MonotonicTimePoint now() const noexcept override
        {
            return current;
        }

        void advance(const std::chrono::milliseconds duration) noexcept
        {
            current += duration;
        }

        farcaster::routing::health::MonotonicTimePoint current{};
    };

    Clock* clock{};
    int followingCalls{};
    int trendingCalls{};
    int trendingFailuresRemaining{};
    std::size_t lastTrendingLimit{};

    [[nodiscard]] farcaster::foundation::result::Result<farcaster::capability::feed::FeedPage>
    following(farcaster::domain::identity::Fid,
        const farcaster::capability::pagination::PageRequest&) override
    {
        ++followingCalls;
        if (clock != nullptr) {
            clock->advance(std::chrono::milliseconds{5});
        }
        return farcaster::capability::feed::FeedPage{};
    }

    [[nodiscard]] farcaster::foundation::result::Result<farcaster::capability::feed::FeedPage>
    trending(const farcaster::capability::pagination::PageRequest& pageRequest) override
    {
        ++trendingCalls;
        lastTrendingLimit = pageRequest.limit;
        if (clock != nullptr) {
            clock->advance(std::chrono::milliseconds{12'000});
        }
        if (trendingFailuresRemaining > 0) {
            --trendingFailuresRemaining;
            return std::unexpected(farcaster::foundation::error::Error::make(
                farcaster::foundation::error::ErrorCode::Timeout,
                "Stage: WaitingForHeaders"));
        }
        return farcaster::capability::feed::FeedPage{};
    }

    [[nodiscard]] farcaster::foundation::result::Result<farcaster::capability::feed::FeedPage>
    channel(const farcaster::domain::channel::ChannelId&,
        const farcaster::capability::pagination::PageRequest&) override
    {
        return farcaster::capability::feed::FeedPage{};
    }

    [[nodiscard]] farcaster::foundation::result::Result<farcaster::capability::feed::FeedPage>
    userPosts(farcaster::domain::identity::Fid,
        const farcaster::capability::pagination::PageRequest&) override
    {
        return farcaster::capability::feed::FeedPage{};
    }

    [[nodiscard]] farcaster::foundation::result::Result<farcaster::capability::feed::FeedPage>
    userQuotes(farcaster::domain::identity::Fid,
        const farcaster::capability::pagination::PageRequest&) override
    {
        return farcaster::capability::feed::FeedPage{};
    }

    [[nodiscard]] farcaster::foundation::result::Result<farcaster::capability::feed::FeedPage>
    userReplies(farcaster::domain::identity::Fid,
        const farcaster::capability::pagination::PageRequest&) override
    {
        return farcaster::capability::feed::FeedPage{};
    }

    [[nodiscard]] farcaster::foundation::result::Result<farcaster::capability::feed::FeedPage>
    userLikes(farcaster::domain::identity::Fid,
        const farcaster::capability::pagination::PageRequest&) override
    {
        return farcaster::capability::feed::FeedPage{};
    }

    [[nodiscard]] farcaster::foundation::result::Result<farcaster::capability::feed::FeedPage>
    userPopular(farcaster::domain::identity::Fid,
        const farcaster::capability::pagination::PageRequest&) override
    {
        return farcaster::capability::feed::FeedPage{};
    }
};

class PendingAsyncReadGateway final
    : public farcaster::application::read::async_gateway::IAsyncReadGateway {
public:
    bool sourceCancelled{false};
    int feedCalls{0};
    farcaster::application::read::async_gateway::FeedCompletion pendingFeed{};

    [[nodiscard]] farcaster::foundation::result::Result<
        farcaster::application::read::async_gateway::Request>
    resolveIdentity(
        std::string_view,
        farcaster::application::read::async_gateway::IdentityCompletion,
        farcaster::application::read::model::ReadOptions) override
    {
        return unsupported();
    }

    [[nodiscard]] farcaster::foundation::result::Result<
        farcaster::application::read::async_gateway::Request>
    profile(
        farcaster::domain::identity::Fid,
        farcaster::application::read::async_gateway::ProfileCompletion,
        farcaster::application::read::model::ReadOptions) override
    {
        return unsupported();
    }

    [[nodiscard]] farcaster::foundation::result::Result<
        farcaster::application::read::async_gateway::Request>
    cast(
        farcaster::domain::cast::CastId,
        farcaster::application::read::async_gateway::CastCompletion,
        farcaster::application::read::model::ReadOptions) override
    {
        return unsupported();
    }

    [[nodiscard]] farcaster::foundation::result::Result<
        farcaster::application::read::async_gateway::Request>
    feed(
        farcaster::application::read::model::FeedQuery,
        farcaster::application::read::async_gateway::FeedCompletion completion,
        farcaster::application::read::model::ReadOptions) override
    {
        ++feedCalls;
        sourceCancelled = false;
        pendingFeed = std::move(completion);
        return farcaster::application::read::async_gateway::Request{
            [this] { sourceCancelled = true; }};
    }

    [[nodiscard]] farcaster::foundation::result::Result<
        farcaster::application::read::async_gateway::Request>
    search(
        farcaster::application::read::model::SearchQuery,
        farcaster::application::read::async_gateway::SearchCompletion,
        farcaster::application::read::model::ReadOptions) override
    {
        return unsupported();
    }

    [[nodiscard]] farcaster::foundation::result::Result<
        farcaster::application::read::async_gateway::Request>
    notifications(
        farcaster::application::read::model::NotificationQuery,
        farcaster::application::read::async_gateway::NotificationCompletion,
        farcaster::application::read::model::ReadOptions) override
    {
        return unsupported();
    }

    [[nodiscard]] farcaster::foundation::result::Result<
        farcaster::application::read::async_gateway::Request>
    social(
        farcaster::application::read::model::SocialQuery,
        farcaster::application::read::async_gateway::SocialCompletion,
        farcaster::application::read::model::ReadOptions) override
    {
        return unsupported();
    }

    void completePending(
        farcaster::foundation::result::Result<farcaster::capability::feed::FeedPage> result)
    {
        auto completion = std::move(pendingFeed);
        pendingFeed = {};
        assert(completion);
        completion(std::move(result));
    }

private:
    [[nodiscard]] static farcaster::foundation::result::Result<
        farcaster::application::read::async_gateway::Request> unsupported()
    {
        return std::unexpected(farcaster::foundation::error::Error::make(
            farcaster::foundation::error::ErrorCode::Unsupported,
            "Unsupported by async routing test fixture."));
    }
};

void testRoutedReadGatewayDispatchesByCapability()
{
    using namespace farcaster;
    routing::registry::CapabilityRegistry registry{};
    const provider::model::ProviderDescriptor descriptor{
        .id = provider::model::ProviderId{"read-source"},
        .displayName = "Read Source",
        .deployment = provider::model::ProviderDeployment::SelfHosted,
        .privacy = provider::model::ProviderPrivacy::Private,
        .priority = 1,
    };
    assert(registry.registerProvider(routing::registry::ProviderRegistration{
        .provider = descriptor,
        .capabilities = {capability::model::CapabilityDescriptor{
            .id = capability::catalog::feedTrendingRead(),
            .version = capability::model::CapabilityVersion{1, 0},
            .origin = capability::model::CapabilityOrigin::Derived,
            .quality = capability::model::CapabilityQuality::Native,
            .access = {capability::model::CapabilityAccess::Read},
            .isSelfHostable = true,
        }},
    }));
    routing::health::HealthRegistry health{};
    routing::router::CapabilityRouter router{
        registry,
        health,
        routing::policy::RoutingPolicy{
            .mode = routing::policy::RoutingMode::StrictSelfHosted,
            .allowManagedProviders = false,
            .minimumQuality = capability::model::CapabilityQuality::Experimental,
        },
    };
    FakeFeedSource source{};
    routing::read::gateway::RoutedReadGateway gateway{
        router,
        health,
        {routing::read::gateway::SourceBinding{
            .providerId = descriptor.id,
            .feed = &source,
        }},
    };
    auto page = gateway.feed(application::read::model::FeedQuery{
        .kind = application::read::model::FeedQuery::Kind::Trending,
    });
    assert(page);
    assert(source.trendingCalls == 1);
    assert(source.lastTrendingLimit == 10);
}

void testCapabilityCircuitBreakerAndIsolation()
{
    using namespace farcaster;

    FakeFeedSource::Clock clock{};
    routing::health::HealthRegistry health{
        clock,
        routing::health::CircuitBreakerPolicy{
            .failureThreshold = 2,
            .cooldown = std::chrono::milliseconds{30'000},
        },
    };
    routing::registry::CapabilityRegistry registry{};
    const provider::model::ProviderDescriptor descriptor{
        .id = provider::model::ProviderId{"circuit-source"},
        .displayName = "Circuit Source",
        .deployment = provider::model::ProviderDeployment::SelfHosted,
        .privacy = provider::model::ProviderPrivacy::Private,
        .priority = 1,
    };
    assert(registry.registerProvider(routing::registry::ProviderRegistration{
        .provider = descriptor,
        .capabilities = {
            feedCapabilityFor(
                capability::catalog::feedTrendingRead(),
                capability::model::CapabilityOrigin::Derived),
            feedCapabilityFor(
                capability::catalog::feedFollowingRead(),
                capability::model::CapabilityOrigin::Protocol),
        },
    }));
    routing::router::CapabilityRouter router{
        registry,
        health,
        routing::policy::RoutingPolicy{
            .mode = routing::policy::RoutingMode::StrictSelfHosted,
            .allowManagedProviders = false,
            .minimumQuality = capability::model::CapabilityQuality::Experimental,
        },
    };
    FakeFeedSource source{};
    source.clock = &clock;
    source.trendingFailuresRemaining = 2;
    routing::read::gateway::RoutedReadGateway gateway{
        router,
        health,
        {routing::read::gateway::SourceBinding{
            .providerId = descriptor.id,
            .feed = &source,
        }},
    };
    const application::read::model::FeedQuery trending{
        .kind = application::read::model::FeedQuery::Kind::Trending,
    };
    const application::read::model::FeedQuery following{
        .kind = application::read::model::FeedQuery::Kind::Following,
        .fid = domain::identity::Fid{42},
    };

    const auto firstFailure = gateway.feed(trending);
    assert(!firstFailure);
    assert(firstFailure.error().message.find("Capability: feed.trending.read") != std::string::npos);
    assert(firstFailure.error().message.find("Provider: circuit-source") != std::string::npos);
    assert(firstFailure.error().message.find("Latency: 12000ms") != std::string::npos);
    assert(firstFailure.error().message.find("Result: Timeout") != std::string::npos);
    auto trendingHealth = health.find(descriptor.id, capability::catalog::feedTrendingRead());
    assert(trendingHealth);
    assert(trendingHealth->state == routing::health::HealthState::Suspect);
    assert(trendingHealth->circuit == routing::health::CircuitState::Closed);
    const auto attemptsBeforeNonRetryable = trendingHealth->observedAttempts;
    health.recordFailure(
        descriptor.id,
        capability::catalog::feedTrendingRead(),
        foundation::error::ErrorCode::NotFound,
        std::chrono::milliseconds{1});
    trendingHealth = health.find(descriptor.id, capability::catalog::feedTrendingRead());
    assert(trendingHealth);
    assert(trendingHealth->observedAttempts == attemptsBeforeNonRetryable);
    assert(trendingHealth->state == routing::health::HealthState::Suspect);

    const auto followingWhileTrendingSuspect = gateway.feed(following);
    assert(followingWhileTrendingSuspect);
    const auto followingHealth = health.find(descriptor.id, capability::catalog::feedFollowingRead());
    assert(followingHealth);
    assert(followingHealth->state == routing::health::HealthState::Healthy);

    const auto secondFailure = gateway.feed(trending);
    assert(!secondFailure);
    trendingHealth = health.find(descriptor.id, capability::catalog::feedTrendingRead());
    assert(trendingHealth);
    assert(trendingHealth->state == routing::health::HealthState::Degraded);
    assert(trendingHealth->circuit == routing::health::CircuitState::Open);
    assert(trendingHealth->cooldownRemaining == std::chrono::milliseconds{30'000});

    const auto fastFailure = gateway.feed(trending);
    assert(!fastFailure);
    assert(source.trendingCalls == 2);
    assert(fastFailure.error().message.find("feed.trending.read") != std::string::npos);
    assert(fastFailure.error().message.find("Provider: circuit-source") != std::string::npos);
    assert(fastFailure.error().message.find("Result: CircuitOpen") != std::string::npos);
    assert(fastFailure.error().message.find("CooldownRemaining: 30000ms") != std::string::npos);
    assert(gateway.feed(following));
    assert(source.followingCalls == 2);

    const auto manualProbe = gateway.feed(
        trending,
        application::read::model::ReadOptions{.allowRecoveryProbe = true});
    assert(manualProbe);
    assert(source.trendingCalls == 3);
    trendingHealth = health.find(descriptor.id, capability::catalog::feedTrendingRead());
    assert(trendingHealth);
    assert(trendingHealth->state == routing::health::HealthState::Healthy);
    assert(trendingHealth->circuit == routing::health::CircuitState::Closed);

    source.trendingFailuresRemaining = 3;
    assert(!gateway.feed(trending));
    assert(!gateway.feed(trending));
    const auto callsBeforeCooldown = source.trendingCalls;
    const auto failedManualProbe = gateway.feed(
        trending,
        application::read::model::ReadOptions{.allowRecoveryProbe = true});
    assert(!failedManualProbe);
    assert(source.trendingCalls == callsBeforeCooldown + 1);
    trendingHealth = health.find(descriptor.id, capability::catalog::feedTrendingRead());
    assert(trendingHealth);
    assert(trendingHealth->state == routing::health::HealthState::Degraded);
    assert(trendingHealth->circuit == routing::health::CircuitState::Open);
    assert(trendingHealth->cooldownRemaining == std::chrono::milliseconds{30'000});

    const auto callsAfterFailedProbe = source.trendingCalls;
    clock.advance(std::chrono::milliseconds{30'000});
    const auto automaticProbe = gateway.feed(trending);
    assert(automaticProbe);
    assert(source.trendingCalls == callsAfterFailedProbe + 1);
    trendingHealth = health.find(descriptor.id, capability::catalog::feedTrendingRead());
    assert(trendingHealth);
    assert(trendingHealth->state == routing::health::HealthState::Healthy);
    assert(routing::health::isRetryableFailure(foundation::error::ErrorCode::RateLimited));
    assert(!routing::health::isRetryableFailure(foundation::error::ErrorCode::NotFound));
}

void testAsyncPrefetchFailureDoesNotConsumeCircuitHealth()
{
    using namespace farcaster;

    FakeFeedSource::Clock clock{};
    routing::health::HealthRegistry health{
        clock,
        routing::health::CircuitBreakerPolicy{
            .failureThreshold = 2,
            .cooldown = std::chrono::milliseconds{30'000},
        },
    };
    routing::registry::CapabilityRegistry registry{};
    const provider::model::ProviderDescriptor descriptor{
        .id = provider::model::ProviderId{"async-prefetch-source"},
        .displayName = "Async Prefetch Source",
        .deployment = provider::model::ProviderDeployment::SelfHosted,
        .privacy = provider::model::ProviderPrivacy::Private,
        .priority = 1,
    };
    assert(registry.registerProvider(routing::registry::ProviderRegistration{
        .provider = descriptor,
        .capabilities = {feedCapabilityFor(
            capability::catalog::feedTrendingRead(),
            capability::model::CapabilityOrigin::Derived)},
    }));
    routing::router::CapabilityRouter router{
        registry,
        health,
        routing::policy::RoutingPolicy{
            .mode = routing::policy::RoutingMode::StrictSelfHosted,
            .allowManagedProviders = false,
            .minimumQuality = capability::model::CapabilityQuality::Experimental,
        },
    };
    PendingAsyncReadGateway source{};
    routing::read::async_gateway::RoutedReadGateway gateway{
        router,
        health,
        {routing::read::async_gateway::SourceBinding{
            .providerId = descriptor.id,
            .gateway = &source,
        }},
    };
    const auto capabilityId = capability::catalog::feedTrendingRead();
    const application::read::model::FeedQuery query{
        .kind = application::read::model::FeedQuery::Kind::Trending,
    };

    bool prefetchCompletionCalled{false};
    auto prefetch = gateway.feed(
        query,
        [&prefetchCompletionCalled](foundation::result::Result<capability::feed::FeedPage> result) {
            assert(!result);
            assert(result.error().code == foundation::error::ErrorCode::RateLimited);
            prefetchCompletionCalled = true;
        },
        application::read::model::ReadOptions{
            .intent = application::read::model::ReadIntent::Prefetch,
        });
    assert(prefetch);
    source.completePending(std::unexpected(foundation::error::Error::make(
        foundation::error::ErrorCode::RateLimited,
        "Background page was rate limited.")));
    assert(prefetchCompletionCalled);
    assert(!health.find(descriptor.id, capabilityId));

    bool visibleCompletionCalled{false};
    auto visible = gateway.feed(
        query,
        [&visibleCompletionCalled](foundation::result::Result<capability::feed::FeedPage> result) {
            assert(!result);
            visibleCompletionCalled = true;
        });
    assert(visible);
    source.completePending(std::unexpected(foundation::error::Error::make(
        foundation::error::ErrorCode::Unavailable,
        "Visible request failed.")));
    assert(visibleCompletionCalled);
    const auto visibleHealth = health.find(descriptor.id, capabilityId);
    assert(visibleHealth);
    assert(visibleHealth->observedAttempts == 1U);
    assert(visibleHealth->consecutiveFailures == 1U);
    assert(visibleHealth->circuit == routing::health::CircuitState::Closed);
}

void testAsyncRecoveryProbeTerminalization()
{
    using namespace farcaster;

    FakeFeedSource::Clock clock{};
    routing::health::HealthRegistry health{
        clock,
        routing::health::CircuitBreakerPolicy{
            .failureThreshold = 2,
            .cooldown = std::chrono::milliseconds{30'000},
        },
    };
    routing::registry::CapabilityRegistry registry{};
    const provider::model::ProviderDescriptor descriptor{
        .id = provider::model::ProviderId{"async-circuit-source"},
        .displayName = "Async Circuit Source",
        .deployment = provider::model::ProviderDeployment::SelfHosted,
        .privacy = provider::model::ProviderPrivacy::Private,
        .priority = 1,
    };
    assert(registry.registerProvider(routing::registry::ProviderRegistration{
        .provider = descriptor,
        .capabilities = {feedCapabilityFor(
            capability::catalog::feedTrendingRead(),
            capability::model::CapabilityOrigin::Derived)},
    }));
    routing::router::CapabilityRouter router{
        registry,
        health,
        routing::policy::RoutingPolicy{
            .mode = routing::policy::RoutingMode::StrictSelfHosted,
            .allowManagedProviders = false,
            .minimumQuality = capability::model::CapabilityQuality::Experimental,
        },
    };
    PendingAsyncReadGateway source{};
    routing::read::async_gateway::RoutedReadGateway gateway{
        router,
        health,
        {routing::read::async_gateway::SourceBinding{
            .providerId = descriptor.id,
            .gateway = &source,
        }},
    };
    const auto capabilityId = capability::catalog::feedTrendingRead();
    const application::read::model::FeedQuery query{
        .kind = application::read::model::FeedQuery::Kind::Trending,
    };

    health.recordFailure(
        descriptor.id, capabilityId,
        foundation::error::ErrorCode::Timeout,
        std::chrono::milliseconds{5});
    health.recordFailure(
        descriptor.id, capabilityId,
        foundation::error::ErrorCode::Timeout,
        std::chrono::milliseconds{5});
    const auto openHealth = health.find(descriptor.id, capabilityId);
    assert(openHealth);
    assert(openHealth->circuit == routing::health::CircuitState::Open);
    const auto attemptsBeforeProbe = openHealth->observedAttempts;
    const auto failuresBeforeProbe = openHealth->consecutiveFailures;

    bool completionCalled = false;
    auto probe = gateway.feed(
        query,
        [&completionCalled](foundation::result::Result<capability::feed::FeedPage>) {
            completionCalled = true;
        },
        application::read::model::ReadOptions{.allowRecoveryProbe = true});
    assert(probe);
    auto probeHealth = health.find(descriptor.id, capabilityId);
    assert(probeHealth);
    assert(probeHealth->circuit == routing::health::CircuitState::HalfOpen);

    probe->cancel();
    assert(source.sourceCancelled);
    probeHealth = health.find(descriptor.id, capabilityId);
    assert(probeHealth);
    assert(probeHealth->circuit == routing::health::CircuitState::Open);
    assert(probeHealth->observedAttempts == attemptsBeforeProbe);
    assert(probeHealth->consecutiveFailures == failuresBeforeProbe);

    // Even a source that races a late completion after cancellation cannot
    // poison health or escape through the routed completion boundary.
    source.completePending(std::unexpected(foundation::error::Error::make(
        foundation::error::ErrorCode::Unavailable,
        "Late completion after cancellation.")));
    assert(!completionCalled);
    probeHealth = health.find(descriptor.id, capabilityId);
    assert(probeHealth);
    assert(probeHealth->circuit == routing::health::CircuitState::Open);
    assert(probeHealth->observedAttempts == attemptsBeforeProbe);

    bool neutralCompletionCalled = false;
    auto neutralProbe = gateway.feed(
        query,
        [&neutralCompletionCalled](
            foundation::result::Result<capability::feed::FeedPage> result) {
            assert(!result);
            assert(result.error().code == foundation::error::ErrorCode::NotFound);
            neutralCompletionCalled = true;
        },
        application::read::model::ReadOptions{.allowRecoveryProbe = true});
    assert(neutralProbe);
    source.completePending(std::unexpected(foundation::error::Error::make(
        foundation::error::ErrorCode::NotFound,
        "Neutral terminal recovery result.")));
    assert(neutralCompletionCalled);
    probeHealth = health.find(descriptor.id, capabilityId);
    assert(probeHealth);
    assert(probeHealth->circuit == routing::health::CircuitState::Open);
    assert(probeHealth->observedAttempts == attemptsBeforeProbe);
    assert(probeHealth->consecutiveFailures == failuresBeforeProbe);

    auto recovery = gateway.feed(
        query,
        [&completionCalled](foundation::result::Result<capability::feed::FeedPage> result) {
            assert(result);
            completionCalled = true;
        },
        application::read::model::ReadOptions{.allowRecoveryProbe = true});
    assert(recovery);
    assert(source.feedCalls == 3);
    source.completePending(capability::feed::FeedPage{});
    assert(completionCalled);
    const auto recoveredHealth = health.find(descriptor.id, capabilityId);
    assert(recoveredHealth);
    assert(recoveredHealth->circuit == routing::health::CircuitState::Closed);
    assert(recoveredHealth->state == routing::health::HealthState::Healthy);
}

}

int main()
{
    testValuationContract();
    testConfiguredFeatureSet();
    testCapabilityRouting();
    testRoutingFailureDiagnostics();
    testSyncCoordinator();
    testRoutedReadGatewayDispatchesByCapability();
    testCapabilityCircuitBreakerAndIsolation();
    testAsyncPrefetchFailureDoesNotConsumeCircuitHealth();
    testAsyncRecoveryProbeTerminalization();
    return 0;
}
