module;

#include <atomic>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

module farcaster.application.read.async_service;

import farcaster.application.executor;
import farcaster.application.read.async_gateway;
import farcaster.application.read.model;
import farcaster.capability.feed;
import farcaster.capability.notification;
import farcaster.capability.search;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.local.read;

namespace farcaster::application::read::async_service {
namespace {

using Error = foundation::error::Error;
using ErrorCode = foundation::error::ErrorCode;

[[nodiscard]] bool isSourceUnavailableFailure(const ErrorCode code) noexcept
{
    return code == ErrorCode::Unavailable || code == ErrorCode::Timeout ||
        code == ErrorCode::TransportFailure || code == ErrorCode::RateLimited;
}

[[nodiscard]] model::ReadMetadata refreshingMetadata(const bool hasCache)
{
    return model::ReadMetadata{
        .phase = model::ReadPhase::Refreshing,
        .isStale = hasCache,
    };
}

[[nodiscard]] model::ReadMetadata failedMetadata(Error error, const bool hasUsableData)
{
    return model::ReadMetadata{
        .phase = model::ReadPhase::Failed,
        .isStale = hasUsableData,
        .isSourceUnavailable = isSourceUnavailableFailure(error.code),
        .error = std::move(error),
    };
}

[[nodiscard]] local::read::FeedKey toLocalKey(const model::FeedQuery& query)
{
    local::read::FeedKey::Kind kind = local::read::FeedKey::Kind::Trending;
    switch (query.kind) {
    case model::FeedQuery::Kind::Following:
        kind = local::read::FeedKey::Kind::Following;
        break;
    case model::FeedQuery::Kind::Trending:
        kind = local::read::FeedKey::Kind::Trending;
        break;
    case model::FeedQuery::Kind::Channel:
        kind = local::read::FeedKey::Kind::Channel;
        break;
    case model::FeedQuery::Kind::UserPosts:
        kind = local::read::FeedKey::Kind::UserPosts;
        break;
    case model::FeedQuery::Kind::UserQuotes:
        kind = local::read::FeedKey::Kind::UserQuotes;
        break;
    case model::FeedQuery::Kind::UserReplies:
        kind = local::read::FeedKey::Kind::UserReplies;
        break;
    case model::FeedQuery::Kind::UserLikes:
        kind = local::read::FeedKey::Kind::UserLikes;
        break;
    case model::FeedQuery::Kind::UserPopular:
        kind = local::read::FeedKey::Kind::UserPopular;
        break;
    }
    return local::read::FeedKey{
        .kind = kind,
        .fid = query.fid,
        .channel = query.channel,
        .limit = query.limit,
    };
}

[[nodiscard]] local::read::SocialKey toLocalKey(const model::SocialQuery& query)
{
    return local::read::SocialKey{
        .kind = query.kind == model::SocialQuery::Kind::Followers
            ? local::read::SocialKey::Kind::Followers
            : local::read::SocialKey::Kind::Following,
        .fid = query.fid,
        .limit = query.limit,
    };
}

}

struct ReadRequest::Control final {
    std::atomic_bool cancelled{false};
    std::mutex mutex{};
    std::vector<async_gateway::Request> children{};

    void attach(async_gateway::Request request)
    {
        if (!request.isActive()) {
            return;
        }
        if (cancelled.load(std::memory_order_acquire)) {
            request.cancel();
            return;
        }
        std::scoped_lock lock{mutex};
        if (cancelled.load(std::memory_order_acquire)) {
            request.cancel();
            return;
        }
        children.push_back(std::move(request));
    }

