#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

import farcaster.application.read.async_gateway;
import farcaster.application.read.model;
import farcaster.capability.feed;
import farcaster.capability.social;
import farcaster.capability.model;
import farcaster.domain.identity;
import farcaster.execution.weighted_thread_pool;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.model;
import farcaster.provider.snapchain.read;
import farcaster.storage.continuation;
import farcaster.transport.async_http;
import farcaster.transport.http;

namespace {
using namespace farcaster;
namespace raw = farcaster::provider::snapchain::read;
namespace model = farcaster::application::read::model;

constexpr std::string_view rootHash = "0x0000000000000000000000000000000000000001";
constexpr std::string_view quoteHash = "0x0000000000000000000000000000000000000002";
constexpr std::string_view replyHash = "0x0000000000000000000000000000000000000003";
constexpr std::string_view embeddedHash = "0x0000000000000000000000000000000000000004";

[[nodiscard]] std::string castJson(const bool wrongFid = false)
{
    std::string body = R"JSON({"messages":[
{"hash":")JSON" + std::string{rootHash} + R"JSON(","data":{"fid":)JSON"
        + (wrongFid ? "99" : "42") +
        R"JSON(,"timestamp":100,"castAddBody":{"text":"root #test","embeds":[{"url":"https://example.com"}]}}},
{"hash":")JSON" + std::string{quoteHash} +
        R"JSON(","data":{"fid":"42","timestamp":101,"castAddBody":{"text":"quote","embeds":[{"castId":{"fid":7,"hash":")JSON"
        + std::string{embeddedHash} + R"JSON("}}]}}},
{"hash":")JSON" + std::string{replyHash} +
        R"JSON(","data":{"fid":42,"timestamp":102,"castAddBody":{"text":"reply","parentCastId":{"fid":7,"hash":")JSON"
        + std::string{embeddedHash} + R"JSON("}}}}],"nextPageToken":"opaque+/="})JSON";
    return body;
}

void testRawProfileCodec()
{
    constexpr std::string_view body = R"JSON({"messages":[
{"data":{"fid":"42","userDataBody":{"type":"USER_DATA_TYPE_USERNAME","value":"alice"}}},
{"data":{"fid":42,"userDataBody":{"type":2,"value":"Alice Example"}}},
{"data":{"fid":"42","userDataBody":{"type":3,"value":"hello world"}}},
{"data":{"fid":42,"userDataBody":{"type":1,"value":"https://cdn.example/pfp.png"}}}
]})JSON";
    auto profile = raw::decodeUserData(body, domain::identity::Fid{42});
    assert(profile);
    assert(profile->fid.value == 42);
    assert(profile->username && profile->username->value == "alice");
    assert(profile->displayName && profile->displayName->value == "Alice Example");
    assert(profile->bio == "hello world");
    assert(profile->avatarUrl && *profile->avatarUrl == "https://cdn.example/pfp.png");

    assert(!raw::decodeUserData(body, domain::identity::Fid{12}));
    assert(!raw::decodeUserData("not-json", domain::identity::Fid{42}));
    assert(!raw::decodeUserData(R"({"messages":{}})", domain::identity::Fid{42}));
}

void testRawUserFeeds()
{
    for (const auto kind : {model::FeedQuery::Kind::UserPosts,
                            model::FeedQuery::Kind::UserQuotes,
                            model::FeedQuery::Kind::UserReplies}) {
        auto page = raw::decodeUserCasts(castJson(), domain::identity::Fid{42}, kind);
        assert(page);
        assert(page->casts.size() == 1U && page->entries.size() == 1U);
        const auto& cast = page->casts.front();
        assert(cast.author.value == 42U);
        assert(cast.id.value == (kind == model::FeedQuery::Kind::UserPosts ? rootHash
            : kind == model::FeedQuery::Kind::UserQuotes ? quoteHash : replyHash));
        assert(page->entries.front().castId.value == cast.id.value);
        assert(!page->next); // Provider cursors are stored only by the gateway.
        if (kind == model::FeedQuery::Kind::UserPosts)
            assert(cast.embeds.size() == 1U);
        if (kind == model::FeedQuery::Kind::UserQuotes)
            assert(cast.embeds.front().referencedAuthor->value == 7U);
        if (kind == model::FeedQuery::Kind::UserReplies)
            assert(cast.parent && cast.parent->value == embeddedHash);
    }
    assert(!raw::decodeUserCasts(castJson(true), domain::identity::Fid{42},
        model::FeedQuery::Kind::UserPosts));
    assert(!raw::decodeUserCasts(castJson(), domain::identity::Fid{42},
        model::FeedQuery::Kind::Trending));
}


