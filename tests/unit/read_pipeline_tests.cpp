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
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <sqlite3.h>

import farcaster.application.executor;
import farcaster.application.read.async_gateway;
import farcaster.application.read.async_service;
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

// The first cache job is admitted. The completion job is deterministically
// rejected, emulating a bounded serial queue under sustained pressure.
class RejectCompletionExecutor final : public application::executor::IExecutor {
public:
    int jobs{};
    [[nodiscard]] foundation::result::Status post(application::executor::Job job) override
    {
        if (++jobs == 1) {
            job();
            return {};
        }
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::RateLimited,
            "Test cache queue admission refused."));
    }
};

class FakeAsyncCastGateway final
    : public application::read::async_gateway::IAsyncReadGateway {
public:
    using Request = application::read::async_gateway::Request;
    using Options = application::read::model::ReadOptions;
    bool failCast{true};
    bool holdProfile{};
    bool includeReply{};
    bool failFeedCompletion{};
    application::read::async_gateway::ProfileCompletion pendingProfile{};
    int profileCalls{};
    std::optional<capability::feed::FeedPage> servedFeed{};

    [[nodiscard]] foundation::result::Result<Request> resolveIdentity(
        std::string_view,
        application::read::async_gateway::IdentityCompletion,
        Options) override
    {
        return unsupported();
    }
    [[nodiscard]] foundation::result::Result<Request> profile(
        domain::identity::Fid,
        application::read::async_gateway::ProfileCompletion completion,
        Options) override
    {
        ++profileCalls;
        if (holdProfile) {
            pendingProfile = std::move(completion);
            return Request{};
        }
        return unsupported();
    }
    [[nodiscard]] foundation::result::Result<Request> cast(
        domain::cast::CastId castId,
        application::read::async_gateway::CastCompletion completion,
        Options) override
    {
        if (failCast) {
            completion(std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable, "Test cast source unavailable.")));
        } else {
            domain::cast::Cast cast{
                .id = std::move(castId),
                .author = domain::identity::Fid{42},
                .text = "Cached background preview",
            };
            if (includeReply) {
                cast.directReplies.push_back(domain::cast::Cast{
                    .id = domain::cast::CastId{"0xreply-fresh"},
                    .author = domain::identity::Fid{16},
                    .text = "Fresh reply before optional author",
                    .parent = cast.id,
                });
            }
            completion(std::move(cast));
        }
        return Request{};
    }
    [[nodiscard]] foundation::result::Result<Request> feed(
        application::read::model::FeedQuery,
        application::read::async_gateway::FeedCompletion completion,
        Options) override
    {
        if (failFeedCompletion) {
            completion(std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "Delayed continuation failed at the transport.")));
            return Request{};
        }
        if (!servedFeed) {
            return unsupported();
        }
        completion(*servedFeed);
        return Request{};
    }
    [[nodiscard]] foundation::result::Result<Request> search(
        application::read::model::SearchQuery,
        application::read::async_gateway::SearchCompletion,
        Options) override
    {
        return unsupported();
    }
    [[nodiscard]] foundation::result::Result<Request> notifications(
        application::read::model::NotificationQuery,
        application::read::async_gateway::NotificationCompletion,
        Options) override
    {
        return unsupported();
    }
    [[nodiscard]] foundation::result::Result<Request> social(
        application::read::model::SocialQuery,
        application::read::async_gateway::SocialCompletion,
        Options) override
    {
        return unsupported();
    }

private:
    [[nodiscard]] static foundation::result::Result<Request> unsupported()
    {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Unsupported,
            "Other read capabilities not enabled in test."));
    }
};

// Counts timeline-wide reads independently of the provider's single-page
// response. A Qt page delta must persist durably without re-materializing all
// earlier pages through the SQLite local-first port.
class CountingReadStore final : public farcaster::local::read::IReadStore {
public:
    explicit CountingReadStore(
        std::unique_ptr<farcaster::local::read::IReadStore> delegate)
        : m_delegate{std::move(delegate)} {}

    int feedMaterializations{};

