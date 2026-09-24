#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

import farcaster.application.executor;
import farcaster.application.read.gateway;
import farcaster.application.read.model;
import farcaster.application.read.service;
import farcaster.capability.feed;
import farcaster.capability.pagination;
import farcaster.capability.search;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.channel;
import farcaster.domain.feed;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.domain.social;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.execution.serial;
import farcaster.local.read;
import farcaster.storage.read;

namespace {

namespace application = farcaster::application;
namespace capability = farcaster::capability;
namespace domain = farcaster::domain;
namespace foundation = farcaster::foundation;

class ImmediateExecutor final : public application::executor::IExecutor {
public:
    [[nodiscard]] foundation::result::Status post(application::executor::Job job) override
    {
        job();
        return {};
    }
};

struct DatabaseFixture final {
    DatabaseFixture()
    {
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
            ("genycaster-read-pipeline-" + std::to_string(unique) + ".sqlite3");
    }
    ~DatabaseFixture()
    {
        std::error_code error{};
        std::filesystem::remove(path, error);
        std::filesystem::remove(path.string() + "-wal", error);
        std::filesystem::remove(path.string() + "-shm", error);
    }
    std::filesystem::path path{};
};

[[nodiscard]] domain::profile::Profile makeProfile(
    const std::uint64_t fid,
    std::string username,
    std::string displayName)
{
    return domain::profile::Profile{
        .fid = domain::identity::Fid{fid},
        .username = domain::identity::Username{std::move(username)},
        .displayName = domain::identity::DisplayName{std::move(displayName)},
        .bio = "Local-first profile",
    };
}

[[nodiscard]] domain::cast::Cast makeCast(
    std::string id,
    const std::uint64_t author,
    std::string text,
    const std::int64_t milliseconds)
{
    return domain::cast::Cast{
        .id = domain::cast::CastId{std::move(id)},
        .author = domain::identity::Fid{author},
        .text = std::move(text),
        .createdAt = foundation::time::Timestamp{std::chrono::milliseconds{milliseconds}},
    };
}

[[nodiscard]] capability::feed::FeedPage pageFor(
    domain::profile::Profile author,
    domain::cast::Cast item,
    std::optional<capability::pagination::ContinuationToken> next = std::nullopt)
{
    const auto id = item.id;
    const auto time = item.createdAt;
    return capability::feed::FeedPage{
        .casts = {std::move(item)},
        .profiles = {std::move(author)},
        .entries = {domain::feed::FeedEntry{.castId = id, .sortTime = time}},
        .next = std::move(next),
    };
}

class FakeGateway final : public application::read::gateway::IReadGateway {
public:
    std::function<std::size_t()> observedStateCount{};
    std::size_t expectedObservedStateCount{1};
    bool unavailable{false};
    capability::feed::FeedPage feedPage{};
    capability::search::SearchPage searchPage{};
    capability::social::SocialGraphPage socialPage{};
    domain::profile::Profile profileValue{makeProfile(1, "alice", "Alice")};
    domain::cast::Cast castValue{makeCast("0xcast", 1, "hello", 1'000)};
    std::optional<std::string> expectedContinuation{};
    std::optional<application::read::model::ReadIntent> expectedIntent{};

    [[nodiscard]] foundation::result::Result<domain::identity::Fid>
    resolveIdentity(std::string_view, application::read::model::ReadOptions) override
    {
        return failOr(profileValue.fid);
    }

    [[nodiscard]] foundation::result::Result<domain::profile::Profile>
    profile(domain::identity::Fid, application::read::model::ReadOptions) override
    {
        return failOr(profileValue);
    }

    [[nodiscard]] foundation::result::Result<domain::cast::Cast>
    cast(const domain::cast::CastId&, application::read::model::ReadOptions) override
    {
        return failOr(castValue);
    }

    [[nodiscard]] foundation::result::Result<capability::feed::FeedPage>
    feed(const application::read::model::FeedQuery& query,
        const application::read::model::ReadOptions options) override
    {
        if (observedStateCount) {
            assert(observedStateCount() == expectedObservedStateCount);
        }
        if (expectedContinuation) {
            assert(query.continuation);
            assert(query.continuation->value == *expectedContinuation);
        }
        if (expectedIntent) {
            assert(options.intent == *expectedIntent);
        }
        return failOr(feedPage);
    }

    [[nodiscard]] foundation::result::Result<capability::search::SearchPage>
    search(const application::read::model::SearchQuery&,
        application::read::model::ReadOptions) override
    {
        return failOr(searchPage);
    }

    [[nodiscard]] foundation::result::Result<capability::social::SocialGraphPage>
    social(const application::read::model::SocialQuery&,
        application::read::model::ReadOptions) override
    {
        return failOr(socialPage);
    }

private:
    template <typename T>
    [[nodiscard]] foundation::result::Result<T> failOr(const T& value) const
    {
        if (unavailable) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable, "Deterministic source-unavailable fixture."));
        }
        return value;
    }
};