    void cancelAll() noexcept
    {
        if (cancelled.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        std::vector<async_gateway::Request> snapshot{};
        {
            std::scoped_lock lock{mutex};
            snapshot = children;
        }
        for (const auto& child : snapshot) {
            child.cancel();
        }
    }
};

namespace {

template <typename Control, typename Observer, typename State>
void publish(
    const std::shared_ptr<Control>& control,
    Observer& observer,
    State state)
{
    if (!control->cancelled.load(std::memory_order_acquire)) {
        observer(std::move(state));
    }
}

}

ReadRequest::ReadRequest(std::shared_ptr<Control> control) noexcept
    : m_control(std::move(control))
{
}

void ReadRequest::cancel() noexcept
{
    if (m_control) {
        m_control->cancelAll();
    }
}

bool ReadRequest::isCancelled() const noexcept
{
    return m_control && m_control->cancelled.load(std::memory_order_acquire);
}

ReadService::ReadService(
    executor::IExecutor& cacheExecutor,
    local::read::IReadStore& store,
    async_gateway::IAsyncReadGateway& gateway) noexcept
    : m_cacheExecutor(cacheExecutor),
      m_store(store),
      m_gateway(gateway)
{
}

foundation::result::Result<ReadRequest> ReadService::openIdentity(
    std::string username,
    IdentityObserver observer,
    const model::ReadOptions options)
{
    if (username.empty() || !observer) {
        return std::unexpected(Error::make(
            ErrorCode::InvalidArgument,
            "Identity username and observer are required."));
    }
    auto control = std::make_shared<ReadRequest::Control>();
    auto initialStatus = m_cacheExecutor.post(
        [this, username = std::move(username), observer = std::move(observer), options, control]() mutable {
            if (control->cancelled.load(std::memory_order_acquire)) {
                return;
            }
            model::IdentityState cachedState{.username = username};
            auto cached = m_store.findProfileByUsername(username);
            if (cached) {
                cachedState.profile = std::move(*cached);
                if (cachedState.profile) {
                    cachedState.fid = cachedState.profile->fid;
                }
                cachedState.metadata = refreshingMetadata(cachedState.profile.has_value());
            } else {
                cachedState.metadata = failedMetadata(std::move(cached.error()), false);
            }
            publish(control, observer, std::move(cachedState));

            auto identity = m_gateway.resolveIdentity(
                username,
                [this, username, observer, options, control](
                    foundation::result::Result<domain::identity::Fid> fid) mutable {
                    if (control->cancelled.load(std::memory_order_acquire)) {
                        return;
                    }
                    if (!fid) {
                        auto error = std::move(fid.error());
                        auto status = m_cacheExecutor.post(
                            [this, username, observer, control, error = std::move(error)]() mutable {
                                model::IdentityState state{.username = username};
                                auto cached = m_store.findProfileByUsername(username);
                                if (cached) {
                                    state.profile = std::move(*cached);
                                    if (state.profile) {
                                        state.fid = state.profile->fid;
                                    }
                                }
                                state.metadata = failedMetadata(std::move(error), state.profile.has_value());
                                publish(control, observer, std::move(state));
                            });
                        static_cast<void>(status);
                        return;
                    }

                    const auto resolvedFid = *fid;
                    auto profile = m_gateway.profile(
                        resolvedFid,
                        [this, username, resolvedFid, observer, control](
                            foundation::result::Result<domain::profile::Profile> profile) mutable {
                            if (control->cancelled.load(std::memory_order_acquire)) {
                                return;
                            }
                            auto status = m_cacheExecutor.post(
                                [this, username, resolvedFid, observer, control,
                                 profile = std::move(profile)]() mutable {
                                    model::IdentityState state{
                                        .username = username,
                                        .fid = resolvedFid,
                                    };
                                    if (!profile) {
                                        auto cached = m_store.findProfileByUsername(username);
                                        if (cached) {
                                            state.profile = std::move(*cached);
                                        }
                                        state.metadata = failedMetadata(
                                            std::move(profile.error()), state.profile.has_value());
                                        publish(control, observer, std::move(state));
                                        return;
                                    }
                                    auto saved = m_store.putProfile(*profile);
                                    if (!saved) {
                                        state.profile = std::move(*profile);
                                        state.metadata = failedMetadata(
                                            std::move(saved.error()), true);
                                        publish(control, observer, std::move(state));
                                        return;
                                    }
                                    state.profile = std::move(*profile);
                                    state.metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready};
                                    publish(control, observer, std::move(state));
                                });
                            static_cast<void>(status);
                        },
                        options);
                    if (!profile) {
                        auto error = std::move(profile.error());
                        auto status = m_cacheExecutor.post(
                            [this, username, resolvedFid, observer, control,
                             error = std::move(error)]() mutable {
                                model::IdentityState state{
                                    .username = username,
                                    .fid = resolvedFid,
                                };
                                auto cached = m_store.findProfileByUsername(username);
                                if (cached) {
                                    state.profile = std::move(*cached);
                                }
                                state.metadata = failedMetadata(
                                    std::move(error), state.profile.has_value());
                                publish(control, observer, std::move(state));
                            });
                        static_cast<void>(status);
                        return;
                    }
                    control->attach(std::move(*profile));
                },
                options);
            if (!identity) {
                model::IdentityState state{.username = username};
                auto cachedAgain = m_store.findProfileByUsername(username);
                if (cachedAgain) {
                    state.profile = std::move(*cachedAgain);
                    if (state.profile) {
                        state.fid = state.profile->fid;
                    }
                }
                state.metadata = failedMetadata(
                    std::move(identity.error()), state.profile.has_value());
                publish(control, observer, std::move(state));
                return;
            }
            control->attach(std::move(*identity));
        });
    if (!initialStatus) {
        return std::unexpected(std::move(initialStatus.error()));
    }
    return ReadRequest{control};
}

foundation::result::Result<ReadRequest> ReadService::openProfile(
    const domain::identity::Fid fid,
    ProfileObserver observer,
    const model::ReadOptions options)
{
    if (fid.value == 0U || !observer) {
        return std::unexpected(Error::make(
            ErrorCode::InvalidArgument,
            "Profile FID and observer are required."));
    }
    auto control = std::make_shared<ReadRequest::Control>();
    auto initialStatus = m_cacheExecutor.post(
        [this, fid, observer = std::move(observer), options, control]() mutable {
            model::ProfileState state{.fid = fid};
            auto cached = m_store.findProfile(fid);
            if (cached) {
                state.profile = std::move(*cached);
                state.metadata = refreshingMetadata(state.profile.has_value());
            } else {
                state.metadata = failedMetadata(std::move(cached.error()), false);
            }
            publish(control, observer, std::move(state));

            auto request = m_gateway.profile(
                fid,
                [this, fid, observer, control](foundation::result::Result<domain::profile::Profile> fresh) mutable {
                    auto status = m_cacheExecutor.post(
                        [this, fid, observer, control, fresh = std::move(fresh)]() mutable {
                            if (control->cancelled.load(std::memory_order_acquire)) {
                                return;
                            }
                            model::ProfileState state{.fid = fid};
                            if (!fresh) {
                                auto cached = m_store.findProfile(fid);
                                if (cached) {
                                    state.profile = std::move(*cached);
                                }
                                state.metadata = failedMetadata(
                                    std::move(fresh.error()), state.profile.has_value());
                                publish(control, observer, std::move(state));
                                return;
                            }
                            auto saved = m_store.putProfile(*fresh);
                            if (!saved) {
                                state.profile = std::move(*fresh);
                                state.metadata = failedMetadata(std::move(saved.error()), true);
                                publish(control, observer, std::move(state));
                                return;
                            }
                            state.profile = std::move(*fresh);
                            state.metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready};
                            publish(control, observer, std::move(state));
                        });
                    static_cast<void>(status);
                },
                options);
            if (!request) {
                model::ProfileState failed{.fid = fid};
                auto cachedAgain = m_store.findProfile(fid);
                if (cachedAgain) {
                    failed.profile = std::move(*cachedAgain);
                }
                failed.metadata = failedMetadata(
                    std::move(request.error()), failed.profile.has_value());
                publish(control, observer, std::move(failed));
                return;
            }
            control->attach(std::move(*request));
        });
    if (!initialStatus) {
        return std::unexpected(std::move(initialStatus.error()));
    }
    return ReadRequest{control};
}