    [[nodiscard]] foundation::result::Result<std::optional<domain::profile::Profile>>
    findProfileByUsername(std::string_view value) override {
        return m_delegate->findProfileByUsername(value);
    }
    [[nodiscard]] foundation::result::Result<std::optional<domain::profile::Profile>>
    findProfile(domain::identity::Fid fid) override {
        return m_delegate->findProfile(fid);
    }
    [[nodiscard]] foundation::result::Result<std::optional<domain::cast::Cast>>
    findCast(const domain::cast::CastId& id) override {
        return m_delegate->findCast(id);
    }
    [[nodiscard]] foundation::result::Status
    putProfile(const domain::profile::Profile& value) override {
        return m_delegate->putProfile(value);
    }
    [[nodiscard]] foundation::result::Status
    putCast(const domain::cast::Cast& value) override {
        return m_delegate->putCast(value);
    }
    [[nodiscard]] foundation::result::Result<capability::feed::FeedPage>
    readFeed(const farcaster::local::read::FeedKey& key) override {
        ++feedMaterializations;
        return m_delegate->readFeed(key);
    }
    [[nodiscard]] foundation::result::Result<farcaster::local::read::FeedWindow>
    readFeedWindow(const farcaster::local::read::FeedKey& key,
                   std::size_t offset, std::size_t limit) override {
        return m_delegate->readFeedWindow(key, offset, limit);
    }
    [[nodiscard]] foundation::result::Status
    writeFeed(const farcaster::local::read::FeedKey& key,
              const capability::feed::FeedPage& page, bool append) override {
        return m_delegate->writeFeed(key, page, append);
    }
    [[nodiscard]] foundation::result::Result<capability::social::SocialGraphPage>
    readSocial(const farcaster::local::read::SocialKey& key) override {
        return m_delegate->readSocial(key);
    }
    [[nodiscard]] foundation::result::Status
    writeSocial(const farcaster::local::read::SocialKey& key,
                const capability::social::SocialGraphPage& page, bool append) override {
        return m_delegate->writeSocial(key, page, append);
    }

private:
    std::unique_ptr<farcaster::local::read::IReadStore> m_delegate{};
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

void largeLocalFeedWindowPreservesOrderAndRemoteContinuation()
{
    using namespace farcaster;
    DatabaseFixture database{};
    auto store = storage::read::openReadStore(database.path);
    assert(store);
    const local::read::FeedKey key{
        .kind = local::read::FeedKey::Kind::Recent,
        .limit = 20U,
    };
    capability::feed::FeedPage source{};
    source.profiles.push_back(makeProfile(91, "window-reader", "Window Reader"));
    constexpr std::size_t count{640U};
    source.entries.reserve(count);
    source.casts.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        auto cast = makeCast(
            "0xwindow-" + std::to_string(index), 91,
            "Bounded cached window", static_cast<std::int64_t>(index + 1000U));
        source.entries.push_back(domain::feed::FeedEntry{
            .castId = cast.id,
            .sortTime = cast.createdAt,
        });
        source.casts.push_back(std::move(cast));
    }
    source.next = capability::pagination::ContinuationToken{"provider-next-only"};
    assert((*store)->writeFeed(key, source, false));

    // Network page size is independent of local-window size.
    auto first = (*store)->readFeedWindow(key, 0U, 64U);
    assert(first && first->hasMore);
    assert(first->page.entries.size() == 64U);
    assert(first->page.casts.size() == 64U);
    assert(first->page.profiles.size() == 1U);
    assert(first->page.entries.front().castId.value == "0xwindow-0");
    assert(first->page.entries.back().castId.value == "0xwindow-63");
    assert(first->page.next && first->page.next->value == "provider-next-only");

    auto middle = (*store)->readFeedWindow(key, 64U, 64U);
    assert(middle && middle->hasMore);
    assert(middle->page.entries.size() == 64U);
    assert(middle->page.entries.front().castId.value == "0xwindow-64");
    assert(middle->page.entries.back().castId.value == "0xwindow-127");
    assert(middle->page.next && middle->page.next->value == "provider-next-only");

    auto last = (*store)->readFeedWindow(key, 576U, 64U);
    assert(last && !last->hasMore && last->page.entries.size() == 64U);
    assert(last->page.entries.back().castId.value == "0xwindow-639");
    auto beyond = (*store)->readFeedWindow(key, 640U, 64U);
    assert(beyond && !beyond->hasMore && beyond->page.entries.empty());
    assert(beyond->page.next && beyond->page.next->value == "provider-next-only");

