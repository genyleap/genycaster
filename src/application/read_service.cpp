module;

#include <atomic>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

module farcaster.application.read.service;

import farcaster.application.executor;
import farcaster.application.read.gateway;
import farcaster.application.read.model;
import farcaster.capability.feed;
import farcaster.capability.search;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.local.read;

namespace farcaster::application::read::service {
namespace {

using foundation::error::Error;
using foundation::error::ErrorCode;

[[nodiscard]] bool isSourceUnavailableFailure(const ErrorCode code) noexcept
{
    return code == ErrorCode::Unavailable || code == ErrorCode::Timeout ||
        code == ErrorCode::TransportFailure;
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
    const bool sourceUnavailable = isSourceUnavailableFailure(error.code);
    return model::ReadMetadata{
        .phase = model::ReadPhase::Failed,
        .isStale = hasUsableData,
        .isSourceUnavailable = sourceUnavailable,
        .error = std::move(error),
    };
}

[[nodiscard]] local::read::FeedKey toLocalKey(const model::FeedQuery& query)
{
    local::read::FeedKey::Kind kind = local::read::FeedKey::Kind::Trending;
    if (query.kind == model::FeedQuery::Kind::Following) {
        kind = local::read::FeedKey::Kind::Following;
    } else if (query.kind == model::FeedQuery::Kind::Channel) {
        kind = local::read::FeedKey::Kind::Channel;
    } else if (query.kind == model::FeedQuery::Kind::UserPosts) {
        kind = local::read::FeedKey::Kind::UserPosts;
    } else if (query.kind == model::FeedQuery::Kind::UserQuotes) {
        kind = local::read::FeedKey::Kind::UserQuotes;
    } else if (query.kind == model::FeedQuery::Kind::UserReplies) {
        kind = local::read::FeedKey::Kind::UserReplies;
    } else if (query.kind == model::FeedQuery::Kind::UserLikes) {
        kind = local::read::FeedKey::Kind::UserLikes;
    } else if (query.kind == model::FeedQuery::Kind::UserPopular) {
        kind = local::read::FeedKey::Kind::UserPopular;
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

template <typename Observer, typename State>
void publish(
    const std::shared_ptr<std::atomic_bool>& cancelled,
    Observer& observer,
    State state)
{
    if (!cancelled->load(std::memory_order_acquire)) {
        observer(std::move(state));
    }
}

[[nodiscard]] foundation::result::Result<ReadRequest> postRequest(
    executor::IExecutor& executor,
    executor::Job job,
    const std::shared_ptr<std::atomic_bool>& cancelled)
{
    auto status = executor.post(std::move(job));
    if (!status) {
        return std::unexpected(std::move(status.error()));
    }
    return ReadRequest{cancelled};
}

}

ReadRequest::ReadRequest(std::shared_ptr<std::atomic_bool> cancelled) noexcept
    : m_cancelled(std::move(cancelled))
{
}

void ReadRequest::cancel() noexcept
{
    if (m_cancelled) {
        m_cancelled->store(true, std::memory_order_release);
    }
}

bool ReadRequest::isCancelled() const noexcept
{
    return m_cancelled && m_cancelled->load(std::memory_order_acquire);
}

ReadService::ReadService(
    executor::IExecutor& executor,
    local::read::IReadStore& store,
    gateway::IReadGateway& gateway) noexcept
    : ReadService(executor, executor, store, gateway)
{
}

ReadService::ReadService(
    executor::IExecutor& cacheExecutor,
    executor::IExecutor& refreshExecutor,
    local::read::IReadStore& store,
    gateway::IReadGateway& gateway) noexcept
    : m_cacheExecutor(cacheExecutor),
      m_refreshExecutor(refreshExecutor),
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
            ErrorCode::InvalidArgument, "Identity username and observer are required."));
    }
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    return postRequest(m_cacheExecutor,
        [this, username = std::move(username), observer = std::move(observer), options, cancelled]() mutable {
            if (cancelled->load(std::memory_order_acquire)) { return; }
            model::IdentityState state{.username = username};
            auto cached = m_store.findProfileByUsername(username);
            if (cached) {
                state.profile = std::move(*cached);
                if (state.profile) { state.fid = state.profile->fid; }
                state.metadata = refreshingMetadata(state.profile.has_value());
            } else {
                state.metadata = failedMetadata(std::move(cached.error()), false);
            }
            publish(cancelled, observer, state);

            auto refreshState = state;
            auto refreshObserver = observer;
            auto status = m_refreshExecutor.post(
                [this, username, observer = std::move(refreshObserver), options, cancelled,
                 state = std::move(refreshState)]() mutable {
                    if (cancelled->load(std::memory_order_acquire)) { return; }
                    auto fid = m_gateway.resolveIdentity(username, options);
                    if (!fid) {
                        state.metadata = failedMetadata(std::move(fid.error()), state.profile.has_value());
                        publish(cancelled, observer, std::move(state));
                        return;
                    }
                    state.fid = *fid;
                    auto profile = m_gateway.profile(*fid, options);
                    if (!profile) {
                        state.metadata = failedMetadata(std::move(profile.error()), state.profile.has_value());
                        publish(cancelled, observer, std::move(state));
                        return;
                    }
                    if (auto saved = m_store.putProfile(*profile); !saved) {
                        state.metadata = failedMetadata(std::move(saved.error()), state.profile.has_value());
                        publish(cancelled, observer, std::move(state));
                        return;
                    }
                    state.profile = std::move(*profile);
                    state.metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready};
                    publish(cancelled, observer, std::move(state));
                });
            if (!status) {
                state.metadata = failedMetadata(std::move(status.error()), state.profile.has_value());
                publish(cancelled, observer, std::move(state));
            }
        }, cancelled);
}