void testRawFollowGraphCodec()
{
    constexpr std::string_view following = R"JSON({
      "messages": [
        {"data":{"fid":"42","linkBody":{"type":"follow","targetFid":"7"}}},
        {"data":{"fid":42,"linkBody":{"type":"follow","targetFid":8}}},
        {"data":{"fid":42,"linkBody":{"type":"follow","targetFid":7}}},
        {"data":{"fid":42,"linkBody":{"type":"block","targetFid":9}}}
      ], "nextPageToken":"page+/="})JSON";
    auto graph = raw::decodeFollowLinks(
        following, domain::identity::Fid{42}, model::SocialQuery::Kind::Following);
    assert(graph && graph->relations.size() == 2U && graph->profiles.size() == 2U);
    assert(graph->relations.front().source.value == 42U);
    assert(graph->relations.front().target.value == 7U);
    assert(graph->profiles.front().fid.value == 7U);
    assert(!graph->profiles.front().username); // Raw graph cannot guess names.
    constexpr std::string_view followers = R"JSON({"messages":[
      {"data":{"fid":99,"linkBody":{"type":"follow","targetFid":42}}}
    ]})JSON";
    auto incoming = raw::decodeFollowLinks(
        followers, domain::identity::Fid{42}, model::SocialQuery::Kind::Followers);
    assert(incoming && incoming->profiles.size() == 1U);
    assert(incoming->relations.front().source.value == 99U);
    assert(incoming->profiles.front().fid.value == 99U);

    assert(!raw::decodeFollowLinks(following, domain::identity::Fid{7},
        model::SocialQuery::Kind::Following));
    assert(!raw::decodeFollowLinks(followers, domain::identity::Fid{7},
        model::SocialQuery::Kind::Followers));
    assert(!raw::decodeFollowLinks(R"JSON({"messages":{}})JSON",
        domain::identity::Fid{42}, model::SocialQuery::Kind::Following));
    assert(!raw::decodeFollowLinks(R"JSON({"messages":[{"data":{
        "fid":42,"linkBody":{"type":"follow","targetFid":0}}}]})JSON",
        domain::identity::Fid{42}, model::SocialQuery::Kind::Following));
}

void testNoFalseCapabilities()
{
    const auto declared = raw::capabilities();
    assert(declared.size() == 8U);
    for (const auto& item : declared) {
        assert(item.id.value != "cast.read");
        assert(item.id.value != "feed.trending.read");
        assert(item.id.value != "search.read");
        assert(item.id.value != "notifications.read");
    }
}

struct DatabaseFixture final {
    DatabaseFixture()
    {
        const auto id = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path()
            / ("genycaster-snapchain-read-" + std::to_string(id) + ".sqlite3");
    }
    ~DatabaseFixture()
    {
        std::error_code ignored{};
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(path.string() + "-wal", ignored);
        std::filesystem::remove(path.string() + "-shm", ignored);
    }
    std::filesystem::path path{};
};