    assert(!(*store)->readFeedWindow(key, 0U, 0U));
    assert(!(*store)->readFeedWindow(key, 0U, 257U));
    // Original callers still obtain the complete accumulated offline snapshot.
    auto original = (*store)->readFeed(key);
    assert(original && original->entries.size() == count);
    assert(original->entries.front().castId.value == "0xwindow-0");
    assert(original->entries.back().castId.value == "0xwindow-639");

    (*store).reset();
    auto reopened = storage::read::openReadStore(database.path);
    assert(reopened);
    auto resumed = (*reopened)->readFeedWindow(key, 128U, 64U);
    assert(resumed && resumed->hasMore);
    assert(resumed->page.entries.front().castId.value == "0xwindow-128");
    assert(resumed->page.entries.back().castId.value == "0xwindow-191");

    // Tombstones are excluded before OFFSET, not after materializing a page;
    // a locally removed cast must never create a gap or duplicate on scroll.
    sqlite3* db{};
    assert(sqlite3_open(database.path.string().c_str(), &db) == SQLITE_OK);
    const char* tombstoneSql =
        "INSERT INTO cast_local_tombstones("
        "cast_id, idempotency_key, author_fid, created_at_ms) "
        "VALUES('0xwindow-2','local-delete-window-2',91,1002);";
    assert(sqlite3_exec(db, tombstoneSql, nullptr, nullptr, nullptr) == SQLITE_OK);
    assert(sqlite3_close(db) == SQLITE_OK);
    auto filtered = (*reopened)->readFeedWindow(key, 1U, 4U);
    assert(filtered && filtered->hasMore);
    assert(filtered->page.entries.size() == 4U);
    assert(filtered->page.entries[0].castId.value == "0xwindow-1");
    assert(filtered->page.entries[1].castId.value == "0xwindow-3");
    assert(filtered->page.entries[2].castId.value == "0xwindow-4");
    assert(filtered->page.entries[3].castId.value == "0xwindow-5");
    auto filteredLast = (*reopened)->readFeedWindow(key, 638U, 2U);
    assert(filteredLast && !filteredLast->hasMore);
    assert(filteredLast->page.entries.size() == 1U);
    assert(filteredLast->page.entries.back().castId.value == "0xwindow-639");
}

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

void normalizedConversationSurvivesRestartAndOfflineRead()
{
    using namespace farcaster;
    DatabaseFixture database{};
    domain::cast::Cast root = makeCast("0xthread-root", 42, "Root cast", 1000);
    root.engagement.replies = 3;
    domain::cast::Cast first = makeCast("0xreply-first", 11, "First", 1100);
    first.parent = root.id;
    first.authorProfile = makeProfile(11, "reply-author", "Reply Author");
    domain::cast::Cast nested = makeCast("0xreply-nested", 12, "Nested", 1200);
    nested.parent = first.id;
    first.directReplies.push_back(std::move(nested));
    domain::cast::Cast second = makeCast("0xreply-second", 13, "Second", 1300);
    second.parent = root.id;
    root.directReplies.push_back(std::move(first));
    root.directReplies.push_back(std::move(second));
    {
        auto store = storage::read::openReadStore(database.path);
        assert(store && (*store)->putCast(root));
        auto stored = (*store)->findCast(root.id);
        assert(stored && *stored);
        assert((*stored)->directReplies.size() == 2);
        assert((*stored)->directReplies[0].id.value == "0xreply-first");
        assert((*stored)->directReplies[0].directReplies.size() == 1);
        assert((*stored)->directReplies[0].directReplies[0].id.value == "0xreply-nested");
        // A plain feed delta carries only the root, and must not discard a
        // separately hydrated, complete conversation already in local state.
        auto shallow = root;
        shallow.directReplies.clear();
        shallow.engagement.likes = 5;
        assert((*store)->putCast(shallow));
        stored = (*store)->findCast(root.id);
        assert(stored && *stored && (*stored)->directReplies.size() == 2);
        assert((*stored)->engagement.likes == 5);
    }
    auto reopened = storage::read::openReadStore(database.path);
    assert(reopened);
    const auto restored = (*reopened)->findCast(root.id);
    assert(restored && *restored && (*restored)->directReplies.size() == 2);
    assert((*restored)->directReplies[0].directReplies.size() == 1);
    assert((*restored)->directReplies[0].authorProfile);
    assert((*restored)->directReplies[0].authorProfile->displayName);
    assert((*restored)->directReplies[0].authorProfile->displayName->value == "Reply Author");
    // Reading an accumulated timeline must NOT deserialize the persisted
    // reply tree for every cast. It is requested only on detail navigation.
    const local::read::FeedKey feedKey{
        .kind = local::read::FeedKey::Kind::Recent,
    };
    auto feedSummary = root;
    feedSummary.directReplies.clear();
    auto listingPage = pageFor(
        makeProfile(42, "root-author", "Root Author"), std::move(feedSummary));
    assert((*reopened)->writeFeed(feedKey, listingPage, false));
    const auto listing = (*reopened)->readFeed(feedKey);
    assert(listing && listing->casts.size() == 1);
    assert(listing->casts.front().id.value == root.id.value);
    assert(listing->casts.front().directReplies.empty());
    assert(listing->casts.front().engagement.replies == root.engagement.replies);
    const auto stillWarm = (*reopened)->findCast(root.id);
    assert(stillWarm && *stillWarm && (*stillWarm)->directReplies.size() == 2);
    assert((*stillWarm)->directReplies.front().directReplies.size() == 1);
    ImmediateExecutor executor{};
    FakeAsyncCastGateway unavailable{};
    application::read::async_service::ReadService service{executor, **reopened, unavailable};
    bool sawWarmConversation = false;
    auto request = service.openCast(root.id,
        [&sawWarmConversation](application::read::model::CastState state) {
            if (state.cast && state.cast->directReplies.size() == 2) {
                sawWarmConversation = true;
            }
        });
    assert(request && sawWarmConversation);
}

