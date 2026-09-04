module;

#include <string_view>
#include <vector>

export module farcaster.routing.read.async_gateway;

import farcaster.application.read.async_gateway;
import farcaster.application.read.model;
import farcaster.capability.feed;
import farcaster.capability.notification;
import farcaster.capability.search;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.result;
import farcaster.provider.model;
import farcaster.routing.health;
import farcaster.routing.router;

export namespace farcaster::routing::read::async_gateway {

/**
 * @brief Binds one registered provider to its provider-neutral async gateway.
 *
 * The borrowed gateway must outlive this binding and all requests started
 * through the routed gateway.
 */
struct SourceBinding final {
    provider::model::ProviderId providerId{};
    application::read::async_gateway::IAsyncReadGateway* gateway{};
};

/**
 * @brief Applies capability routing, circuit admission and health observation to async reads.
 *
 * No operation waits for network I/O. Routing and circuit checks happen before
 * provider admission; completion records latency and retryable failures without
 * moving decoding or storage work onto the caller thread.
 */
class RoutedReadGateway final : public application::read::async_gateway::IAsyncReadGateway {
public:
    RoutedReadGateway(
        const router::CapabilityRouter& router,
        health::HealthRegistry& health,
        std::vector<SourceBinding> bindings) noexcept;

    [[nodiscard]] foundation::result::Result<application::read::async_gateway::Request>
    resolveIdentity(
        std::string_view username,
        application::read::async_gateway::IdentityCompletion completion,
        application::read::model::ReadOptions options = {}) override;

    [[nodiscard]] foundation::result::Result<application::read::async_gateway::Request>
    profile(
        domain::identity::Fid fid,
        application::read::async_gateway::ProfileCompletion completion,
        application::read::model::ReadOptions options = {}) override;

    [[nodiscard]] foundation::result::Result<application::read::async_gateway::Request>
    cast(
        domain::cast::CastId castId,
        application::read::async_gateway::CastCompletion completion,
        application::read::model::ReadOptions options = {}) override;

    [[nodiscard]] foundation::result::Result<application::read::async_gateway::Request>
    feed(
        application::read::model::FeedQuery query,
        application::read::async_gateway::FeedCompletion completion,
        application::read::model::ReadOptions options = {}) override;

    [[nodiscard]] foundation::result::Result<application::read::async_gateway::Request>
    search(
        application::read::model::SearchQuery query,
        application::read::async_gateway::SearchCompletion completion,
        application::read::model::ReadOptions options = {}) override;

    [[nodiscard]] foundation::result::Result<application::read::async_gateway::Request>
    notifications(
        application::read::model::NotificationQuery query,
        application::read::async_gateway::NotificationCompletion completion,
        application::read::model::ReadOptions options = {}) override;

    [[nodiscard]] foundation::result::Result<application::read::async_gateway::Request>
    social(
        application::read::model::SocialQuery query,
        application::read::async_gateway::SocialCompletion completion,
        application::read::model::ReadOptions options = {}) override;

private:
    [[nodiscard]] foundation::result::Result<const SourceBinding*>
    bindingFor(const provider::model::ProviderId& providerId) const;

    const router::CapabilityRouter& m_router;
    health::HealthRegistry& m_health;
    std::vector<SourceBinding> m_bindings{};
};

}