foundation::result::Result<ReadRequest> ReadService::openProfile(
    const domain::identity::Fid fid,
    ProfileObserver observer,
    const model::ReadOptions options)
{
    if (fid.value == 0 || !observer) {
        return std::unexpected(Error::make(ErrorCode::InvalidArgument, "Profile FID and observer are required."));
    }
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    return postRequest(m_cacheExecutor,
        [this, fid, observer = std::move(observer), options, cancelled]() mutable {
            if (cancelled->load(std::memory_order_acquire)) { return; }
            model::ProfileState state{.fid = fid};
            auto cached = m_store.findProfile(fid);
            if (cached) {
                state.profile = std::move(*cached);
                state.metadata = refreshingMetadata(state.profile.has_value());
            } else {
                state.metadata = failedMetadata(std::move(cached.error()), false);
            }
            publish(cancelled, observer, state);

            auto refreshState = state;
            auto refreshObserver = observer;
            auto status = m_refreshExecutor.post(
                [this, fid, observer = std::move(refreshObserver), options, cancelled,
                 state = std::move(refreshState)]() mutable {
                    if (cancelled->load(std::memory_order_acquire)) { return; }
                    auto fresh = m_gateway.profile(fid, options);
                    if (!fresh) {
                        state.metadata = failedMetadata(std::move(fresh.error()), state.profile.has_value());
                        publish(cancelled, observer, std::move(state));
                        return;
                    }
                    if (auto saved = m_store.putProfile(*fresh); !saved) {
                        state.metadata = failedMetadata(std::move(saved.error()), state.profile.has_value());
                        publish(cancelled, observer, std::move(state));
                        return;
                    }
                    state.profile = std::move(*fresh);
                    state.metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready};
                    publish(cancelled, observer, std::move(state));
                });
            if (!status) {
                state.metadata = failedMetadata(std::move(status.error()), state.profile.has_value());
                publish(cancelled, observer, std::move(state));
            }
        }, cancelled);
}

