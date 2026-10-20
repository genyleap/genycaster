module;

#include <atomic>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <unordered_map>
#include <unordered_set>
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
import farcaster.domain.feed;
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
    case model::FeedQuery::Kind::Recent:
        kind = local::read::FeedKey::Kind::Recent;
        break;
    case model::FeedQuery::Kind::Conversations:
        kind = local::read::FeedKey::Kind::Conversations;
        break;
    case model::FeedQuery::Kind::Nfts:
        kind = local::read::FeedKey::Kind::Nfts;
        break;
    case model::FeedQuery::Kind::Developers:
        kind = local::read::FeedKey::Kind::Developers;
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
            continue;
        }
        castIndex.emplace(cast.id.value, state.casts.size());
        state.casts.push_back(cast);
    }

    std::unordered_map<std::uint64_t, std::size_t> profileIndex{};
    profileIndex.reserve(state.profiles.size() + page.profiles.size());
    for (std::size_t index = 0; index < state.profiles.size(); ++index) {
        profileIndex.insert_or_assign(state.profiles[index].fid.value, index);
    }
    for (const auto& profile : page.profiles) {
        if (const auto found = profileIndex.find(profile.fid.value); found != profileIndex.end()) {
            state.profiles[found->second] = profile;
            continue;
        }
        profileIndex.emplace(profile.fid.value, state.profiles.size());
        state.profiles.push_back(profile);
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

[[nodiscard]] model::FeedState feedFallbackState(
    const model::FeedQuery& query,
    const std::optional<capability::feed::FeedPage>& existing,
    const capability::feed::FeedPage& fresh)
{
    model::FeedState state{.query = query};
    if (query.continuation && existing) {
        seedFeedState(state, *existing);
        mergeFeedPage(state, fresh);
    } else {
        seedFeedState(state, fresh);
    }
    return state;
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

// A rejected cache queue job cannot silently strand the UI in Refreshing.
// Fail on the completing worker without attempting SQLite access while the
// serialized cache executor is saturated or shutting down.
template <typename Control, typename Observer, typename State>
void publishRejectedCacheJob(
    const std::shared_ptr<Control>& control,
    Observer& observer,
    State state,
    foundation::result::Status status)
{
    if (!status) {
        state.metadata = failedMetadata(std::move(status.error()), false);
        publish(control, observer, std::move(state));
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
                        publishRejectedCacheJob(control, observer,
                            model::IdentityState{.username = username}, std::move(status));
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
                            publishRejectedCacheJob(control, observer,
                                model::IdentityState{.username = username, .fid = resolvedFid},
                                std::move(status));
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
                        publishRejectedCacheJob(control, observer,
                            model::IdentityState{.username = username, .fid = resolvedFid},
                            std::move(status));
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
                    publishRejectedCacheJob(control, observer,
                        model::ProfileState{.fid = fid}, std::move(status));
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
                        publishRejectedCacheJob(control, observer,
                            model::CastState{.castId = castId}, std::move(status));
                        return;
                    }

                    auto castValue = std::make_shared<domain::cast::Cast>(std::move(*fresh));
                    // Canonical cast content is the primary read. Publish it
                    // independently of the optional author network round trip.
                    // Cached author information is useful immediately.
                    auto primaryStatus = m_cacheExecutor.post(
                        [this, castId, castValue, observer, control]() mutable {
                            if (control->cancelled.load(std::memory_order_acquire)) {
                                return;
                            }
                            model::CastState state{
                                .castId = castId,
                                .cast = *castValue,
                            };
                            auto saved = m_store.putCast(*state.cast);
                            if (auto author = m_store.findProfile(state.cast->author); author) {
                                state.author = std::move(*author);
                            }
                            state.metadata = saved
                                ? model::ReadMetadata{.phase = model::ReadPhase::Ready}
                                : failedMetadata(std::move(saved.error()), true);
                            publish(control, observer, std::move(state));
                        });
                    if (!primaryStatus) {
                        model::CastState failed{.castId = castId};
                        failed.metadata = failedMetadata(std::move(primaryStatus.error()), false);
                        publish(control, observer, std::move(failed));
                        return;
                    }

                    if (options.intent == model::ReadIntent::Prefetch) {
                        return;
                    }

                    // Enrichment is separately owned by the same cancellation
                    // token but can only publish metadata after a successful
                    // primary snapshot. Failure must not replace valid content.
                    auto authorRequest = m_gateway.profile(
                        castValue->author,
                        [this, castId, observer, control](
                            foundation::result::Result<domain::profile::Profile> author) mutable {
                            if (!author || control->cancelled.load(std::memory_order_acquire)) {
                                return;
                            }
                            auto status = m_cacheExecutor.post(
                                [this, castId, observer, control,
                                 author = std::move(*author)]() mutable {
                                    if (control->cancelled.load(std::memory_order_acquire)) {
                                        return;
                                    }
                                    if (auto saved = m_store.putProfile(author); !saved) {
                                        return;
                                    }
                                    auto cachedCast = m_store.findCast(castId);
                                    if (!cachedCast || !cachedCast->has_value()) {
                                        return;
                                    }
                                    model::CastState state{
                                        .castId = castId,
                                        .cast = std::move(**cachedCast),
                                        .author = std::move(author),
                                        .metadata = model::ReadMetadata{.phase = model::ReadPhase::Ready},
                                    };
                                    publish(control, observer, std::move(state));
                                });
                            // Optional work can be dropped when the cache lane
                            // is saturated; never fail the primary cast read.
                            static_cast<void>(status);
                        },
                        options);
                    if (authorRequest) {
                        control->attach(std::move(*authorRequest));
                    }
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
            // Continuation already has a stable accumulated presentation in
            // the caller. Avoid materializing the entire SQLite timeline only
            // to discard it before issuing the next provider page.
            if (!query.continuation) {
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
            }

            auto request = m_gateway.feed(
                query,
                [this, query, localKey, observer, options, control](
                    foundation::result::Result<capability::feed::FeedPage> fresh) mutable {
                    auto status = m_cacheExecutor.post(
                        [this, query, localKey, observer, options, control,
                         fresh = std::move(fresh)]() mutable {
                            if (control->cancelled.load(std::memory_order_acquire)) {
                                return;
                            }
                            model::FeedState state{.query = query};
                            if (!fresh) {
                                if (query.continuation && options.pageOnlyContinuation) {
                                    // The presentation already owns the prior pages. A
                                    // rejected/failed continuation must not trigger an
                                    // O(entire accumulated feed) SQLite recovery read.
                                    // Empty deltas preserve the earlier visible rows.
                                    state.next.reset();
                                    if (options.intent == model::ReadIntent::Prefetch) {
                                        state.metadata = model::ReadMetadata{
                                            .phase = model::ReadPhase::Ready,
                                        };
                                    } else {
                                        state.metadata = failedMetadata(
                                            std::move(fresh.error()), true);
                                    }
                                    publish(control, observer, std::move(state));
                                    return;
                                }
                                auto cached = m_store.readFeed(localKey);
                                if (cached) {
                                    state.casts = std::move(cached->casts);
                                    state.profiles = std::move(cached->profiles);
                                    state.entries = std::move(cached->entries);
                                    state.next = std::move(cached->next);
                                }
                                if (query.continuation
                                    && options.intent == model::ReadIntent::Prefetch
                                    && !state.entries.empty()) {
                                    // Background pagination is opportunistic. Preserve the
                                    // already-usable materialized feed and stop this local
                                    // continuation chain instead of promoting a 429/5xx into
                                    // a page-wide Source issue. A later first-page refresh can
                                    // establish a fresh continuation token.
                                    state.next.reset();
                                    state.metadata = model::ReadMetadata{
                                        .phase = model::ReadPhase::Ready,
                                    };
                                    publish(control, observer, std::move(state));
                                    return;
                                }
                                state.metadata = failedMetadata(
                                    std::move(fresh.error()), !state.entries.empty());
                                publish(control, observer, std::move(state));
                                return;
                            }
                            // In the normal continuation path there is exactly one
                            // post-append materialization, not three full reads.
                            // Only construct the expensive offline fallback if
                            // storage really fails.
                            const auto failedFallback = [this, &query, &localKey, &fresh]() {
                                std::optional<capability::feed::FeedPage> existing{};
                                if (query.continuation) {
                                    if (auto cached = m_store.readFeed(localKey); cached) {
                                        existing = std::move(*cached);
                                    }
                                }
                                auto fallback = feedFallbackState(query, existing, *fresh);
                                fallback.metadata.communityFallback = fresh->communityFallback;
                                return fallback;
                            };
                            auto saved = m_store.writeFeed(
                                localKey, *fresh, query.continuation.has_value());
                            if (!saved) {
                                model::FeedState fallback = failedFallback();
                                fallback.metadata = failedMetadata(
                                    std::move(saved.error()), !fallback.entries.empty());
                                publish(control, observer, std::move(fallback));
                                return;
                            }
                            if (query.continuation && options.pageOnlyContinuation) {
                                // The Qt presentation already retains the earlier pages
                                // by stable cast ID. Publishing only the newly persisted
                                // page prevents an O(total-feed-length) SQLite scan,
                                // profile lookup and QVariant rebuild on every append.
                                // The provider page contains the canonical entries and
                                // hydration required for this visible delta. Offline
                                // restarts still call readFeed() for a full warm snapshot.
                                state.casts = std::move(fresh->casts);
                                state.profiles = std::move(fresh->profiles);
                                state.entries = std::move(fresh->entries);
                                state.next = std::move(fresh->next);
                                state.metadata = model::ReadMetadata{
                                    .phase = model::ReadPhase::Ready,
                                    .communityFallback = fresh->communityFallback,
                                };
                                publish(control, observer, std::move(state));
                                return;
                            }
                            auto materialized = m_store.readFeed(localKey);
                            if (!materialized) {
                                model::FeedState fallback = failedFallback();
                                fallback.metadata = failedMetadata(
                                    std::move(materialized.error()), !fallback.entries.empty());
                                publish(control, observer, std::move(fallback));
                                return;
                            }
                            state.casts = std::move(materialized->casts);
                            state.profiles = std::move(materialized->profiles);
                            state.entries = std::move(materialized->entries);
                            state.next = std::move(materialized->next);
                            state.metadata = model::ReadMetadata{
                                .phase = model::ReadPhase::Ready,
                                .communityFallback = fresh->communityFallback,
                            };
                            publish(control, observer, std::move(state));
                        });
                    publishRejectedCacheJob(control, observer,
                        model::FeedState{.query = query}, std::move(status));
                },
                options);
            if (!request) {
                model::FeedState failed{.query = query};
                if (query.continuation && options.pageOnlyContinuation) {
                    failed.next.reset();
                    if (options.intent == model::ReadIntent::Prefetch) {
                        failed.metadata = model::ReadMetadata{
                            .phase = model::ReadPhase::Ready,
                        };
                    } else {
                        failed.metadata = failedMetadata(
                            std::move(request.error()), true);
                    }
                    publish(control, observer, std::move(failed));
                    return;
                }
                auto cachedAgain = m_store.readFeed(localKey);
                if (cachedAgain) {
                    failed.casts = std::move(cachedAgain->casts);
                    failed.profiles = std::move(cachedAgain->profiles);
                    failed.entries = std::move(cachedAgain->entries);
                    failed.next = std::move(cachedAgain->next);
                }
                if (query.continuation
                    && options.intent == model::ReadIntent::Prefetch
                    && !failed.entries.empty()) {
                    failed.next.reset();
                    failed.metadata = model::ReadMetadata{
                        .phase = model::ReadPhase::Ready,
                    };
                } else {
                    failed.metadata = failedMetadata(
                        std::move(request.error()), !failed.entries.empty());
                }
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
                    publishRejectedCacheJob(control, observer,
                        model::SocialState{.query = query}, std::move(status));
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