void largeConversationEmbedsRemainDistinctAfterBatchPersistence()
{
    using namespace farcaster;
    DatabaseFixture database{};
    auto store = storage::read::openReadStore(database.path);
    assert(store);
    auto root = makeCast("0xlarge-batch", 42, "root", 1000);
    constexpr std::size_t replyCount{96};
    for (std::size_t index = 0; index < replyCount; ++index) {
        auto child = makeCast(
            "0xbatch-reply-" + std::to_string(index), 18,
            "reply-" + std::to_string(index), 1010 + static_cast<std::int64_t>(index));
        child.parent = root.id;
        child.embeds.push_back(domain::cast::Embed{
            .kind = domain::cast::EmbedKind::Url,
            .value = "https://example.com/item/" + std::to_string(index),
        });
        child.embeds.push_back(domain::cast::Embed{
            .kind = domain::cast::EmbedKind::CastReference,
            .value = "0xquote-" + std::to_string(index),
            .referencedAuthor = domain::identity::Fid{19},
        });
        root.directReplies.push_back(std::move(child));
    }
    assert((*store)->putCast(root));
    const auto restored = (*store)->findCast(root.id);
    assert(restored && *restored);
    assert((*restored)->directReplies.size() == replyCount);
    for (std::size_t index = 0; index < replyCount; ++index) {
        const auto& child = (*restored)->directReplies[index];
        assert(child.id.value == "0xbatch-reply-" + std::to_string(index));
        assert(child.embeds.size() == 2);
        assert(child.embeds[0].value ==
            "https://example.com/item/" + std::to_string(index));
        assert(child.embeds[1].kind == domain::cast::EmbedKind::CastReference);
        assert(child.embeds[1].value == "0xquote-" + std::to_string(index));
        assert(child.embeds[1].referencedAuthor
            && child.embeds[1].referencedAuthor->value == 19);
    }
}

void overBudgetConversationFailsAtomically()
{
    using namespace farcaster;
    DatabaseFixture database{};
    auto store = storage::read::openReadStore(database.path);
    assert(store);
    auto root = makeCast("0xoversize", 42, "Too many replies", 2000);
    for (std::size_t index = 0; index < 2050; ++index) {
        root.directReplies.push_back(makeCast(
            "0xnode-" + std::to_string(index), 42, "Child", 2100));
    }
    auto status = (*store)->putCast(root);
    assert(!status);
    assert(status.error().code == foundation::error::ErrorCode::InvalidArgument);
    const auto absent = (*store)->findCast(root.id);
    assert(absent && !*absent);
}