foundation::result::Result<ReadRequest> ReadService::openCast(
    domain::cast::CastId castId,
    CastObserver observer,
    const model::ReadOptions options)
{
    if (castId.value.empty() || !observer) {
        return std::unexpected(Error::make(ErrorCode::InvalidArgument, "Cast id and observer are required."));
    }
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    return postRequest(m_cacheExecutor,
        [this, castId = std::move(castId), observer = std::move(observer), options, cancelled]() mutable {
            if (cancelled->load(std::memory_order_acquire)) { return; }
            model::CastState state{.castId = castId};
            auto cached = m_store.findCast(castId);
            if (cached) {
                state.cast = std::move(*cached);
                if (state.cast) {
                    auto author = m_store.findProfile(state.cast->author);
                    if (author) { state.author = std::move(*author); }
                }
                state.metadata = refreshingMetadata(state.cast.has_value());
            } else {
                state.metadata = failedMetadata(std::move(cached.error()), false);
            }
            publish(cancelled, observer, state);

            auto refreshState = state;
            auto refreshObserver = observer;
            auto status = m_refreshExecutor.post(
                [this, castId, observer = std::move(refreshObserver), options, cancelled,
                 state = std::move(refreshState)]() mutable {
                    if (cancelled->load(std::memory_order_acquire)) { return; }
                    auto fresh = m_gateway.cast(castId, options);
                    if (!fresh) {
                        state.metadata = failedMetadata(std::move(fresh.error()), state.cast.has_value());
                        publish(cancelled, observer, std::move(state));
                        return;
                    }
                    if (auto saved = m_store.putCast(*fresh); !saved) {
                        state.metadata = failedMetadata(std::move(saved.error()), state.cast.has_value());
                        publish(cancelled, observer, std::move(state));
                        return;
                    }
                    state.cast = std::move(*fresh);
                    auto author = m_gateway.profile(state.cast->author);
                    if (author) {
                        static_cast<void>(m_store.putProfile(*author));
                        state.author = std::move(*author);
                    } else {
                        auto cachedAuthor = m_store.findProfile(state.cast->author);
                        if (cachedAuthor) { state.author = std::move(*cachedAuthor); }
                    }
                    state.metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready};
                    publish(cancelled, observer, std::move(state));
                });
            if (!status) {
                state.metadata = failedMetadata(std::move(status.error()), state.cast.has_value());
                publish(cancelled, observer, std::move(state));
            }
        }, cancelled);
}

foundation::result::Result<ReadRequest> ReadService::openFeed(
    model::FeedQuery query,
    FeedObserver observer,
    const model::ReadOptions options)
{
    if (query.limit == 0 || !observer ||
        ((query.kind == model::FeedQuery::Kind::Following ||
          query.kind == model::FeedQuery::Kind::UserPosts ||
          query.kind == model::FeedQuery::Kind::UserQuotes ||
          query.kind == model::FeedQuery::Kind::UserReplies ||
          query.kind == model::FeedQuery::Kind::UserLikes ||
          query.kind == model::FeedQuery::Kind::UserPopular) && (!query.fid || query.fid->value == 0)) ||
        (query.kind == model::FeedQuery::Kind::Channel && (!query.channel || query.channel->value.empty()))) {
        return std::unexpected(Error::make(ErrorCode::InvalidArgument, "Feed selector, limit and observer must be valid."));
    }
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    return postRequest(m_cacheExecutor,
        [this, query = std::move(query), observer = std::move(observer), options, cancelled]() mutable {
            if (cancelled->load(std::memory_order_acquire)) { return; }
            model::FeedState state{.query = query};
            const auto localKey = toLocalKey(query);
            auto cached = m_store.readFeed(localKey);
            if (cached) {
                state.casts = cached->casts;
                state.profiles = cached->profiles;
                state.entries = cached->entries;
                state.next = cached->next;
                state.metadata = refreshingMetadata(!state.entries.empty());
            } else {
                state.metadata = failedMetadata(std::move(cached.error()), false);
            }
            publish(cancelled, observer, state);

            auto refreshState = state;
            auto refreshObserver = observer;
            auto status = m_refreshExecutor.post(
                [this, query, localKey, observer = std::move(refreshObserver), options, cancelled,
                 state = std::move(refreshState)]() mutable {
                    if (cancelled->load(std::memory_order_acquire)) { return; }
                    auto fresh = m_gateway.feed(query, options);
                    if (!fresh) {
                        state.metadata = failedMetadata(std::move(fresh.error()), !state.entries.empty());
                        publish(cancelled, observer, std::move(state));
                        return;
                    }
                    if (auto saved = m_store.writeFeed(localKey, *fresh, query.continuation.has_value()); !saved) {
                        state.metadata = failedMetadata(std::move(saved.error()), !state.entries.empty());
                        publish(cancelled, observer, std::move(state));
                        return;
                    }
                    auto materialized = m_store.readFeed(localKey);
                    if (!materialized) {
                        state.metadata = failedMetadata(std::move(materialized.error()), !state.entries.empty());
                        publish(cancelled, observer, std::move(state));
                        return;
                    }
                    state.casts = std::move(materialized->casts);
                    state.profiles = std::move(materialized->profiles);
                    state.entries = std::move(materialized->entries);
                    state.next = std::move(materialized->next);
                    state.metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready};
                    publish(cancelled, observer, std::move(state));
                });
            if (!status) {
                state.metadata = failedMetadata(std::move(status.error()), !state.entries.empty());
                publish(cancelled, observer, std::move(state));
            }
        }, cancelled);
}