foundation::result::Result<ReadRequest> ReadService::openCast(
    domain::cast::CastId castId,
    CastObserver observer,
    const model::ReadOptions options)
{
    if (castId.value.empty() || !observer) {
        return std::unexpected(Error::make(
            ErrorCode::InvalidArgument,
            "Cast id and observer are required."));
    }
    auto control = std::make_shared<ReadRequest::Control>();
    auto initialStatus = m_cacheExecutor.post(
        [this, castId = std::move(castId), observer = std::move(observer), options, control]() mutable {
            model::CastState state{.castId = castId};
            auto cached = m_store.findCast(castId);
            if (cached) {
                state.cast = std::move(*cached);
                if (state.cast) {
                    auto author = m_store.findProfile(state.cast->author);
                    if (author) {
                        state.author = std::move(*author);
                    }
                }
                state.metadata = refreshingMetadata(state.cast.has_value());
            } else {
                state.metadata = failedMetadata(std::move(cached.error()), false);
            }
            publish(control, observer, std::move(state));

            auto request = m_gateway.cast(
                castId,
                [this, castId, observer, options, control](
                    foundation::result::Result<domain::cast::Cast> fresh) mutable {
                    if (!fresh) {
                        auto error = std::move(fresh.error());
                        auto status = m_cacheExecutor.post(
                            [this, castId, observer, control, error = std::move(error)]() mutable {
                                model::CastState state{.castId = castId};
                                auto cached = m_store.findCast(castId);
                                if (cached) {
                                    state.cast = std::move(*cached);
                                    if (state.cast) {
                                        auto author = m_store.findProfile(state.cast->author);
                                        if (author) {
                                            state.author = std::move(*author);
                                        }
                                    }
                                }
                                state.metadata = failedMetadata(
                                    std::move(error), state.cast.has_value());
                                publish(control, observer, std::move(state));
                            });
                        static_cast<void>(status);
                        return;
                    }

                    auto castValue = std::make_shared<domain::cast::Cast>(std::move(*fresh));
                    auto authorRequest = m_gateway.profile(
                        castValue->author,
                        [this, castId, castValue, observer, control](
                            foundation::result::Result<domain::profile::Profile> author) mutable {
                            auto status = m_cacheExecutor.post(
                                [this, castId, castValue, observer, control,
                                 author = std::move(author)]() mutable {
                                    if (control->cancelled.load(std::memory_order_acquire)) {
                                        return;
                                    }
                                    model::CastState state{
                                        .castId = castId,
                                        .cast = std::move(*castValue),
                                    };
                                    if (auto saved = m_store.putCast(*state.cast); !saved) {
                                        state.metadata = failedMetadata(std::move(saved.error()), true);
                                        publish(control, observer, std::move(state));
                                        return;
                                    }
                                    if (author) {
                                        static_cast<void>(m_store.putProfile(*author));
                                        state.author = std::move(*author);
                                    } else {
                                        auto cachedAuthor = m_store.findProfile(state.cast->author);
                                        if (cachedAuthor) {
                                            state.author = std::move(*cachedAuthor);
                                        }
                                    }
                                    state.metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready};
                                    publish(control, observer, std::move(state));
                                });
                            static_cast<void>(status);
                        },
                        options);
                    if (!authorRequest) {
                        auto status = m_cacheExecutor.post(
                            [this, castId, castValue, observer, control]() mutable {
                                model::CastState state{
                                    .castId = castId,
                                    .cast = std::move(*castValue),
                                };
                                static_cast<void>(m_store.putCast(*state.cast));
                                auto cachedAuthor = m_store.findProfile(state.cast->author);
                                if (cachedAuthor) {
                                    state.author = std::move(*cachedAuthor);
                                }
                                state.metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready};
                                publish(control, observer, std::move(state));
                            });
                        static_cast<void>(status);
                        return;
                    }
                    control->attach(std::move(*authorRequest));
                },
                options);
            if (!request) {
                model::CastState failed{.castId = castId};
                auto cachedAgain = m_store.findCast(castId);
                if (cachedAgain) {
                    failed.cast = std::move(*cachedAgain);
                }
                failed.metadata = failedMetadata(
                    std::move(request.error()), failed.cast.has_value());
                publish(control, observer, std::move(failed));
                return;
            }
            control->attach(std::move(*request));
        });
    if (!initialStatus) {
        return std::unexpected(std::move(initialStatus.error()));
    }
    return ReadRequest{control};
}