class FakeAsyncTransport final : public transport::async_http::IAsyncHttpTransport {
public:
    transport::async_http::Request last{};
    std::size_t requests{0};
    bool defer{false};
    bool followingFixture{false};
    bool graphTooLarge{false};
    bool authorHistoryIncomplete{false};
    std::vector<std::string> urls{};
    transport::async_http::Completion pending{};
    foundation::result::Result<transport::async_http::Operation> start(
        transport::async_http::Request request,
        transport::async_http::Completion complete) override
    {
        last = std::move(request);
        ++requests;
        if (defer) {
            pending = std::move(complete);
            return transport::async_http::Operation{[] {}};
        }

        urls.push_back(last.url);
        if (followingFixture) {
            if (last.url.find("linksByFid") != std::string::npos) {
                complete(transport::http::HttpResponse{.statusCode = 200U,
                    .body = graphTooLarge
                        ? R"({"messages":[],"nextPageToken":"graph-more"})"
                        : R"({"messages":[
                          {"data":{"fid":42,"linkBody":{"type":"follow","targetFid":7}}},
                          {"data":{"fid":42,"linkBody":{"type":"follow","targetFid":9}}}
                        ]})"});
            } else if (last.url.find("castsByFid") != std::string::npos) {
                const auto fid = last.url.find("fid=7") != std::string::npos ? 7U
                    : last.url.find("fid=9") != std::string::npos ? 9U : 42U;
                const auto entry = [](const unsigned author, const unsigned time,
                                      const std::string_view hash) {
                    return "{\"hash\":\"" + std::string{hash}
                        + "\",\"data\":{\"fid\":" + std::to_string(author)
                        + ",\"timestamp\":" + std::to_string(time)
                        + ",\"castAddBody\":{\"text\":\"observed\"}}}";
                };
                std::string messages{};
                if (fid == 7U) {
                    messages = entry(fid, 200U, rootHash) + ","
                        + entry(fid, 100U, embeddedHash);
                } else if (fid == 9U) {
                    messages = entry(fid, 150U, quoteHash);
                } else {
                    messages = entry(fid, 160U, replyHash);
                }
                complete(transport::http::HttpResponse{.statusCode = 200U,
                    .body = "{\"messages\":[" + messages + "]"
                        + (authorHistoryIncomplete ? ",\"nextPageToken\":\"more\"}" : "}")});
            } else {
                complete(transport::http::HttpResponse{.statusCode = 404U});
            }
            return transport::async_http::Operation{[] {}};
        }
        if (last.url.find("castsByFid") != std::string::npos)
            complete(transport::http::HttpResponse{.statusCode = 200U, .body = castJson()});
        else if (last.url.find("userDataByFid") != std::string::npos)
            complete(transport::http::HttpResponse{
                .statusCode = 200U, .body = R"({"messages":[]})"});
        else if (last.url.find("linksByFid") != std::string::npos)
            complete(transport::http::HttpResponse{.statusCode = 200U,
                .body = R"({"messages":[{"data":{"fid":42,"linkBody":{"type":"follow","targetFid":7}}}],"nextPageToken":"link+/="})"});
        else if (last.url.find("linksByTargetFid") != std::string::npos)
            complete(transport::http::HttpResponse{.statusCode = 200U,
                .body = R"({"messages":[{"data":{"fid":7,"linkBody":{"type":"follow","targetFid":42}}}],"nextPageToken":"followers+/="})"});
        else
            complete(transport::http::HttpResponse{.statusCode = 404U});
        return transport::async_http::Operation{[] {}};
    }
};


void testRawSocialRequestAndCursorAffinity()
{
    DatabaseFixture db{};
    auto continuations = storage::continuation::openContinuationStore(db.path);
    assert(continuations);
    execution::weighted_thread_pool::WeightedThreadPoolExecutor decoder{
        1U, 16U, 8U * 1024U * 1024U};
    FakeAsyncTransport transport{};
    auto gateway = raw::makeGateway(transport, decoder, *continuations,
        raw::Config{.baseUrl = "https://raw.example",
            .providerId = provider::model::ProviderId{"social-node"}});

    const auto fetch = [&gateway](model::SocialQuery query) {
        auto promise = std::make_shared<std::promise<
            foundation::result::Result<capability::social::SocialGraphPage>>>();
        auto future = promise->get_future();
        auto started = gateway->social(query,
            [promise](auto result) { promise->set_value(std::move(result)); });
        assert(started);
        assert(future.wait_for(std::chrono::seconds{3}) == std::future_status::ready);
        auto result = future.get();
        assert(result);
        return *result;
    };
    auto following = fetch(model::SocialQuery{
        .kind = model::SocialQuery::Kind::Following,
        .fid = domain::identity::Fid{42}, .limit = 12U});
    assert(following.profiles.size() == 1U);
    assert(following.profiles.front().fid.value == 7U);
    assert(following.next);
    assert(transport.last.url.find("/v1/linksByFid?fid=42&link_type=follow&pageSize=12")
           != std::string::npos);
    auto next = fetch(model::SocialQuery{
        .kind = model::SocialQuery::Kind::Following,
        .fid = domain::identity::Fid{42}, .limit = 12U,
        .continuation = following.next});
    assert(next.relations.size() == 1U);
    assert(transport.last.url.find("pageToken=link%2B%2F%3D") != std::string::npos);

    auto wrongDirection = gateway->social(model::SocialQuery{
        .kind = model::SocialQuery::Kind::Followers,
        .fid = domain::identity::Fid{42}, .limit = 12U,
        .continuation = following.next}, [](auto) {});
    assert(!wrongDirection);
    auto wrongFid = gateway->social(model::SocialQuery{
        .kind = model::SocialQuery::Kind::Following,
        .fid = domain::identity::Fid{7}, .limit = 12U,
        .continuation = following.next}, [](auto) {});
    assert(!wrongFid);
    FakeAsyncTransport otherTransport{};
    auto moved = raw::makeGateway(otherTransport, decoder, *continuations,
        raw::Config{.baseUrl = "https://other-raw.example",
            .providerId = provider::model::ProviderId{"social-node"}});
    auto wrongEndpoint = moved->social(model::SocialQuery{
        .kind = model::SocialQuery::Kind::Following,
        .fid = domain::identity::Fid{42}, .limit = 12U,
        .continuation = following.next}, [](auto) {});
    assert(!wrongEndpoint && otherTransport.requests == 0U);

    auto followers = fetch(model::SocialQuery{
        .kind = model::SocialQuery::Kind::Followers,
        .fid = domain::identity::Fid{42}, .limit = 10U});
    assert(followers.relations.size() == 1U);
    assert(followers.relations.front().source.value == 7U);
    assert(transport.last.url.find("/v1/linksByTargetFid?target_fid=42&link_type=follow")
           != std::string::npos);
}