void rejectedAsyncCacheCompletionIsTerminal()
{
    using namespace farcaster;
    DatabaseFixture database{};
    auto store = storage::read::openReadStore(database.path);
    assert(store);
    RejectCompletionExecutor executor{};
    FakeAsyncCastGateway gateway{};
    application::read::async_service::ReadService service{executor, **store, gateway};
    std::vector<application::read::model::ReadPhase> phases{};
    auto request = service.openCast(domain::cast::CastId{"0xqueue-rejection"},
        [&phases](application::read::model::CastState state) {
            phases.push_back(state.metadata.phase);
            if (state.metadata.phase == application::read::model::ReadPhase::Failed) {
                assert(state.metadata.error);
                assert(state.metadata.error->code == foundation::error::ErrorCode::RateLimited);
            }
        });
    assert(request);
    assert(executor.jobs == 2);
    assert(phases.size() == 2);
    assert(phases.front() == application::read::model::ReadPhase::Refreshing);
    assert(phases.back() == application::read::model::ReadPhase::Failed);
}

void asyncPaginationPublishesOnlyNewPageWhenPresentationOptsIn()
{
    using namespace farcaster;
    DatabaseFixture database{};
    auto opened = storage::read::openReadStore(database.path);
    assert(opened);
    CountingReadStore store{std::move(*opened)};
    ImmediateExecutor executor{};
    FakeAsyncCastGateway gateway{};
    application::read::async_service::ReadService service{executor, store, gateway};
    const application::read::model::FeedQuery firstQuery{
        .kind = application::read::model::FeedQuery::Kind::Recent,
        .limit = 1U,
    };
    gateway.servedFeed = pageFor(makeProfile(1, "first", "First"),
        makeCast("0xincremental-first", 1, "First page", 1000),
        capability::pagination::ContinuationToken{"second"});
    std::vector<application::read::model::FeedState> observed{};
    auto first = service.openFeed(firstQuery,
        [&observed](application::read::model::FeedState state) {
            observed.push_back(std::move(state));
        });
    assert(first);
    assert(observed.size() == 2);
    assert(observed.back().metadata.phase == application::read::model::ReadPhase::Ready);
    assert(observed.back().entries.size() == 1);
    assert(observed.back().next && observed.back().next->value == "second");
    assert(store.feedMaterializations == 2); // Warm-cache read and cold first page.

    observed.clear();
    gateway.servedFeed = pageFor(makeProfile(2, "second", "Second"),
        makeCast("0xincremental-second", 2, "Second page", 2000),
        capability::pagination::ContinuationToken{"third"});
    auto second = service.openFeed(
        application::read::model::FeedQuery{
            .kind = application::read::model::FeedQuery::Kind::Recent,
            .limit = 1U,
            .continuation = capability::pagination::ContinuationToken{"second"},
        },
        [&observed](application::read::model::FeedState state) {
            observed.push_back(std::move(state));
        },
        application::read::model::ReadOptions{
            .intent = application::read::model::ReadIntent::Prefetch,
            .pageOnlyContinuation = true,
        });
    assert(second);
    assert(observed.size() == 1);
    assert(observed.front().entries.size() == 1);
    assert(observed.front().entries.front().castId.value == "0xincremental-second");
    assert(observed.front().casts.size() == 1);
    assert(observed.front().next && observed.front().next->value == "third");
    assert(observed.front().metadata.phase == application::read::model::ReadPhase::Ready);
    assert(store.feedMaterializations == 2); // No O(total timeline) read on append.
    const local::read::FeedKey key{.kind = local::read::FeedKey::Kind::Recent};
    const auto durable = store.readFeed(key);
    assert(store.feedMaterializations == 3);
    assert(durable && durable->entries.size() == 2);
    assert(durable->entries.front().castId.value == "0xincremental-first");
    assert(durable->entries.back().castId.value == "0xincremental-second");

    // Legacy consumers keep the default complete snapshot semantics.
    observed.clear();
    gateway.servedFeed = pageFor(makeProfile(3, "third", "Third"),
        makeCast("0xincremental-third", 3, "Third page", 3000));
    auto third = service.openFeed(
        application::read::model::FeedQuery{
            .kind = application::read::model::FeedQuery::Kind::Recent,
            .limit = 1U,
            .continuation = capability::pagination::ContinuationToken{"third"},
        },
        [&observed](application::read::model::FeedState state) {
            observed.push_back(std::move(state));
        });
    assert(third);
    assert(observed.size() == 1);
    assert(observed.front().entries.size() == 3);
    assert(store.feedMaterializations == 4); // The opt-out still materializes.
    assert(observed.front().entries[0].castId.value == "0xincremental-first");
    assert(observed.front().entries[1].castId.value == "0xincremental-second");
    assert(observed.front().entries[2].castId.value == "0xincremental-third");
}