foundation::result::Result<ReadRequest> ReadService::openFeed(
    model::FeedQuery query,
    FeedObserver observer,
    const model::ReadOptions options)
{
    if (query.limit == 0U || !observer ||
        ((query.kind == model::FeedQuery::Kind::Following ||
          query.kind == model::FeedQuery::Kind::UserPosts ||
          query.kind == model::FeedQuery::Kind::UserQuotes ||
          query.kind == model::FeedQuery::Kind::UserReplies ||
          query.kind == model::FeedQuery::Kind::UserLikes ||
          query.kind == model::FeedQuery::Kind::UserPopular) &&
         (!query.fid || query.fid->value == 0U)) ||
        (query.kind == model::FeedQuery::Kind::Channel &&
         (!query.channel || query.channel->value.empty()))) {
        return std::unexpected(Error::make(
            ErrorCode::InvalidArgument,
            "Feed selector, limit and observer must be valid."));
    }
    auto control = std::make_shared<ReadRequest::Control>();
    auto initialStatus = m_cacheExecutor.post(
        [this, query = std::move(query), observer = std::move(observer), options, control]() mutable {
            const auto localKey = toLocalKey(query);
            model::FeedState state{.query = query};
            auto cached = m_store.readFeed(localKey);
            if (cached) {
                state.casts = std::move(cached->casts);
                state.profiles = std::move(cached->profiles);
                state.entries = std::move(cached->entries);
                state.next = std::move(cached->next);
                state.metadata = refreshingMetadata(!state.entries.empty());
            } else {
                state.metadata = failedMetadata(std::move(cached.error()), false);
            }
            publish(control, observer, std::move(state));

            auto request = m_gateway.feed(
                query,
                [this, query, localKey, observer, control](
                    foundation::result::Result<capability::feed::FeedPage> fresh) mutable {
                    auto status = m_cacheExecutor.post(
                        [this, query, localKey, observer, control,
                         fresh = std::move(fresh)]() mutable {
                            if (control->cancelled.load(std::memory_order_acquire)) {
                                return;
                            }
                            model::FeedState state{.query = query};
                            if (!fresh) {
                                auto cached = m_store.readFeed(localKey);
                                if (cached) {
                                    state.casts = std::move(cached->casts);
                                    state.profiles = std::move(cached->profiles);
                                    state.entries = std::move(cached->entries);
                                    state.next = std::move(cached->next);
                                }
                                state.metadata = failedMetadata(
                                    std::move(fresh.error()), !state.entries.empty());
                                publish(control, observer, std::move(state));
                                return;
                            }
                            auto saved = m_store.writeFeed(
                                localKey, *fresh, query.continuation.has_value());
                            if (!saved) {
                                state.casts = std::move(fresh->casts);
                                state.profiles = std::move(fresh->profiles);
                                state.entries = std::move(fresh->entries);
                                state.next = std::move(fresh->next);
                                state.metadata = failedMetadata(std::move(saved.error()), !state.entries.empty());
                                publish(control, observer, std::move(state));
                                return;
                            }
                            auto materialized = m_store.readFeed(localKey);
                            if (!materialized) {
                                state.metadata = failedMetadata(std::move(materialized.error()), false);
                                publish(control, observer, std::move(state));
                                return;
                            }
                            state.casts = std::move(materialized->casts);
                            state.profiles = std::move(materialized->profiles);
                            state.entries = std::move(materialized->entries);
                            state.next = std::move(materialized->next);
                            state.metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready};
                            publish(control, observer, std::move(state));
                        });
                    static_cast<void>(status);
                },
                options);
            if (!request) {
                model::FeedState failed{.query = query};
                auto cachedAgain = m_store.readFeed(localKey);
                if (cachedAgain) {
                    failed.casts = std::move(cachedAgain->casts);
                    failed.profiles = std::move(cachedAgain->profiles);
                    failed.entries = std::move(cachedAgain->entries);
                    failed.next = std::move(cachedAgain->next);
                }
                failed.metadata = failedMetadata(
                    std::move(request.error()), !failed.entries.empty());
                publish(control, observer, std::move(failed));
                return;
            }
            control->attach(std::move(*request));
        });
    if (!initialStatus) {
        return std::unexpected(std::move(initialStatus.error()));
    }
    return ReadRequest{control};
}