void testBoundedLiveFollowingFromRawNode()
{
    DatabaseFixture db{};
    auto cursors = storage::continuation::openContinuationStore(db.path);
    assert(cursors);
    execution::weighted_thread_pool::WeightedThreadPoolExecutor decoder{
        1U, 40U, 16U * 1024U * 1024U};
    FakeAsyncTransport transport{};
    transport.followingFixture = true;
    auto gateway = raw::makeGateway(transport, decoder, *cursors,
        raw::Config{.baseUrl = "https://read.operator.example",
            .providerId = provider::model::ProviderId{"personal-raw"}});

    const auto fetch = [&gateway](model::FeedQuery query) {
        auto done = std::make_shared<std::promise<
            foundation::result::Result<capability::feed::FeedPage>>>();
        auto future = done->get_future();
        auto req = gateway->feed(query,
            [done](auto value) { done->set_value(std::move(value)); });
        assert(req);
        assert(future.wait_for(std::chrono::seconds{4}) == std::future_status::ready);
        return future.get();
    };
    const auto query = model::FeedQuery{
        .kind = model::FeedQuery::Kind::Following,
        .fid = domain::identity::Fid{42}, .limit = 2U,
    };
    auto first = fetch(query);
    assert(first && first->casts.size() == 2U && first->next);
    assert(first->casts[0].id.value == rootHash);
    assert(first->casts[1].id.value == replyHash);
    assert(first->entries[0].sortTime > first->entries[1].sortTime);
    assert(transport.urls.size() == 4U); // graph + own and 2 followed FIDs
    assert(transport.urls[0].find("link_type=follow&pageSize=33")
        != std::string::npos);
    assert(transport.urls[0].starts_with(
        "https://read.operator.example/v1/linksByFid"));

    transport.urls.clear();
    auto second = fetch(model::FeedQuery{
        .kind = model::FeedQuery::Kind::Following,
        .fid = domain::identity::Fid{42}, .limit = 2U,
        .continuation = first->next,
    });
    assert(second && second->casts.size() == 2U && !second->next);
    assert(second->casts[0].id.value == quoteHash);
    assert(second->casts[1].id.value == embeddedHash);
    assert(transport.urls.size() == 3U); // cursor freezes graph membership
    for (const auto& url : transport.urls) {
        assert(url.find("castsByFid") != std::string::npos);
        assert(url.find("reverse=1") != std::string::npos);
    }
    const auto requestCount = transport.requests;
    auto wrongFid = gateway->feed(model::FeedQuery{
        .kind = model::FeedQuery::Kind::Following,
        .fid = domain::identity::Fid{7}, .limit = 2U,
        .continuation = first->next,
    }, [](auto) {});
    assert(!wrongFid);
    auto wrongLimit = gateway->feed(model::FeedQuery{
        .kind = model::FeedQuery::Kind::Following,
        .fid = domain::identity::Fid{42}, .limit = 3U,
        .continuation = first->next,
    }, [](auto) {});
    assert(!wrongLimit);
    FakeAsyncTransport elsewhere{};
    auto moved = raw::makeGateway(elsewhere, decoder, *cursors,
        raw::Config{.baseUrl = "https://other-node.example",
            .providerId = provider::model::ProviderId{"personal-raw"}});
    auto wrongNode = moved->feed(model::FeedQuery{
        .kind = model::FeedQuery::Kind::Following,
        .fid = domain::identity::Fid{42}, .limit = 2U,
        .continuation = first->next,
    }, [](auto) {});
    assert(!wrongNode);
    assert(transport.requests == requestCount && elsewhere.requests == 0U);

    // A >32-Follow graph must not be misrepresented as a complete feed.
    transport.graphTooLarge = true;
    auto oversized = fetch(query);
    assert(!oversized
        && oversized.error().code == foundation::error::ErrorCode::Unsupported);
    transport.graphTooLarge = false;
    // One incomplete 100-message window is not enough to merge safely.
    transport.authorHistoryIncomplete = true;
    auto unsafe = fetch(query);
    assert(!unsafe
        && unsafe.error().code == foundation::error::ErrorCode::Unsupported);

    assert(!gateway->feed(model::FeedQuery{
        .kind = model::FeedQuery::Kind::Following,
        .fid = domain::identity::Fid{42}, .limit = 31U}, [](auto) {}));

    // Cancel during the first raw graph request. Even a late transport
    // completion must not emit a partial feed or request author Cast pages.
    FakeAsyncTransport paused{};
    paused.defer = true;
    auto pausedGateway = raw::makeGateway(paused, decoder, *cursors,
        raw::Config{.baseUrl = "https://read.operator.example",
            .providerId = provider::model::ProviderId{"personal-raw"}});
    bool invoked = false;
    auto pending = pausedGateway->feed(query, [&](auto) { invoked = true; });
    assert(pending && paused.requests == 1U);
    pending->cancel();
    paused.pending(transport::http::HttpResponse{
        .statusCode = 200U, .body = R"({"messages":[]})"});
    assert(!invoked && paused.requests == 1U);
}

