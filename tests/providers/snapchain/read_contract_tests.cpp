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

void testNoFalseCapabilities()
{
    const auto declared = raw::capabilities();
    assert(declared.size() == 5U);
    for (const auto& item : declared) {
        assert(item.id.value != "cast.read");
        assert(item.id.value != "feed.trending.read");
        assert(item.id.value != "feed.following.read");
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
        if (last.url.find("castsByFid") != std::string::npos)
            complete(transport::http::HttpResponse{.statusCode = 200U, .body = castJson()});
        else if (last.url.find("userDataByFid") != std::string::npos)
            complete(transport::http::HttpResponse{
                .statusCode = 200U, .body = R"({"messages":[]})"});
        else
            complete(transport::http::HttpResponse{.statusCode = 404U});
        return transport::async_http::Operation{[] {}};
    }
};

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
}

} // namespace

int main()
{
    testRawProfileCodec();
    testRawUserFeeds();
    testNoFalseCapabilities();
    testAsyncReadAndOpaquePagination();
    testCancellationAndCredentialIsolation();
    return 0;
}