foundation::result::Result<ReadRequest> ReadService::openSearch(
    model::SearchQuery query,
    SearchObserver observer,
    const model::ReadOptions options)
{
    if (query.text.empty() || query.limit == 0U || query.limit > 30U || !observer) {
        return std::unexpected(Error::make(
            ErrorCode::InvalidArgument,
            "Search query, bounded limit and observer are required."));
    }
    auto control = std::make_shared<ReadRequest::Control>();
    auto initialStatus = m_cacheExecutor.post(
        [this, query = std::move(query), observer = std::move(observer), options, control]() mutable {
            publish(control, observer, model::SearchState{
                .query = query,
                .page = {},
                .metadata = refreshingMetadata(false),
            });
            auto request = m_gateway.search(
                query,
                [query, observer, control](foundation::result::Result<capability::search::SearchPage> fresh) mutable {
                    model::SearchState state{.query = query};
                    if (!fresh) {
                        state.metadata = failedMetadata(std::move(fresh.error()), false);
                    } else {
                        state.page = std::move(*fresh);
                        state.metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready};
                    }
                    publish(control, observer, std::move(state));
                },
                options);
            if (!request) {
                model::SearchState state{.query = query};
                state.metadata = failedMetadata(std::move(request.error()), false);
                publish(control, observer, std::move(state));
                return;
            }
            control->attach(std::move(*request));
        });
    if (!initialStatus) {
        return std::unexpected(std::move(initialStatus.error()));
    }
    return ReadRequest{control};
}