void testCancellationAndCredentialIsolation()
{
    DatabaseFixture db{};
    auto store = storage::continuation::openContinuationStore(db.path);
    assert(store);
    execution::weighted_thread_pool::WeightedThreadPoolExecutor decoder{1U, 16U, 8U * 1024U * 1024U};
    FakeAsyncTransport transport{};
    transport.defer = true;
    auto gateway = raw::makeGateway(transport, decoder, *store,
        raw::Config{
            .baseUrl = "https://raw.example",
            .authorizationHeaderName = "x-api-key",
            .authorizationHeaderValue = "local-provider-token",
            .providerId = provider::model::ProviderId{"raw-test"},
        });
    bool completionCalled = false;
    auto started = gateway->profile(domain::identity::Fid{42},
        [&](auto) { completionCalled = true; });
    assert(started);
    assert(transport.last.headers.size() == 2U);
    assert(transport.last.headers[1] == "x-api-key: local-provider-token");
    assert(transport.last.url.starts_with("https://raw.example/v1/userDataByFid"));
    assert(transport.pending);
    started->cancel();
    transport.pending(transport::http::HttpResponse{
        .statusCode = 200U,
        .body = R"({"messages":[]})",
    });
    assert(!completionCalled);

    // Unsupported derived capabilities never invoke a raw node.
    auto search = gateway->search(model::SearchQuery{},
        [&](auto) { completionCalled = true; });
    assert(!search);
    assert(transport.requests == 1U && !completionCalled);
}

