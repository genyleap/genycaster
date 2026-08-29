module;

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <utility>

export module farcaster.application.read.service;

import farcaster.application.executor;
import farcaster.application.read.gateway;
import farcaster.application.read.model;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.foundation.result;
import farcaster.local.read;

export namespace farcaster::application::read::service {

/**
 * @brief Move-only cancellation handle for one local-first read request.
 *
 * Cancellation suppresses future observer delivery. It does not forcibly
 * interrupt an already-running blocking provider call.
 */
class ReadRequest final {
public:
    /** @brief Creates an inactive request handle. */
    ReadRequest() = default;
    /** @brief Creates a handle sharing an internal cancellation flag. */
    explicit ReadRequest(std::shared_ptr<std::atomic_bool> cancelled) noexcept;
    /** @brief Cancels observer delivery for this request. */
    void cancel() noexcept;
    /** @brief Returns whether cancellation has been requested. */
    [[nodiscard]] bool isCancelled() const noexcept;

private:
    std::shared_ptr<std::atomic_bool> m_cancelled{};
    friend class ReadService;
};

/**
 * @brief Coordinates cache-first reads, background refresh and reconciliation.
 *
 * The executor, store and gateway are borrowed and must outlive the service and
 * every request. Observers run on the executor context; presentation adapters
 * must marshal state to their UI thread.
 */
class ReadService final {
public:
    /** @brief Observer receiving immutable owned identity snapshots. */
    using IdentityObserver = std::function<void(model::IdentityState)>;
    /** @brief Observer receiving immutable owned profile snapshots. */
    using ProfileObserver = std::function<void(model::ProfileState)>;
    /** @brief Observer receiving immutable owned cast snapshots. */
    using CastObserver = std::function<void(model::CastState)>;
    /** @brief Observer receiving immutable owned feed snapshots. */
    using FeedObserver = std::function<void(model::FeedState)>;
    /** @brief Observer receiving immutable owned social snapshots. */
    using SocialObserver = std::function<void(model::SocialState)>;

    /**
     * @brief Creates a service over borrowed execution, persistence and gateway ports.
     */
    ReadService(
        executor::IExecutor& executor,
        local::read::IReadStore& store,
        gateway::IReadGateway& gateway) noexcept;

    /**
     * @brief Creates a cache-first service with independent cache and refresh lanes.
     *
     * Keeping local SQLite work separate from blocking network refresh prevents a
     * slow provider request from delaying navigation to already-cached content.
     */
    ReadService(
        executor::IExecutor& cacheExecutor,
        executor::IExecutor& refreshExecutor,
        local::read::IReadStore& store,
        gateway::IReadGateway& gateway) noexcept;

    /** @brief Resolves and opens an identity, publishing cache before refresh. */
    [[nodiscard]] foundation::result::Result<ReadRequest>
    openIdentity(
        std::string username,
        IdentityObserver observer,
        model::ReadOptions options = {});
    /** @brief Opens a profile, publishing cache before refresh. */
    [[nodiscard]] foundation::result::Result<ReadRequest>
    openProfile(
        domain::identity::Fid fid,
        ProfileObserver observer,
        model::ReadOptions options = {});
    /** @brief Opens a cast and its cached/refreshed author profile. */
    [[nodiscard]] foundation::result::Result<ReadRequest>
    openCast(
        domain::cast::CastId castId,
        CastObserver observer,
        model::ReadOptions options = {});
    /** @brief Opens or paginates a feed while retaining the materialized cache. */
    [[nodiscard]] foundation::result::Result<ReadRequest>
    openFeed(
        model::FeedQuery query,
        FeedObserver observer,
        model::ReadOptions options = {});
    /** @brief Opens or paginates a social graph list cache-first. */
    [[nodiscard]] foundation::result::Result<ReadRequest>
    openSocial(
        model::SocialQuery query,
        SocialObserver observer,
        model::ReadOptions options = {});

private:
    executor::IExecutor& m_cacheExecutor;
    executor::IExecutor& m_refreshExecutor;
    local::read::IReadStore& m_store;
    gateway::IReadGateway& m_gateway;
};

}