foundation::result::Result<ReadRequest> ReadService::openNotifications(
    model::NotificationQuery query,
    NotificationObserver observer,
    const model::ReadOptions options)
{
    if (query.viewer.value == 0U || query.limit == 0U || query.limit > 100U || !observer) {
        return std::unexpected(Error::make(
            ErrorCode::InvalidArgument,
            "Notification viewer, bounded limit and observer are required."));
    }
    auto control = std::make_shared<ReadRequest::Control>();
    auto initialStatus = m_cacheExecutor.post(
        [this, query = std::move(query), observer = std::move(observer), options, control]() mutable {
            publish(control, observer, model::NotificationState{
                .query = query,
                .items = {},
                .next = {},
                .metadata = refreshingMetadata(false),
            });
            auto request = m_gateway.notifications(
                query,
                [query, observer, control](
                    foundation::result::Result<capability::notification::NotificationPage> fresh) mutable {
                    model::NotificationState state{.query = query};
                    if (!fresh) {
                        state.metadata = failedMetadata(std::move(fresh.error()), false);
                    } else {
                        state.items = std::move(fresh->items);
                        state.next = std::move(fresh->next);
                        state.metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready};
                    }
                    publish(control, observer, std::move(state));
                },
                options);
            if (!request) {
                model::NotificationState state{.query = query};
                state.metadata = failedMetadata(std::move(request.error()), false);
                publish(control, observer, std::move(state));
                return;
            }
            control->attach(std::move(*request));
        });
    if (!initialStatus) {
        return std::unexpected(std::move(initialStatus.error()));
    }
    return ReadRequest{control};
}