void cachedFeedPrecedesRefreshAndPaginationReconciles()
{
    DatabaseFixture database{};
    auto store = farcaster::storage::read::openReadStore(database.path);
    assert(store);
    const auto cachedPage = pageFor(
        makeProfile(1, "alice", "Alice"), makeCast("0xcached", 1, "cached", 1'000),
        capability::pagination::ContinuationToken{"cached-next"});
    const farcaster::local::read::FeedKey key{
        .kind = farcaster::local::read::FeedKey::Kind::Trending,
        .limit = 1,
    };
    assert((*store)->writeFeed(key, cachedPage, false));

    ImmediateExecutor executor{};
    FakeGateway gateway{};
    gateway.feedPage = pageFor(
        makeProfile(2, "bob", "Bob"), makeCast("0xfresh", 2, "fresh", 2'000),
        capability::pagination::ContinuationToken{"next-safe"});
    std::vector<application::read::model::FeedState> states{};
    gateway.observedStateCount = [&states] { return states.size(); };
    application::read::service::ReadService service{executor, **store, gateway};
    auto request = service.openFeed(
        application::read::model::FeedQuery{
            .kind = application::read::model::FeedQuery::Kind::Trending,
            .limit = 1,
        },
        [&states](auto state) { states.push_back(std::move(state)); });
    assert(request);
    assert(states.size() == 2);
    assert(states.front().metadata.phase == application::read::model::ReadPhase::Refreshing);
    assert(states.front().entries.front().castId.value == "0xcached");
    assert(states.back().metadata.phase == application::read::model::ReadPhase::Ready);
    assert(states.back().entries.front().castId.value == "0xfresh");
    assert(states.back().next && states.back().next->value == "next-safe");

    states.clear();
    gateway.observedStateCount = [&states] { return states.size(); };
    gateway.expectedObservedStateCount = 0;
    gateway.expectedContinuation = "next-safe";
    gateway.feedPage = pageFor(
        makeProfile(3, "carol", "Carol"), makeCast("0xpage2", 3, "page two", 3'000));
    request = service.openFeed(
        application::read::model::FeedQuery{
            .kind = application::read::model::FeedQuery::Kind::Trending,
            .limit = 1,
            .continuation = capability::pagination::ContinuationToken{"next-safe"},
        },
        [&states](auto state) { states.push_back(std::move(state)); });
    assert(request);
    // Continuation reads keep the already-visible cache internal and publish only
    // the reconciled terminal timeline; page-start must not republish page N.
    assert(states.size() == 1);
    assert(states.back().entries.size() == 2);
    assert(states.back().entries[0].castId.value == "0xfresh");
    assert(states.back().entries[1].castId.value == "0xpage2");

    // Network page size must not truncate the accumulated local materialization.
    // With the historical bug this read returned only the first row even though
    // the continuation had already advanced beyond the second provider page.
    const auto materialized = (*store)->readFeed(key);
    assert(materialized);
    assert(materialized->entries.size() == 2);
    assert(materialized->entries[0].castId.value == "0xfresh");
    assert(materialized->entries[1].castId.value == "0xpage2");
}

void prefetchContinuationFailurePreservesUsableFeed()
{
    DatabaseFixture database{};
    auto store = farcaster::storage::read::openReadStore(database.path);
    assert(store);
    ImmediateExecutor executor{};
    FakeGateway gateway{};
    gateway.feedPage = pageFor(
        makeProfile(20, "prefetch", "Prefetch"),
        makeCast("0xpersisted-page", 20, "usable page", 20'000),
        capability::pagination::ContinuationToken{"prefetch-next"});
    application::read::service::ReadService service{executor, **store, gateway};

    std::vector<application::read::model::FeedState> states{};
    assert(service.openFeed(
        application::read::model::FeedQuery{
            .kind = application::read::model::FeedQuery::Kind::Trending,
            .limit = 1,
        },
        [&states](auto state) { states.push_back(std::move(state)); }));
    assert(states.size() == 2);
    assert(states.back().metadata.phase == application::read::model::ReadPhase::Ready);
    assert(states.back().next && states.back().next->value == "prefetch-next");

    states.clear();
    gateway.unavailable = true;
    gateway.expectedObservedStateCount = 0;
    gateway.observedStateCount = [&states] { return states.size(); };
    gateway.expectedContinuation = "prefetch-next";
    gateway.expectedIntent = application::read::model::ReadIntent::Prefetch;
    assert(service.openFeed(
        application::read::model::FeedQuery{
            .kind = application::read::model::FeedQuery::Kind::Trending,
            .limit = 1,
            .continuation = capability::pagination::ContinuationToken{"prefetch-next"},
        },
        [&states](auto state) { states.push_back(std::move(state)); },
        application::read::model::ReadOptions{
            .intent = application::read::model::ReadIntent::Prefetch,
        }));

    assert(states.size() == 1);
    assert(states.front().entries.size() == 1);
    assert(states.front().entries.front().castId.value == "0xpersisted-page");
    assert(states.front().metadata.phase == application::read::model::ReadPhase::Ready);
    assert(!states.front().metadata.isSourceUnavailable);
    assert(!states.front().metadata.error);
    assert(!states.front().next);
}

void refreshFailureWithoutCacheIsNotMarkedStale()
{
    DatabaseFixture database{};
    auto store = farcaster::storage::read::openReadStore(database.path);
    assert(store);
    ImmediateExecutor executor{};
    FakeGateway gateway{};
    gateway.unavailable = true;
    application::read::service::ReadService service{executor, **store, gateway};
    std::vector<application::read::model::FeedState> states{};
    assert(service.openFeed(
        application::read::model::FeedQuery{.kind = application::read::model::FeedQuery::Kind::Trending},
        [&states](auto state) { states.push_back(std::move(state)); }));
    assert(states.size() == 2);
    assert(states.front().entries.empty());
    assert(states.back().entries.empty());
    assert(!states.back().metadata.isStale);
    assert(states.back().metadata.isSourceUnavailable);
    assert(states.back().metadata.error);
}

void refreshFailurePreservesCacheAcrossRestart()
{
    DatabaseFixture database{};
    {
        auto store = farcaster::storage::read::openReadStore(database.path);
        assert(store);
        ImmediateExecutor executor{};
        FakeGateway gateway{};
        gateway.feedPage = pageFor(
            makeProfile(10, "restart", "Restart"),
            makeCast("0xpersisted", 10, "survives restart", 10'000));
        application::read::service::ReadService service{executor, **store, gateway};
        std::vector<application::read::model::FeedState> states{};
        assert(service.openFeed(
            application::read::model::FeedQuery{.kind = application::read::model::FeedQuery::Kind::Trending},
            [&states](auto state) { states.push_back(std::move(state)); }));
        assert(states.back().entries.size() == 1);
    }
    {
        auto store = farcaster::storage::read::openReadStore(database.path);
        assert(store);
        ImmediateExecutor executor{};
        FakeGateway gateway{};
        gateway.unavailable = true;
        application::read::service::ReadService service{executor, **store, gateway};
        std::vector<application::read::model::FeedState> states{};
        assert(service.openFeed(
            application::read::model::FeedQuery{.kind = application::read::model::FeedQuery::Kind::Trending},
            [&states](auto state) { states.push_back(std::move(state)); }));
        assert(states.size() == 2);
        assert(states.front().entries.front().castId.value == "0xpersisted");
        assert(states.back().entries.front().castId.value == "0xpersisted");
        assert(states.back().metadata.isStale);
        assert(states.back().metadata.isSourceUnavailable);
        assert(states.back().metadata.error);
        auto persistedProfile = (*store)->findProfile(domain::identity::Fid{10});
        auto persistedCast = (*store)->findCast(domain::cast::CastId{"0xpersisted"});
        assert(persistedProfile && *persistedProfile);
        assert(persistedCast && *persistedCast);

        gateway.unavailable = false;
        gateway.feedPage = pageFor(
            makeProfile(11, "returned", "Returned"),
            makeCast("0xreturned", 11, "network returned", 11'000));
        states.clear();
        assert(service.openFeed(
            application::read::model::FeedQuery{.kind = application::read::model::FeedQuery::Kind::Trending},
            [&states](auto state) { states.push_back(std::move(state)); }));
        assert(states.front().entries.front().castId.value == "0xpersisted");
        assert(states.back().metadata.phase == application::read::model::ReadPhase::Ready);
        assert(states.back().entries.front().castId.value == "0xreturned");
    }
}

void interactiveSearchPublishesRefreshingThenReady()
{
    DatabaseFixture database{};
    auto store = farcaster::storage::read::openReadStore(database.path);
    assert(store);
    ImmediateExecutor executor{};
    FakeGateway gateway{};
    gateway.searchPage = capability::search::SearchPage{
        .profiles = {makeProfile(100, "searcher", "Searcher")},
        .channels = {capability::search::ChannelSuggestion{
            .id = domain::channel::ChannelId{"dev"},
            .name = "Developers",
            .description = "Developer discussion",
            .followerCount = 1'024,
        }},
    };
    application::read::service::ReadService service{executor, **store, gateway};

    std::vector<application::read::model::SearchState> states{};
    assert(service.openSearch(
        application::read::model::SearchQuery{.text = "dev", .limit = 6},
        [&states](auto state) { states.push_back(std::move(state)); }));

    assert(states.size() == 2);
    assert(states.front().metadata.phase == application::read::model::ReadPhase::Refreshing);
    assert(states.front().page.profiles.empty());
    assert(states.front().page.channels.empty());
    assert(states.back().metadata.phase == application::read::model::ReadPhase::Ready);
    assert(states.back().query.text == "dev");
    assert(states.back().page.profiles.size() == 1);
    assert(states.back().page.profiles.front().fid.value == 100);
    assert(states.back().page.channels.size() == 1);
    assert(states.back().page.channels.front().id.value == "dev");
}

void identityCastProfileAndSocialFlowsPersistCanonicalData()
{
    DatabaseFixture database{};
    auto store = farcaster::storage::read::openReadStore(database.path);
    assert(store);
    ImmediateExecutor executor{};
    FakeGateway gateway{};
    gateway.profileValue = makeProfile(42, "phase4", "Phase Four");
    gateway.castValue = makeCast("0xdetail", 42, "detail", 42'000);
    gateway.socialPage = capability::social::SocialGraphPage{
        .profiles = {makeProfile(7, "follower", "Follower")},
        .relations = {domain::social::SocialRelation{
            .source = domain::identity::Fid{7},
            .target = domain::identity::Fid{42},
            .kind = domain::social::SocialRelationKind::Follows,
        }},
    };
    application::read::service::ReadService service{executor, **store, gateway};

    std::vector<application::read::model::IdentityState> identities{};
    assert(service.openIdentity("phase4", [&identities](auto state) {
        identities.push_back(std::move(state));
    }));
    assert(identities.back().profile && identities.back().profile->fid.value == 42);

    std::vector<application::read::model::ProfileState> profiles{};
    assert(service.openProfile(domain::identity::Fid{42}, [&profiles](auto state) {
        profiles.push_back(std::move(state));
    }));
    assert(profiles.front().profile);

    std::vector<application::read::model::CastState> casts{};
    assert(service.openCast(domain::cast::CastId{"0xdetail"}, [&casts](auto state) {
        casts.push_back(std::move(state));
    }));
    assert(casts.back().cast && casts.back().author);

    std::vector<application::read::model::SocialState> social{};
    assert(service.openSocial(
        application::read::model::SocialQuery{
            .kind = application::read::model::SocialQuery::Kind::Followers,
            .fid = domain::identity::Fid{42},
        },
        [&social](auto state) { social.push_back(std::move(state)); }));
    assert(social.back().relations.size() == 1);
    assert(social.back().profiles.size() == 1);
}


void profileFeedCachesRemainIsolated()
{
    using namespace farcaster;

    DatabaseFixture database{};
    auto store = storage::read::openReadStore(database.path);
    assert(store);

    const domain::identity::Fid fid{42};
    const auto posts = pageFor(
        makeProfile(42, "alice", "Alice"),
        makeCast("0xpost", 42, "post", 10'000));
    const auto replies = pageFor(
        makeProfile(42, "alice", "Alice"),
        makeCast("0xreply", 42, "reply", 11'000));
    const auto popular = pageFor(
        makeProfile(42, "alice", "Alice"),
        makeCast("0xpopular", 42, "popular", 12'000));

    const local::read::FeedKey postsKey{
        .kind = local::read::FeedKey::Kind::UserPosts,
        .fid = fid,
    };
    const local::read::FeedKey repliesKey{
        .kind = local::read::FeedKey::Kind::UserReplies,
        .fid = fid,
    };
    const local::read::FeedKey popularKey{
        .kind = local::read::FeedKey::Kind::UserPopular,
        .fid = fid,
    };

    assert((*store)->writeFeed(postsKey, posts, false));
    assert((*store)->writeFeed(repliesKey, replies, false));
    assert((*store)->writeFeed(popularKey, popular, false));

    const auto cachedPosts = (*store)->readFeed(postsKey);
    const auto cachedReplies = (*store)->readFeed(repliesKey);
    const auto cachedPopular = (*store)->readFeed(popularKey);
    assert(cachedPosts && cachedPosts->entries.size() == 1);
    assert(cachedReplies && cachedReplies->entries.size() == 1);
    assert(cachedPopular && cachedPopular->entries.size() == 1);
    assert(cachedPosts->entries.front().castId.value == "0xpost");
    assert(cachedReplies->entries.front().castId.value == "0xreply");
    assert(cachedPopular->entries.front().castId.value == "0xpopular");
}

void embeddedFeedProfilesDoNotRegressAuthoritativeProfileMetadata()
{
    using namespace farcaster;

    DatabaseFixture database{};
    auto store = storage::read::openReadStore(database.path);
    assert(store);

    domain::profile::Profile authoritative = makeProfile(321969, "compez.eth", "COMPEZ");
    authoritative.bio = "Canonical profile";
    authoritative.avatarUrl = "https://cdn.test/compez.png";
    authoritative.bannerUrl = "https://cdn.test/compez-banner.png";
    authoritative.location = "Earth";
    authoritative.profileUrl = "https://example.test/compez";
    authoritative.followerCount = 18'000;
    authoritative.followingCount = 454;
    assert((*store)->putProfile(authoritative));

    domain::profile::Profile staleSummary = makeProfile(321969, "compez.eth", "COMPEZ");
    staleSummary.followerCount = 88'000;
    staleSummary.followingCount = 212;
    const auto page = pageFor(
        std::move(staleSummary),
        makeCast("0xsummary", 321969, "summary", 15'000));
    const local::read::FeedKey key{
        .kind = local::read::FeedKey::Kind::UserPosts,
        .fid = domain::identity::Fid{321969},
    };
    assert((*store)->writeFeed(key, page, false));

    const auto persisted = (*store)->findProfile(domain::identity::Fid{321969});
    assert(persisted && *persisted);
    assert((*persisted)->followerCount == 18'000);
    assert((*persisted)->followingCount == 454);
    assert((*persisted)->avatarUrl == authoritative.avatarUrl);
    assert((*persisted)->bannerUrl == authoritative.bannerUrl);
    assert((*persisted)->location == authoritative.location);
    assert((*persisted)->profileUrl == authoritative.profileUrl);
    assert((*persisted)->bio == authoritative.bio);
}

void serializedExecutorKeepsBlockingReadOffCallerThread()
{
    DatabaseFixture database{};
    auto store = farcaster::storage::read::openReadStore(database.path);
    assert(store);
    FakeGateway gateway{};
    gateway.feedPage = pageFor(
        makeProfile(99, "worker", "Worker"),
        makeCast("0xworker", 99, "off caller", 99'000));
    farcaster::execution::serial::SerialExecutor executor{};
    application::read::service::ReadService service{executor, **store, gateway};
    const std::thread::id caller = std::this_thread::get_id();
    std::thread::id observerThread{};
    std::mutex mutex{};
    std::condition_variable condition{};
    bool ready{false};
    assert(service.openFeed(
        application::read::model::FeedQuery{.kind = application::read::model::FeedQuery::Kind::Trending},
        [&](auto state) {
            if (state.metadata.phase != application::read::model::ReadPhase::Ready) {
                return;
            }
            {
                std::scoped_lock lock{mutex};
                observerThread = std::this_thread::get_id();
                ready = true;
            }
            condition.notify_one();
        }));
    std::unique_lock lock{mutex};
    const bool completed = condition.wait_for(lock, std::chrono::seconds{5}, [&ready] { return ready; });
    assert(completed);
    assert(observerThread != caller);
}

}

int main()
{
    cachedFeedPrecedesRefreshAndPaginationReconciles();
    prefetchContinuationFailurePreservesUsableFeed();
    refreshFailureWithoutCacheIsNotMarkedStale();
    refreshFailurePreservesCacheAcrossRestart();
    interactiveSearchPublishesRefreshingThenReady();
    identityCastProfileAndSocialFlowsPersistCanonicalData();
    profileFeedCachesRemainIsolated();
    embeddedFeedProfilesDoNotRegressAuthoritativeProfileMetadata();
    serializedExecutorKeepsBlockingReadOffCallerThread();
    return 0;
}