void pageOnlyContinuationFailuresPreserveViewportWithoutFullRead()
{
    using namespace farcaster;
    DatabaseFixture database{};
    auto opened = storage::read::openReadStore(database.path);
    assert(opened);
    CountingReadStore store{std::move(*opened)};
    ImmediateExecutor executor{};
    FakeAsyncCastGateway gateway{};
    application::read::async_service::ReadService service{executor, store, gateway};
    gateway.servedFeed = pageFor(makeProfile(88, "cached", "Cached"),
        makeCast("0xkept-visible", 88, "still visible", 1000),
        capability::pagination::ContinuationToken{"page-next"});
    const application::read::model::FeedQuery firstQuery{
        .kind = application::read::model::FeedQuery::Kind::Recent,
        .limit = 20U,
    };
    std::vector<application::read::model::FeedState> observed{};
    auto first = service.openFeed(firstQuery,
        [&observed](auto state) { observed.push_back(std::move(state)); });
    assert(first && observed.size() == 2U);
    assert(store.feedMaterializations == 2);

    const application::read::model::FeedQuery continued{
        .kind = application::read::model::FeedQuery::Kind::Recent,
        .limit = 20U,
        .continuation = capability::pagination::ContinuationToken{"page-next"},
    };
    const application::read::model::ReadOptions pageOnly{
        .intent = application::read::model::ReadIntent::Prefetch,
        .pageOnlyContinuation = true,
    };

    // A callback-time 503 must publish an empty terminal delta. Older pages
    // are already owned by the active surface and must not be read again.
    observed.clear();
    gateway.failFeedCompletion = true;
    auto failure = service.openFeed(continued,
        [&observed](auto state) { observed.push_back(std::move(state)); }, pageOnly);
    assert(failure && observed.size() == 1U);
    assert(observed.front().metadata.phase == application::read::model::ReadPhase::Ready);
    assert(observed.front().entries.empty() && !observed.front().next);
    assert(store.feedMaterializations == 2);

    // Admission failure before a request handle exists must follow exactly
    // the same page-only path; it cannot accidentally re-scan all local rows.
    observed.clear();
    gateway.failFeedCompletion = false;
    gateway.servedFeed.reset();
    auto rejected = service.openFeed(continued,
        [&observed](auto state) { observed.push_back(std::move(state)); }, pageOnly);
    assert(rejected && observed.size() == 1U);
    assert(observed.front().metadata.phase == application::read::model::ReadPhase::Ready);
    assert(observed.front().entries.empty() && !observed.front().next);
    assert(store.feedMaterializations == 2);

    // Non-page-only clients still receive the complete cached snapshot on
    // unavailable continuation, preserving their historical semantics.
    observed.clear();
    auto legacy = service.openFeed(continued,
        [&observed](auto state) { observed.push_back(std::move(state)); });
    assert(legacy && observed.size() == 1U);
    assert(observed.front().entries.size() == 1U);
    assert(observed.front().entries.front().castId.value == "0xkept-visible");
    assert(store.feedMaterializations == 3);
}

void lightweightCastPreviewSkipsOptionalProfileNetwork()
{
    using namespace farcaster;
    DatabaseFixture database{};
    auto store = storage::read::openReadStore(database.path);
    assert(store);
    ImmediateExecutor executor{};
    FakeAsyncCastGateway gateway{};
    gateway.failCast = false;
    application::read::async_service::ReadService service{executor, **store, gateway};
    int readyEvents{};
    auto request = service.openCast(
        domain::cast::CastId{"0xpreview-only"},
        [&readyEvents](application::read::model::CastState state) {
            if (state.metadata.phase == application::read::model::ReadPhase::Ready) {
                assert(state.cast);
                assert(state.cast->text == "Cached background preview");
                ++readyEvents;
            }
        },
        application::read::model::ReadOptions{
            .intent = application::read::model::ReadIntent::Prefetch,
        });
    assert(request);
    assert(readyEvents == 1);
    assert(gateway.profileCalls == 0);
    const auto warm = (*store)->findCast(domain::cast::CastId{"0xpreview-only"});
    assert(warm && *warm && (*warm)->text == "Cached background preview");
}

