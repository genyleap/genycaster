/*!
 * @file        src/application/read_async_gateway.cppm
 * @brief       Asynchronous read gateway contract.
 *
 * @details
 * Provides cancellable completion-based reads between application use
 * cases and eligible remote capability implementations.
 *
 * @author      Kambiz Asadzadeh
 * @since       04 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <functional>
#include <memory>
#include <string_view>

export module farcaster.application.read.async_gateway;

import farcaster.application.read.model;
import farcaster.capability.feed;
import farcaster.capability.notification;
import farcaster.capability.search;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.result;

export namespace farcaster::application::read::async_gateway {

/** @brief Copyable cancellation handle for one provider-neutral async read. */
class Request final {
public:
    Request() = default;
    explicit Request(std::function<void()> cancelFunction)
        : m_cancelFunction(std::make_shared<std::function<void()>>(std::move(cancelFunction)))
    {
    }

    /** @brief Requests cancellation. Safe to call more than once. */
    void cancel() const noexcept
    {
        if (!m_cancelFunction) {
            return;
        }
        try {
            const auto function = *m_cancelFunction;
            if (function) {
                function();
            }
        } catch (...) {
        }
    }

    [[nodiscard]] bool isActive() const noexcept
    {
        return static_cast<bool>(m_cancelFunction);
    }

private:
    std::shared_ptr<std::function<void()>> m_cancelFunction{};
};

using IdentityCompletion = std::function<void(
    foundation::result::Result<domain::identity::Fid>)>;
using ProfileCompletion = std::function<void(
    foundation::result::Result<domain::profile::Profile>)>;
using CastCompletion = std::function<void(
    foundation::result::Result<domain::cast::Cast>)>;
using FeedCompletion = std::function<void(
    foundation::result::Result<capability::feed::FeedPage>)>;
using SearchCompletion = std::function<void(
    foundation::result::Result<capability::search::SearchPage>)>;
using NotificationCompletion = std::function<void(
    foundation::result::Result<capability::notification::NotificationPage>)>;
using SocialCompletion = std::function<void(
    foundation::result::Result<capability::social::SocialGraphPage>)>;

/**
 * @brief Provider-neutral event-driven read gateway.
 *
 * Implementations must return without waiting for network I/O. Completion may
 * execute on a bounded provider decode executor and therefore must not assume a
 * GUI thread.
 */
class IAsyncReadGateway {
public:
    virtual ~IAsyncReadGateway() = default;

    [[nodiscard]] virtual foundation::result::Result<Request> resolveIdentity(
        std::string_view username,
        IdentityCompletion completion,
        model::ReadOptions options = {}) = 0;

    [[nodiscard]] virtual foundation::result::Result<Request> profile(
        domain::identity::Fid fid,
        ProfileCompletion completion,
        model::ReadOptions options = {}) = 0;

    [[nodiscard]] virtual foundation::result::Result<Request> cast(
        domain::cast::CastId castId,
        CastCompletion completion,
        model::ReadOptions options = {}) = 0;

    [[nodiscard]] virtual foundation::result::Result<Request> feed(
        model::FeedQuery query,
        FeedCompletion completion,
        model::ReadOptions options = {}) = 0;

    [[nodiscard]] virtual foundation::result::Result<Request> search(
        model::SearchQuery query,
        SearchCompletion completion,
        model::ReadOptions options = {}) = 0;

    [[nodiscard]] virtual foundation::result::Result<Request> notifications(
        model::NotificationQuery query,
        NotificationCompletion completion,
        model::ReadOptions options = {}) = 0;

    [[nodiscard]] virtual foundation::result::Result<Request> social(
        model::SocialQuery query,
        SocialCompletion completion,
        model::ReadOptions options = {}) = 0;
};

}
