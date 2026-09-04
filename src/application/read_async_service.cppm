module;

#include <atomic>
#include <functional>
#include <memory>
#include <string>

export module farcaster.application.read.async_service;

import farcaster.application.executor;
import farcaster.application.read.async_gateway;
import farcaster.application.read.model;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.foundation.result;
import farcaster.local.read;

export namespace farcaster::application::read::async_service {

/**
 * @brief Cancellation handle for one end-to-end asynchronous local-first read.
 *
 * Cancellation suppresses observer delivery and propagates to all currently
 * active provider operations attached to the request.
 */
class ReadRequest final {
public:
    ReadRequest() = default;
    /** @brief Cancels network work and future observer delivery. */
    void cancel() noexcept;
    /** @brief Returns whether cancellation has been requested. */
    [[nodiscard]] bool isCancelled() const noexcept;

private:
    struct Control;
    explicit ReadRequest(std::shared_ptr<Control> control) noexcept;
    std::shared_ptr<Control> m_control{};
    friend class ReadService;
};

/**
 * @brief Event-driven local-first read orchestration.
 *
 * The cache executor serializes persistence. The gateway owns asynchronous
 * network/decode work. No application worker remains blocked while waiting for
 * network I/O.
 */
class ReadService final {
public:
    using IdentityObserver = std::function<void(model::IdentityState)>;
    using ProfileObserver = std::function<void(model::ProfileState)>;
    using CastObserver = std::function<void(model::CastState)>;
    using FeedObserver = std::function<void(model::FeedState)>;
    using SearchObserver = std::function<void(model::SearchState)>;
    using NotificationObserver = std::function<void(model::NotificationState)>;
    using SocialObserver = std::function<void(model::SocialState)>;

    ReadService(
        executor::IExecutor& cacheExecutor,
        local::read::IReadStore& store,
        async_gateway::IAsyncReadGateway& gateway) noexcept;

    [[nodiscard]] foundation::result::Result<ReadRequest> openIdentity(
        std::string username,
        IdentityObserver observer,
        model::ReadOptions options = {});

    [[nodiscard]] foundation::result::Result<ReadRequest> openProfile(
        domain::identity::Fid fid,
        ProfileObserver observer,
        model::ReadOptions options = {});

    [[nodiscard]] foundation::result::Result<ReadRequest> openCast(
        domain::cast::CastId castId,
        CastObserver observer,
        model::ReadOptions options = {});

    [[nodiscard]] foundation::result::Result<ReadRequest> openFeed(
        model::FeedQuery query,
        FeedObserver observer,
        model::ReadOptions options = {});

    [[nodiscard]] foundation::result::Result<ReadRequest> openSearch(
        model::SearchQuery query,
        SearchObserver observer,
        model::ReadOptions options = {});

    [[nodiscard]] foundation::result::Result<ReadRequest> openNotifications(
        model::NotificationQuery query,
        NotificationObserver observer,
        model::ReadOptions options = {});

    [[nodiscard]] foundation::result::Result<ReadRequest> openSocial(
        model::SocialQuery query,
        SocialObserver observer,
        model::ReadOptions options = {});

private:
    executor::IExecutor& m_cacheExecutor;
    local::read::IReadStore& m_store;
    async_gateway::IAsyncReadGateway& m_gateway;
};

}
