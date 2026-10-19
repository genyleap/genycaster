module;

#include <atomic>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

module farcaster.application.read.service;

import farcaster.application.executor;
import farcaster.application.read.gateway;
import farcaster.application.read.model;
import farcaster.capability.feed;
import farcaster.capability.search;
import farcaster.capability.notification;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.domain.feed;
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
    if (query.kind == model::FeedQuery::Kind::Nfts) {
        kind = local::read::FeedKey::Kind::Nfts;
    } else if (query.kind == model::FeedQuery::Kind::Developers) {
        kind = local::read::FeedKey::Kind::Developers;
    } else if (query.kind == model::FeedQuery::Kind::Recent) {
        kind = local::read::FeedKey::Kind::Recent;
    } else if (query.kind == model::FeedQuery::Kind::Following) {
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

void seedFeedState(model::FeedState& state, const capability::feed::FeedPage& page)
{
    state.casts = page.casts;
    state.profiles = page.profiles;
    state.entries = page.entries;
    state.next = page.next;
}

void mergeFeedPage(model::FeedState& state, const capability::feed::FeedPage& page)
{
    std::unordered_map<std::string, std::size_t> castIndex{};
    castIndex.reserve(state.casts.size() + page.casts.size());
    for (std::size_t index = 0; index < state.casts.size(); ++index) {
        castIndex.insert_or_assign(state.casts[index].id.value, index);
    }
    for (const auto& cast : page.casts) {
        if (const auto found = castIndex.find(cast.id.value); found != castIndex.end()) {
            state.casts[found->second] = cast;
        } else {
            castIndex.emplace(cast.id.value, state.casts.size());
            state.casts.push_back(cast);
        }
    }

    std::unordered_map<std::uint64_t, std::size_t> profileIndex{};
    profileIndex.reserve(state.profiles.size() + page.profiles.size());
    for (std::size_t index = 0; index < state.profiles.size(); ++index) {
        profileIndex.insert_or_assign(state.profiles[index].fid.value, index);
    }
    for (const auto& profile : page.profiles) {
        if (const auto found = profileIndex.find(profile.fid.value); found != profileIndex.end()) {
            state.profiles[found->second] = profile;
        } else {
            profileIndex.emplace(profile.fid.value, state.profiles.size());
            state.profiles.push_back(profile);
        }
    }

    std::unordered_set<std::string> entryIds{};
    entryIds.reserve(state.entries.size() + page.entries.size());
    for (const auto& entry : state.entries) {
        entryIds.insert(entry.castId.value);
    }
    for (const auto& entry : page.entries) {
        if (entryIds.insert(entry.castId.value).second) {
            state.entries.push_back(entry);
        }
    }
    state.next = page.next;
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
            // A continuation request is already represented by the current UI
            // snapshot. Re-emitting the same cache before page N+1 arrives causes
            // unnecessary delegate reconciliation and can temporarily discard
            // presentation-only enrichment. Keep the cache for fallback/merge but
            // publish it immediately only for the first page.
            if (!query.continuation) {
                publish(cancelled, observer, state);
            }

            auto refreshState = state;
            auto refreshObserver = observer;
            auto status = m_refreshExecutor.post(
                [this, query, localKey, observer = std::move(refreshObserver), options, cancelled,
                 state = std::move(refreshState)]() mutable {
                    if (cancelled->load(std::memory_order_acquire)) { return; }
                    auto fresh = m_gateway.feed(query, options);
                    if (!fresh) {
                        if (query.continuation
                            && options.intent == model::ReadIntent::Prefetch
                            && !state.entries.empty()) {
                            state.next.reset();
                            state.metadata = model::ReadMetadata{
                                .phase = model::ReadPhase::Ready,
                            };
                            publish(cancelled, observer, std::move(state));
                            return;
                        }
                        state.metadata = failedMetadata(std::move(fresh.error()), !state.entries.empty());
                        publish(cancelled, observer, std::move(state));
                        return;
                    }
                    model::FeedState fallback = state;
                    if (query.continuation) {
                        mergeFeedPage(fallback, *fresh);
                    } else {
                        seedFeedState(fallback, *fresh);
                    }
                    if (auto saved = m_store.writeFeed(localKey, *fresh, query.continuation.has_value()); !saved) {
                        fallback.metadata = failedMetadata(std::move(saved.error()), !fallback.entries.empty());
                        publish(cancelled, observer, std::move(fallback));
                        return;
                    }
                    auto materialized = m_store.readFeed(localKey);
                    if (!materialized) {
                        fallback.metadata = failedMetadata(std::move(materialized.error()), !fallback.entries.empty());
                        publish(cancelled, observer, std::move(fallback));
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

foundation::result::Result<ReadRequest> ReadService::openNotifications(
    model::NotificationQuery query,
    NotificationObserver observer,
    const model::ReadOptions options)
{
    if (query.viewer.value == 0 || query.limit == 0 || query.limit > 100 || !observer) {
        return std::unexpected(Error::make(
            ErrorCode::InvalidArgument, "Notification viewer, bounded limit and observer are required."));
    }
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    return postRequest(m_refreshExecutor,
        [this, query = std::move(query), observer = std::move(observer), options, cancelled]() mutable {
            if (cancelled->load(std::memory_order_acquire)) { return; }
            model::NotificationState state{
                .query = query,
                .items = {},
                .next = {},
                .metadata = refreshingMetadata(false),
            };
            publish(cancelled, observer, state);
            auto fresh = m_gateway.notifications(query, options);
            if (!fresh) {
                state.metadata = failedMetadata(std::move(fresh.error()), false);
                publish(cancelled, observer, std::move(state));
                return;
            }
            state.items = std::move(fresh->items);
            state.next = std::move(fresh->next);
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