void testAsyncReadAndOpaquePagination()
{
    DatabaseFixture db{};
    auto store = storage::continuation::openContinuationStore(db.path);
    assert(store);
    execution::weighted_thread_pool::WeightedThreadPoolExecutor decoder{1U, 16U, 8U * 1024U * 1024U};
    FakeAsyncTransport transport{};
    auto gateway = raw::makeGateway(transport, decoder, *store,
        raw::Config{.baseUrl = "https://raw.example",
                    .providerId = provider::model::ProviderId{"raw-test"}});
    assert(gateway);
    auto feedResult = std::make_shared<std::promise<
        foundation::result::Result<capability::feed::FeedPage>>>();
    auto future = feedResult->get_future();
    auto started = gateway->feed(model::FeedQuery{
        .kind = model::FeedQuery::Kind::UserPosts,
        .fid = domain::identity::Fid{42},
        .limit = 5U,
    }, [feedResult](auto result) { feedResult->set_value(std::move(result)); });
    assert(started);
    assert(future.wait_for(std::chrono::seconds{3}) == std::future_status::ready);
    auto page = future.get();
    assert(page && page->casts.size() == 1U);
    assert(page->next && !page->next->value.empty());
    assert(page->next->value != "opaque+/=");
    assert(transport.last.url.find("pageSize=5") != std::string::npos);
    assert(transport.last.url.find("reverse=1") != std::string::npos);
    assert(transport.last.url.find("reverse=true") == std::string::npos);

    auto nextResult = std::make_shared<std::promise<
        foundation::result::Result<capability::feed::FeedPage>>>();
    auto nextFuture = nextResult->get_future();
    auto next = gateway->feed(model::FeedQuery{
        .kind = model::FeedQuery::Kind::UserPosts,
        .fid = domain::identity::Fid{42},
        .limit = 5U,
        .continuation = page->next,
    }, [nextResult](auto result) { nextResult->set_value(std::move(result)); });
    assert(next);
    assert(nextFuture.wait_for(std::chrono::seconds{3}) == std::future_status::ready);
    assert(nextFuture.get());
    assert(transport.last.url.find("pageToken=opaque%2B%2F%3D") != std::string::npos);

    // Provider-affine tokens are also scoped to the exact user FID.
    auto wrongUser = gateway->feed(model::FeedQuery{
        .kind = model::FeedQuery::Kind::UserPosts,
        .fid = domain::identity::Fid{99},
        .limit = 5U,
        .continuation = page->next,
    }, [](auto) {});
    assert(!wrongUser);
    assert(wrongUser.error().code == foundation::error::ErrorCode::Conflict);

    // A continuation cannot be repurposed to another feed operation.
    auto misuse = gateway->feed(model::FeedQuery{
        .kind = model::FeedQuery::Kind::UserQuotes,
        .fid = domain::identity::Fid{42},
        .limit = 5U,
        .continuation = page->next,
    }, [](auto) {});
    assert(!misuse);
    assert(misuse.error().code == foundation::error::ErrorCode::Conflict);

    // A saved node ID is not an immutable endpoint identity. Reusing that ID
    // after an operator switches server origins must not replay its old token
    // against a new raw node, even when capability and user FID match.
    FakeAsyncTransport movedTransport{};
    auto movedGateway = raw::makeGateway(movedTransport, decoder, *store,
        raw::Config{
            .baseUrl = "https://different-node.example",
            .providerId = provider::model::ProviderId{"raw-test"},
        });
    auto retargeted = movedGateway->feed(model::FeedQuery{
        .kind = model::FeedQuery::Kind::UserPosts,
        .fid = domain::identity::Fid{42},
        .limit = 5U,
        .continuation = page->next,
    }, [](auto) {});
    assert(!retargeted);
    assert(retargeted.error().code == foundation::error::ErrorCode::Conflict);
    assert(movedTransport.requests == 0U);

    // Provider ID affinity also rejects a different registered source with
    // an otherwise identical origin and user/capability.
    auto otherGateway = raw::makeGateway(movedTransport, decoder, *store,
        raw::Config{
            .baseUrl = "https://raw.example",
            .providerId = provider::model::ProviderId{"another-operator"},
        });
    auto other = otherGateway->feed(model::FeedQuery{
        .kind = model::FeedQuery::Kind::UserPosts,
        .fid = domain::identity::Fid{42},
        .limit = 5U,
        .continuation = page->next,
    }, [](auto) {});
    assert(!other);
    assert(other.error().code == foundation::error::ErrorCode::Conflict);
    assert(movedTransport.requests == 0U);
}

} // namespace

int main()
{
    testRawProfileCodec();
    testRawUserFeeds();
    testRawFollowGraphCodec();
    testNoFalseCapabilities();
    testAsyncReadAndOpaquePagination();
    testRawSocialRequestAndCursorAffinity();
    testBoundedLiveFollowingFromRawNode();
    testCancellationAndCredentialIsolation();
    return 0;
}