foundation::result::Result<ReadRequest> ReadService::openSearch(
    model::SearchQuery query,
    SearchObserver observer,
    const model::ReadOptions options)
{
    if (query.text.empty() || query.limit == 0 || query.limit > 30 || !observer) {
        return std::unexpected(Error::make(
            ErrorCode::InvalidArgument, "Search query, bounded limit and observer are required."));
    }
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    return postRequest(m_refreshExecutor,
        [this, query = std::move(query), observer = std::move(observer), options, cancelled]() mutable {
            if (cancelled->load(std::memory_order_acquire)) { return; }
            model::SearchState state{
                .query = query,
                .page = {},
                .metadata = refreshingMetadata(false),
            };
            publish(cancelled, observer, state);
            auto fresh = m_gateway.search(query, options);
            if (!fresh) {
                state.metadata = failedMetadata(std::move(fresh.error()), false);
                publish(cancelled, observer, std::move(state));
                return;
            }
            state.page = std::move(*fresh);
            state.metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready};
            publish(cancelled, observer, std::move(state));
        }, cancelled);
}

foundation::result::Result<ReadRequest> ReadService::openSocial(
    model::SocialQuery query,
    SocialObserver observer,
    const model::ReadOptions options)
{
    if (query.fid.value == 0 || query.limit == 0 || !observer) {
        return std::unexpected(Error::make(ErrorCode::InvalidArgument, "Social selector, FID, limit and observer are required."));
    }
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    return postRequest(m_cacheExecutor,
        [this, query = std::move(query), observer = std::move(observer), options, cancelled]() mutable {
            if (cancelled->load(std::memory_order_acquire)) { return; }
            model::SocialState state{.query = query};
            const auto localKey = toLocalKey(query);
            auto cached = m_store.readSocial(localKey);
            if (cached) {
                state.profiles = cached->profiles;
                state.relations = cached->relations;
                state.next = cached->next;
                state.metadata = refreshingMetadata(!state.profiles.empty());
            } else {
                state.metadata = failedMetadata(std::move(cached.error()), false);
            }
            publish(cancelled, observer, state);

            auto refreshState = state;
            auto refreshObserver = observer;
            auto status = m_refreshExecutor.post(
                [this, query, localKey, observer = std::move(refreshObserver), options, cancelled,
                 state = std::move(refreshState)]() mutable {
                    if (cancelled->load(std::memory_order_acquire)) { return; }
                    auto fresh = m_gateway.social(query, options);
                    if (!fresh) {
                        state.metadata = failedMetadata(std::move(fresh.error()), !state.profiles.empty());
                        publish(cancelled, observer, std::move(state));
                        return;
                    }
                    if (auto saved = m_store.writeSocial(localKey, *fresh, query.continuation.has_value()); !saved) {
                        state.metadata = failedMetadata(std::move(saved.error()), !state.profiles.empty());
                        publish(cancelled, observer, std::move(state));
                        return;
                    }
                    auto materialized = m_store.readSocial(localKey);
                    if (!materialized) {
                        state.metadata = failedMetadata(std::move(materialized.error()), !state.profiles.empty());
                        publish(cancelled, observer, std::move(state));
                        return;
                    }
                    state.profiles = std::move(materialized->profiles);
                    state.relations = std::move(materialized->relations);
                    state.next = std::move(materialized->next);
                    state.metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready};
                    publish(cancelled, observer, std::move(state));
                });
            if (!status) {
                state.metadata = failedMetadata(std::move(status.error()), !state.profiles.empty());
                publish(cancelled, observer, std::move(state));
            }
        }, cancelled);
}

}