foundation::result::Result<ReadRequest> ReadService::openSocial(
    model::SocialQuery query,
    SocialObserver observer,
    const model::ReadOptions options)
{
    if (query.fid.value == 0U || query.limit == 0U || !observer) {
        return std::unexpected(Error::make(
            ErrorCode::InvalidArgument,
            "Social selector, FID, limit and observer are required."));
    }
    auto control = std::make_shared<ReadRequest::Control>();
    auto initialStatus = m_cacheExecutor.post(
        [this, query = std::move(query), observer = std::move(observer), options, control]() mutable {
            const auto localKey = toLocalKey(query);
            model::SocialState state{.query = query};
            auto cached = m_store.readSocial(localKey);
            if (cached) {
                state.profiles = std::move(cached->profiles);
                state.relations = std::move(cached->relations);
                state.next = std::move(cached->next);
                state.metadata = refreshingMetadata(!state.profiles.empty());
            } else {
                state.metadata = failedMetadata(std::move(cached.error()), false);
            }
            publish(control, observer, std::move(state));

            auto request = m_gateway.social(
                query,
                [this, query, localKey, observer, control](
                    foundation::result::Result<capability::social::SocialGraphPage> fresh) mutable {
                    auto status = m_cacheExecutor.post(
                        [this, query, localKey, observer, control,
                         fresh = std::move(fresh)]() mutable {
                            model::SocialState state{.query = query};
                            if (!fresh) {
                                auto cached = m_store.readSocial(localKey);
                                if (cached) {
                                    state.profiles = std::move(cached->profiles);
                                    state.relations = std::move(cached->relations);
                                    state.next = std::move(cached->next);
                                }
                                state.metadata = failedMetadata(
                                    std::move(fresh.error()), !state.profiles.empty());
                                publish(control, observer, std::move(state));
                                return;
                            }
                            auto saved = m_store.writeSocial(
                                localKey, *fresh, query.continuation.has_value());
                            if (!saved) {
                                state.profiles = std::move(fresh->profiles);
                                state.relations = std::move(fresh->relations);
                                state.next = std::move(fresh->next);
                                state.metadata = failedMetadata(
                                    std::move(saved.error()), !state.profiles.empty());
                                publish(control, observer, std::move(state));
                                return;
                            }
                            auto materialized = m_store.readSocial(localKey);
                            if (!materialized) {
                                state.metadata = failedMetadata(std::move(materialized.error()), false);
                                publish(control, observer, std::move(state));
                                return;
                            }
                            state.profiles = std::move(materialized->profiles);
                            state.relations = std::move(materialized->relations);
                            state.next = std::move(materialized->next);
                            state.metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready};
                            publish(control, observer, std::move(state));
                        });
                    static_cast<void>(status);
                },
                options);
            if (!request) {
                model::SocialState failed{.query = query};
                auto cachedAgain = m_store.readSocial(localKey);
                if (cachedAgain) {
                    failed.profiles = std::move(cachedAgain->profiles);
                    failed.relations = std::move(cachedAgain->relations);
                    failed.next = std::move(cachedAgain->next);
                }
                failed.metadata = failedMetadata(
                    std::move(request.error()), !failed.profiles.empty());
                publish(control, observer, std::move(failed));
                return;
            }
            control->attach(std::move(*request));
        });
    if (!initialStatus) {
        return std::unexpected(std::move(initialStatus.error()));
    }
    return ReadRequest{control};
}

}