void freshCastAndRepliesDoNotWaitForOptionalAuthor()
{
    using namespace farcaster;
    DatabaseFixture database{};
    auto store = storage::read::openReadStore(database.path);
    assert(store);
    ImmediateExecutor executor{};
    FakeAsyncCastGateway gateway{};
    gateway.failCast = false;
    gateway.includeReply = true;
    gateway.holdProfile = true;
    application::read::async_service::ReadService service{executor, **store, gateway};

    std::vector<application::read::model::CastState> deliveries{};
    auto request = service.openCast(
        domain::cast::CastId{"0xprimary-before-author"},
        [&deliveries](application::read::model::CastState state) {
            if (state.metadata.phase == application::read::model::ReadPhase::Ready) {
                deliveries.push_back(std::move(state));
            }
        });
    assert(request);
    assert(gateway.profileCalls == 1);
    assert(gateway.pendingProfile);
    assert(deliveries.size() == 1U);
    assert(deliveries[0].cast);
    assert(deliveries[0].cast->directReplies.size() == 1U);
    assert(deliveries[0].cast->directReplies[0].text
        == "Fresh reply before optional author");
    assert(!deliveries[0].author);

    // A delayed optional author can enrich the already usable detail without
    // clearing the confirmed reply tree or producing another loading state.
    auto reply = std::move(gateway.pendingProfile);
    reply(makeProfile(42, "fresh-author", "Fresh Author"));
    assert(deliveries.size() == 2U);
    assert(deliveries.back().author && deliveries.back().author->displayName);
    assert(deliveries.back().author->displayName->value == "Fresh Author");
    assert(deliveries.back().cast);
    assert(deliveries.back().cast->directReplies.size() == 1U);

    // A failed optional profile read also leaves fresh content and replies
    // usable, without publishing a new Failed primary state.
    std::vector<application::read::model::CastState> failedAuthorDeliveries{};
    auto failedRequest = service.openCast(
        domain::cast::CastId{"0xauthor-unavailable"},
        [&failedAuthorDeliveries](application::read::model::CastState state) {
            if (state.metadata.phase == application::read::model::ReadPhase::Ready) {
                failedAuthorDeliveries.push_back(std::move(state));
            }
        });
    assert(failedRequest && gateway.pendingProfile);
    assert(failedAuthorDeliveries.size() == 1U);
    assert(failedAuthorDeliveries.front().metadata.phase
        == application::read::model::ReadPhase::Ready);
    auto failedProfile = std::move(gateway.pendingProfile);
    failedProfile(std::unexpected(foundation::error::Error::make(
        foundation::error::ErrorCode::Unavailable,
        "Deliberately failed optional profile.")));
    assert(failedAuthorDeliveries.size() == 1U);
    assert(failedAuthorDeliveries.front().cast);
    assert(failedAuthorDeliveries.front().cast->directReplies.size() == 1U);
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
    largeLocalFeedWindowPreservesOrderAndRemoteContinuation();
    cachedFeedPrecedesRefreshAndPaginationReconciles();
    prefetchContinuationFailurePreservesUsableFeed();
    refreshFailureWithoutCacheIsNotMarkedStale();
    refreshFailurePreservesCacheAcrossRestart();
    interactiveSearchPublishesRefreshingThenReady();
    identityCastProfileAndSocialFlowsPersistCanonicalData();
    profileFeedCachesRemainIsolated();
    embeddedFeedProfilesDoNotRegressAuthoritativeProfileMetadata();
    normalizedConversationSurvivesRestartAndOfflineRead();
    largeConversationEmbedsRemainDistinctAfterBatchPersistence();
    overBudgetConversationFailsAtomically();
    rejectedAsyncCacheCompletionIsTerminal();
    asyncPaginationPublishesOnlyNewPageWhenPresentationOptsIn();
    pageOnlyContinuationFailuresPreserveViewportWithoutFullRead();
    lightweightCastPreviewSkipsOptionalProfileNetwork();
    freshCastAndRepliesDoNotWaitForOptionalAuthor();
    serializedExecutorKeepsBlockingReadOffCallerThread();
    return 0;
}
