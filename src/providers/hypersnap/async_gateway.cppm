module;

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

export module farcaster.provider.hypersnap.async_gateway;

import farcaster.application.executor;
import farcaster.application.read.async_gateway;
import farcaster.application.read.model;
import farcaster.domain.cast;
import farcaster.domain.feed;
import farcaster.domain.identity;
import farcaster.execution.weighted_thread_pool;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.async_request;
import farcaster.provider.hypersnap.config;
import farcaster.storage.continuation;
import farcaster.transport.async_http;

export namespace farcaster::provider::hypersnap::async_gateway {

/**
 * @brief End-to-end asynchronous Hypersnap read gateway used by the Qt runtime.
 *
 * Provider cursor persistence is serialized on the supplied continuation
 * executor. Network I/O uses the shared reactor and JSON decoding uses the
 * supplied weighted CPU pool.
 */
class AsyncGateway final : public application::read::async_gateway::IAsyncReadGateway {
public:
    AsyncGateway(
        transport::async_http::IAsyncHttpTransport& transport,
        execution::weighted_thread_pool::WeightedThreadPoolExecutor& decodeExecutor,
        application::executor::IExecutor& continuationExecutor,
        storage::continuation::ContinuationStore& continuations,
        config::Config config);

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

    /** @brief Resolves exact Like/Recast state for visible cast targets. */
    [[nodiscard]] foundation::result::Result<application::read::async_gateway::Request>
    viewerReactionsFor(
        domain::identity::Fid viewer,
        std::vector<domain::cast::CastReference> targets,
        std::function<void(foundation::result::Result<domain::feed::ViewerReactionSnapshot>)> completion);

    /** @brief Resolves one following relationship without blocking a worker. */
    [[nodiscard]] foundation::result::Result<application::read::async_gateway::Request>
    isFollowing(
        domain::identity::Fid source,
        domain::identity::Fid target,
        std::function<void(foundation::result::Result<bool>)> completion);

    /** @brief Resolves an exact protocol Link(type=block) from the selected node. */
    [[nodiscard]] foundation::result::Result<application::read::async_gateway::Request>
    isBlocked(
        domain::identity::Fid source,
        domain::identity::Fid target,
        std::function<void(foundation::result::Result<bool>)> completion);

private:
    struct RecentWindowCache;
    /** @brief Loads confirmed network casts, keeping blocking work off Qt. */
    [[nodiscard]] foundation::result::Result<application::read::async_gateway::Request>
    recentFeed(
        application::read::model::FeedQuery query,
        application::read::async_gateway::FeedCompletion completion,
        application::read::model::ReadOptions options);

    std::shared_ptr<RecentWindowCache> m_recentWindowCache{};
    async_request::Client m_client;
    application::executor::IExecutor& m_continuationExecutor;
    storage::continuation::ContinuationStore& m_continuations;
    config::Config m_config{};
    // Provider-shape incompatibility is global to this adapter, not to the
    // visible cast. Avoid probing the same unsupported batch contract on
    // every navigation for fifteen minutes.
    std::atomic<std::int64_t> m_batchReactionProbeAfterMs{0};
};

}
