#include <algorithm>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <deque>
#include <filesystem>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

import farcaster.application.read.async_gateway;
import farcaster.application.read.model;
import farcaster.execution.serial;
import farcaster.execution.weighted_thread_pool;
import farcaster.transport.async_http;
import farcaster.provider.hypersnap.async_gateway;
import farcaster.capability.catalog;
import farcaster.capability.feed;
import farcaster.capability.model;
import farcaster.capability.pagination;
import farcaster.capability.notification;
import farcaster.domain.cast;
import farcaster.domain.channel;
import farcaster.domain.identity;
import farcaster.domain.social;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.cast;
import farcaster.provider.hypersnap.codec;
import farcaster.provider.hypersnap.config;
import farcaster.provider.hypersnap.feed;
import farcaster.provider.hypersnap.identity;
import farcaster.provider.hypersnap.notification;
import farcaster.provider.hypersnap.profile;
import farcaster.provider.hypersnap.registration;
import farcaster.provider.hypersnap.search;
import farcaster.provider.hypersnap.social;
import farcaster.provider.model;
import farcaster.storage.continuation;
import farcaster.transport.http;
import farcaster.transport.curl;
import farcaster.test.hypersnap.transport;
import farcaster.test.hypersnap.fixtures;

namespace {

using namespace farcaster;

[[nodiscard]] provider::hypersnap::config::Config testConfig()
{
    return provider::hypersnap::config::Config{
        .baseUrl = "https://node.test///",
        .provider = provider::model::ProviderDescriptor{
            .id = provider::model::ProviderId{"hypersnap-test"},
            .displayName = "Hypersnap Contract Node",
            .deployment = provider::model::ProviderDeployment::SelfHosted,
            .privacy = provider::model::ProviderPrivacy::Private,
            .priority = 10,
        },
    };
}

void testCanonicalCodec()
{
    auto user = provider::hypersnap::codec::decodeUser(test::hypersnap::fixtures::userResponse());
    assert(user);
    assert(user->fid.value == 42);
    assert(user->username && user->username->value == "alice");
    assert(user->displayName && user->displayName->value == "Alice");
    assert(user->bio == "hello from hypersnap");
    assert(user->avatarUrl && *user->avatarUrl == "https://cdn.test/alice.png");
    assert(user->bannerUrl && *user->bannerUrl == "https://cdn.test/alice-banner.png");
    assert(user->location && *user->location == "San Francisco");
    assert(user->verifiedEthAddresses.size() == 2U);
    assert(user->verifiedEthAddresses[0] == "0x1111111111111111111111111111111111111111");
    assert(user->verifiedEthAddresses[1] == "0x2222222222222222222222222222222222222222");
    assert(user->verifiedSolAddresses.size() == 1U);
    assert(user->verifiedSolAddresses[0] == "So11111111111111111111111111111111111111112");
    assert(user->followerCount == 12);
    assert(user->followingCount == 8);

    auto userUrl = provider::hypersnap::codec::decodeUserDataValue(
        test::hypersnap::fixtures::userUrlResponse());
    assert(userUrl && *userUrl == "https://example.test/alice");

    auto cast = provider::hypersnap::codec::decodeCast(test::hypersnap::fixtures::castResponse());
    assert(cast);
    assert(cast->id.value == "0xabc123");
    assert(cast->author.value == 42);
    assert(cast->parent && cast->parent->value == "0xparent");
    assert(cast->embeds.size() == 2);
    assert(cast->embeds[0].kind == domain::cast::EmbedKind::Url);
    assert(cast->embeds[0].value == "https://example.test/post");
    assert(cast->embeds[1].kind == domain::cast::EmbedKind::CastReference);
    assert(cast->embeds[1].value == "0xquoted");
    assert(cast->embeds[1].referencedAuthor);
    assert(cast->embeds[1].referencedAuthor->value == 7);
    assert(cast->engagement.likes == 2);
    assert(cast->engagement.recasts == 1);
    assert(cast->engagement.replies == 3);

    auto conversation = provider::hypersnap::codec::decodeConversationPayload(
        test::hypersnap::fixtures::conversationResponse());
    assert(conversation);
    assert(conversation->cast.id.value == "0xabc123");
    assert(conversation->directReplyCount == 2U);
    assert(conversation->cast.engagement.replies == 2U);
    assert(conversation->cast.directReplies.size() == 2U);
    assert(conversation->cast.directReplies.front().id.value == "0xreply-one");
    assert(conversation->cast.directReplies.front().directReplies.size() == 1U);
    assert(conversation->cast.directReplies.front().directReplies.front().id.value == "0xreply-grandchild");

    const auto expectedTime = std::chrono::sys_days{std::chrono::year{2026}/8/27} +
        std::chrono::hours{12} + std::chrono::minutes{34} +
        std::chrono::seconds{56} + std::chrono::milliseconds{789};
    assert(cast->createdAt.time_since_epoch() ==
        std::chrono::duration_cast<std::chrono::milliseconds>(expectedTime.time_since_epoch()));

    auto malformed = provider::hypersnap::codec::decodeFeed("{not-json");
    assert(!malformed);
    assert(malformed.error().code == foundation::error::ErrorCode::ProtocolFailure);

    auto wrappedFeed = provider::hypersnap::codec::decodeFeed(
        std::string{"{\"result\":"} + test::hypersnap::fixtures::feedResponse("") + "}");
    assert(wrappedFeed);
    assert(wrappedFeed->casts.size() == 1U);
}

void testRecentNetworkEventDecoding()
{
    auto shards = provider::hypersnap::codec::decodeRecentShardInfo(R"json({
      "shardInfos": [
        {"shardId": 2, "maxHeight": 220},
        {"shardId": 0, "maxHeight": 200},
        {"shardId": 1, "maxHeight": 230}
      ]
    })json");
    assert(shards && shards->size() == 2U);
    assert((*shards)[0].shard == 1U && (*shards)[0].height == 230U);
    assert((*shards)[1].shard == 2U && (*shards)[1].height == 220U);
    const auto events = provider::hypersnap::codec::decodeRecentCastEvents(R"json({
      "events": [
        {"type":"HUB_EVENT_TYPE_MERGE_MESSAGE","id":3244033,"blockNumber":198,
         "mergeMessageBody":{"message":{"hash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
           "data":{"type":"MESSAGE_TYPE_CAST_ADD","timestamp":182600000,
                    "castAddBody":{"text":"confirmed root"}}}}},
        {"type":"HUB_EVENT_TYPE_MERGE_MESSAGE","id":3260417,"blockNumber":199,
         "mergeMessageBody":{"message":{"hash":"0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
           "data":{"type":"MESSAGE_TYPE_CAST_ADD","timestamp":182600001,
                    "castAddBody":{"parentCastId":{"hash":"0xdead","fid":42}}}}}},
        {"type":"HUB_EVENT_TYPE_MERGE_MESSAGE","id":3276800,"blockNumber":200,
         "mergeMessageBody":{"message":{"hash":"0xcccccccccccccccccccccccccccccccccccccccc",
           "data":{"type":"MESSAGE_TYPE_CAST_ADD","timestamp":182600002,
                    "castAddBody":{}}}}}
      ]
    })json", 198U, 200U);
    assert(events && events->size() == 1U);
    assert(events->front().hash == "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    assert(events->front().protocolTimestamp == 182600000U);
    // The independent Conversations source selects actual CastAdd replies only.
    const auto replies = provider::hypersnap::codec::decodeRecentCastEvents(R"json({
      "events": [
        {"type":"HUB_EVENT_TYPE_MERGE_MESSAGE","id":3244033,"blockNumber":198,
         "mergeMessageBody":{"message":{"hash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
           "data":{"type":"MESSAGE_TYPE_CAST_ADD","timestamp":182600000,
                    "castAddBody":{"text":"question without replies"}}}}},
        {"type":"HUB_EVENT_TYPE_MERGE_MESSAGE","id":3260417,"blockNumber":199,
         "mergeMessageBody":{"message":{"hash":"0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
           "data":{"type":"MESSAGE_TYPE_CAST_ADD","timestamp":182600001,
                    "castAddBody":{"parentCastId":{"hash":"0xdead","fid":42}}}}}}
      ]
    })json", 198U, 200U, true);
    assert(replies && replies->size() == 1U);
    assert(replies->front().hash == "0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    auto invalid = provider::hypersnap::codec::decodeRecentShardInfo(R"json({"shardInfos":[]})json");
    assert(!invalid);
    auto broken = provider::hypersnap::codec::decodeRecentCastEvents(
        R"json({"events":"invalid"})json", 0U, 1U);
    assert(!broken);
}

// A deterministic provider-level runtime exercise without a Qt display or
// system audio service. Asynchronous callbacks are delivered on a bounded
// worker, mirroring the production reactor/decoder separation.
class RecentAsyncTransport final : public transport::async_http::IAsyncHttpTransport {
public:
    RecentAsyncTransport() : m_worker{[this] { drain(); }} {}
    ~RecentAsyncTransport() override {
        {
            const std::scoped_lock lock{m_mutex};
            m_closed = true;
        }
        m_cv.notify_all();
        m_worker.join();
    }

    [[nodiscard]] foundation::result::Result<transport::async_http::Operation>
    start(transport::async_http::Request request,
          transport::async_http::Completion completion) override
    {
        {
            const std::scoped_lock lock{m_mutex};
            m_pending.emplace_back(std::move(request), std::move(completion));
        }
        m_cv.notify_one();
        return transport::async_http::Operation{[] {}};
    }

    [[nodiscard]] std::vector<std::string> requestedUrls() const {
        const std::scoped_lock lock{m_mutex};
        return m_urls;
    }

private:
    void drain() {
        while (true) {
            std::pair<transport::async_http::Request,
                      transport::async_http::Completion> pending{};
            {
                std::unique_lock lock{m_mutex};
                m_cv.wait(lock, [this] { return m_closed || !m_pending.empty(); });
                if (m_pending.empty()) {
                    if (m_closed) { return; }
                    continue;
                }
                pending = std::move(m_pending.front());
                m_pending.pop_front();
                m_urls.push_back(pending.first.url);
            }
            auto response = transport::http::HttpResponse{
                .statusCode = 200U,
                .body = responseFor(pending.first.url),
            };
            pending.second(std::move(response));
        }
    }

    static std::string responseFor(const std::string& url) {
        constexpr std::string_view hashA{
            "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
        constexpr std::string_view hashB{
            "0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"};
        if (url.find("/v1/info") != std::string::npos) {
            return R"json({"shardInfos":[{"shardId":0,"maxHeight":1300},
                     {"shardId":2,"maxHeight":1500},{"shardId":1,"maxHeight":1500}]})json";
        }
        if (url.find("/v1/events") != std::string::npos) {
            if (url.find("shard_index=2") != std::string::npos) {
                return R"json({"events":[]})json";
            }
            return R"json({"events":[{"id":23756802,"blockNumber":1450,"type":"HUB_EVENT_TYPE_MERGE_MESSAGE","mergeMessageBody":{"message":{"hash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","data":{"type":"MESSAGE_TYPE_CAST_ADD","timestamp":182600010,"castAddBody":{"text":"A"}}}}},{"id":23756801,"blockNumber":1450,"type":"HUB_EVENT_TYPE_MERGE_MESSAGE","mergeMessageBody":{"message":{"hash":"0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","data":{"type":"MESSAGE_TYPE_CAST_ADD","timestamp":182600000,"castAddBody":{"text":"B"}}}}},{"id":23756800,"blockNumber":1450,"type":"HUB_EVENT_TYPE_MERGE_MESSAGE","mergeMessageBody":{"message":{"hash":"0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","data":{"type":"MESSAGE_TYPE_CAST_ADD","timestamp":182600015,"castAddBody":{"parentCastId":{"fid":2,"hash":"0xparent"}}}}}}]})json";
        }
        if (url.find("/v2/farcaster/cast/search") != std::string::npos) {
            // Matches the provider's real {result:{casts,next}} envelope.
            // The search index returns a bare, unprefixed cast hash.
            std::string body = test::hypersnap::fixtures::rootPostFeedResponse();
            return std::string{"{\"result\":"} + body + "}";
        }
        if (url.find("/v2/farcaster/cast/bulk") != std::string::npos) {
            std::string body = test::hypersnap::fixtures::rootPostFeedResponse();
            const std::string replacement{
                url.find(std::string{hashB}) != std::string::npos ? hashB : hashA};
            const auto old = body.find("0xabc123");
            assert(old != std::string::npos);
            // Match the live Hypersnap cast/bulk contract: unlike event hashes,
            // hydrated cast IDs contain 40 hex characters WITHOUT 0x.
            body.replace(old, std::string{"0xabc123"}.size(), replacement.substr(2U));
            return body;
        }
        return "{}";
    }

    mutable std::mutex m_mutex{};
    std::condition_variable m_cv{};
    std::deque<std::pair<transport::async_http::Request,
                         transport::async_http::Completion>> m_pending{};
    std::vector<std::string> m_urls{};
    bool m_closed{};
    std::thread m_worker;
};

void testRecentAsyncFeedAndCursor()
{
    const auto path = std::filesystem::temp_directory_path() /
        ("genycaster-recent-contract-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".sqlite3");
    auto continuation = storage::continuation::openContinuationStore(path);
    assert(continuation);
    RecentAsyncTransport transport{};
    execution::weighted_thread_pool::WeightedThreadPoolExecutor decode{1, 32, 4U * 1024U * 1024U};
    execution::serial::SerialExecutor serial{32};
    provider::hypersnap::async_gateway::AsyncGateway gateway{
        transport, decode, serial, *continuation, testConfig()};

    auto requestPage = [&](std::optional<capability::pagination::ContinuationToken> token)
        -> capability::feed::FeedPage {
        auto pending = std::make_shared<
            std::promise<foundation::result::Result<capability::feed::FeedPage>>>();
        auto future = pending->get_future();
        auto request = gateway.feed(
            application::read::model::FeedQuery{
                .kind = application::read::model::FeedQuery::Kind::Recent,
                .limit = 1U, .continuation = std::move(token)},
            [pending](foundation::result::Result<capability::feed::FeedPage> page) {
                pending->set_value(std::move(page));
            });
        assert(request);
        assert(future.wait_for(std::chrono::seconds{5}) == std::future_status::ready);
        auto result = future.get();
        if (!result) { std::fprintf(stderr, "Recent provider error: %s\n", result.error().message.c_str()); }
        assert(result);
        return std::move(*result);
    };
    auto first = requestPage(std::nullopt);
    assert(first.casts.size() == 1U);
    assert(first.casts.front().id.value
        == "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    assert(first.next);
    auto second = requestPage(first.next);
    assert(second.casts.size() == 1U);
    assert(second.casts.front().id.value
        == "0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    assert(second.next);

    const auto paths = transport.requestedUrls();
    assert(std::ranges::count_if(paths, [](const auto& url) {
        return url.find("/v1/info") != std::string::npos; }) == 1);
    assert(std::ranges::count_if(paths, [](const auto& url) {
        return url.find("/v1/events?") != std::string::npos; }) == 4);
    assert(std::ranges::count_if(paths, [](const auto& url) {
        return url.find("/v2/farcaster/cast/bulk?") != std::string::npos; }) == 2);
    const auto querySemantic = [&](const application::read::model::FeedQuery::Kind kind) {
        auto pending = std::make_shared<
            std::promise<foundation::result::Result<capability::feed::FeedPage>>>();
        auto future = pending->get_future();
        auto request = gateway.feed(
            application::read::model::FeedQuery{.kind = kind, .limit = 3U},
            [pending](foundation::result::Result<capability::feed::FeedPage> page) {
                pending->set_value(std::move(page));
            });
        assert(request);
        assert(future.wait_for(std::chrono::seconds{5}) == std::future_status::ready);
        auto result = future.get();
        assert(result && !result->casts.empty());
        assert(result->casts.front().id.value.starts_with("0x"));
    };
    querySemantic(application::read::model::FeedQuery::Kind::Nfts);
    querySemantic(application::read::model::FeedQuery::Kind::Developers);
    const auto searches = transport.requestedUrls();
    assert(std::ranges::any_of(searches, [](const auto& url) {
        return url.find("/v2/farcaster/cast/search") != std::string::npos
            && url.find("q=nft") != std::string::npos; }));
    assert(std::ranges::any_of(searches, [](const auto& url) {
        return url.find("/v2/farcaster/cast/search") != std::string::npos
            && url.find("q=github") != std::string::npos; }));
    // Every semantic source is isolated by its own provider-affine cursor key.
}

void testProfileCastAndIdentityEndpoints()
{
    test::hypersnap::transport::FakeTransport transport{};
    const auto config = testConfig();

    transport.enqueue(200, test::hypersnap::fixtures::userResponse());
    transport.enqueue(200, test::hypersnap::fixtures::userDataByFidResponse());
    provider::hypersnap::profile::ProfileSource profiles{transport, config};
    auto profile = profiles.get(domain::identity::Fid{42});
    assert(profile && profile->fid.value == 42);
    assert(profile->profileUrl && *profile->profileUrl == "https://example.test/alice");
    assert(profile->qualityScore && *profile->qualityScore == 0.83);
    assert(transport.urls().size() == 2);
    assert(transport.urls().front() == "https://node.test/v2/farcaster/user?fid=42");
    assert(transport.urls().back() ==
        "https://node.test/v1/userDataByFid?fid=42");

    test::hypersnap::transport::FakeTransport bannerTransport{};
    bannerTransport.enqueue(200, test::hypersnap::fixtures::userWithoutBannerResponse());
    bannerTransport.enqueue(200, test::hypersnap::fixtures::userDataByFidResponse());
    provider::hypersnap::profile::ProfileSource bannerProfiles{bannerTransport, config};
    auto bannerProfile = bannerProfiles.get(domain::identity::Fid{42});
    assert(bannerProfile);
    assert(bannerProfile->bannerUrl);
    assert(*bannerProfile->bannerUrl == "https://cdn.test/alice-banner-fallback.png");
    assert(bannerTransport.urls().back() ==
        "https://node.test/v1/userDataByFid?fid=42");

    test::hypersnap::transport::FakeTransport usernameTransport{};
    usernameTransport.enqueue(200, test::hypersnap::fixtures::userWithSyntheticUsernameResponse());
    usernameTransport.enqueue(200, test::hypersnap::fixtures::userDataByFidResponse());
    provider::hypersnap::profile::ProfileSource usernameProfiles{usernameTransport, config};
    auto usernameProfile = usernameProfiles.get(domain::identity::Fid{42});
    assert(usernameProfile);
    assert(usernameProfile->username && usernameProfile->username->value == "alice");

    transport.enqueue(200, test::hypersnap::fixtures::userResponse());
    provider::hypersnap::identity::IdentityResolver identities{transport, config};
    auto fid = identities.resolveUsername("alice bob");
    assert(fid && fid->value == 42);
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/user/by-username?username=alice%20bob");

    transport.enqueue(404, R"({"error":"not_found"})");
    transport.enqueue(200, test::hypersnap::fixtures::castResponse());
    provider::hypersnap::cast::CastSource casts{transport, config};
    auto cast = casts.get(domain::cast::CastId{"0xabc/def?"});
    assert(cast && cast->id.value == "0xabc123");
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/cast?identifier=0xabc%2Fdef%3F&type=hash");

    auto emptyCast = casts.get(domain::cast::CastId{});
    assert(!emptyCast);
    assert(emptyCast.error().code == foundation::error::ErrorCode::InvalidArgument);

    auto emptyUsername = identities.resolveUsername("");
    assert(!emptyUsername);
    assert(emptyUsername.error().code == foundation::error::ErrorCode::InvalidArgument);
}

void testHttpErrorMappingAndConfigValidation()
{
    test::hypersnap::transport::FakeTransport transport{};
    auto config = testConfig();
    provider::hypersnap::profile::ProfileSource profiles{transport, config};

    transport.enqueue(429, "{}", {"Retry-After: 0"});
    transport.enqueue(429, "{}", {"Retry-After: 0"});
    transport.enqueue(429, "{}", {"Retry-After: 0"});
    auto rateLimited = profiles.get(domain::identity::Fid{42});
    assert(!rateLimited);
    assert(rateLimited.error().code == foundation::error::ErrorCode::RateLimited);
    assert(rateLimited.error().message.find("Endpoint: /v2/farcaster/user") != std::string::npos);
    assert(rateLimited.error().message.find("Limit: n/a") != std::string::npos);
    assert(rateLimited.error().message.find("Deadline: 15000ms") != std::string::npos);
    assert(rateLimited.error().message.find("Attempts: 3") != std::string::npos);

    transport.enqueue(503, R"({"error":"Feed service not available"})");
    transport.enqueue(200, test::hypersnap::fixtures::userResponse());
    transport.enqueue(404, R"({"error":"user data unavailable"})");
    auto recovered = profiles.get(domain::identity::Fid{42});
    assert(recovered);
    assert(recovered->fid.value == 42U);

    transport.enqueue(503, R"({"error":"Feed service not available"})");
    transport.enqueue(503, R"({"error":"Feed service not available"})");
    transport.enqueue(503, R"({"error":"Feed service not available"})");
    auto unavailable = profiles.get(domain::identity::Fid{42});
    assert(!unavailable);
    assert(unavailable.error().code == foundation::error::ErrorCode::Unavailable);
    assert(unavailable.error().message.find("Endpoint: /v2/farcaster/user") != std::string::npos);
    assert(unavailable.error().message.find("status 503") != std::string::npos);
    assert(unavailable.error().message.find("Feed service not available") != std::string::npos);
    assert(unavailable.error().message.find("Attempts: 3") != std::string::npos);

    config.baseUrl.clear();
    provider::hypersnap::profile::ProfileSource invalid{transport, config};
    auto invalidResult = invalid.get(domain::identity::Fid{42});
    assert(!invalidResult);
    assert(invalidResult.error().code == foundation::error::ErrorCode::InvalidArgument);
}

void testFeedPaginationAffinity()
{
    test::hypersnap::fixtures::TemporaryDatabase database{};
    auto storeResult = storage::continuation::openContinuationStore(database.path());
    assert(storeResult);
    auto store = std::move(*storeResult);

    test::hypersnap::transport::FakeTransport transport{};
    transport.enqueue(200, test::hypersnap::fixtures::feedResponse("raw-feed-cursor+/="));
    provider::hypersnap::feed::FeedSource feeds{transport, store, testConfig()};

    auto first = feeds.following(
        domain::identity::Fid{42},
        capability::pagination::PageRequest{.limit = 2});
    assert(first);
    assert(first->casts.size() == 1);
    assert(first->profiles.size() == 1);
    assert(first->entries.size() == 1);
    assert(first->next);
    assert(first->next->value.find("raw-feed-cursor") == std::string::npos);
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/feed/following?fid=42&limit=2");

    assert(transport.requests().back().timeout == std::chrono::milliseconds{12'000});

    auto binding = store.resolve(*first->next);
    assert(binding);
    assert(binding->providerCursor == "raw-feed-cursor+/=");
    assert(binding->providerId.value == "hypersnap-test");
    assert(binding->capabilityId.value == capability::catalog::feedFollowingRead().value);

    // A provider that repeats the same raw cursor has made no pagination
    // progress. The adapter terminates the sequence instead of rebinding that
    // cursor to an endless chain of new opaque continuation tokens.
    transport.enqueue(200, test::hypersnap::fixtures::feedResponse("raw-feed-cursor+/="));
    auto second = feeds.following(
        domain::identity::Fid{42},
        capability::pagination::PageRequest{
            .limit = 2,
            .continuation = first->next,
        });
    assert(second);
    assert(!second->next);
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/feed/following?fid=42&limit=2&cursor=raw-feed-cursor%2B%2F%3D");

    const auto requestCount = transport.urls().size();
    auto wrongCapability = feeds.trending(capability::pagination::PageRequest{
        .limit = 2,
        .continuation = first->next,
    });
    assert(!wrongCapability);
    assert(wrongCapability.error().code == foundation::error::ErrorCode::Conflict);
    assert(transport.urls().size() == requestCount);

    auto invalidLimit = feeds.trending(capability::pagination::PageRequest{.limit = 101});
    assert(!invalidLimit);
    assert(invalidLimit.error().code == foundation::error::ErrorCode::InvalidArgument);

    // Compatibility aliases are used only when the explicit endpoint is not
    // implemented by a node. Transient failures retry the same endpoint and do
    // not fan out to equivalent aliases.
    transport.enqueue(404, R"({"error":"not found"})");
    transport.enqueue(200, test::hypersnap::fixtures::feedResponse(""));
    auto trendingCompatibilityFallback = feeds.trending(
        capability::pagination::PageRequest{.limit = 10});
    assert(trendingCompatibilityFallback && trendingCompatibilityFallback->casts.size() == 1U);
    assert(transport.urls()[transport.urls().size() - 2] ==
        "https://node.test/v2/farcaster/feed/trending?limit=10");
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/feed?feed_type=trending&limit=10");

    const auto beforeTransientFailure = transport.urls().size();
    transport.enqueue(503, R"({"error":"Trending temporarily unavailable"})");
    transport.enqueue(503, R"({"error":"Trending temporarily unavailable"})");
    transport.enqueue(503, R"({"error":"Trending temporarily unavailable"})");
    auto trendingUnavailable = feeds.trending(
        capability::pagination::PageRequest{.limit = 10});
    assert(!trendingUnavailable);
    assert(trendingUnavailable.error().code == foundation::error::ErrorCode::Unavailable);
    assert(trendingUnavailable.error().message.find(
        "Endpoint: /v2/farcaster/feed/trending") != std::string::npos);
    assert(trendingUnavailable.error().message.find("Limit: 10") != std::string::npos);
    assert(trendingUnavailable.error().message.find("Deadline: 12000ms") != std::string::npos);
    assert(trendingUnavailable.error().message.find("Attempts: 3") != std::string::npos);
    assert(transport.urls().size() == beforeTransientFailure + 3U);
    for (std::size_t index = beforeTransientFailure; index < transport.urls().size(); ++index) {
        assert(transport.urls()[index] ==
            "https://node.test/v2/farcaster/feed/trending?limit=10");
    }

    const auto beforeEmptyTrending = transport.urls().size();
    transport.enqueue(200, R"({"casts":[],"next":{"cursor":null}})");
    auto emptyTrending = feeds.trending(
        capability::pagination::PageRequest{.limit = 10});
    assert(emptyTrending);
    assert(emptyTrending->casts.empty());
    assert(transport.urls().size() == beforeEmptyTrending + 1U);
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/feed/trending?limit=10");

    const auto beforeEmptyFollowing = transport.urls().size();
    transport.enqueue(200, R"({"casts":[],"next":{"cursor":null}})");
    auto emptyFollowing = feeds.following(
        domain::identity::Fid{42},
        capability::pagination::PageRequest{.limit = 10});
    assert(emptyFollowing);
    assert(emptyFollowing->casts.empty());
    assert(transport.urls().size() == beforeEmptyFollowing + 1U);
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/feed/following?fid=42&limit=10");

    transport.enqueue(200, test::hypersnap::fixtures::channelFeedResponse());
    auto channel = feeds.channel(
        domain::channel::ChannelId{"dev/chat"},
        capability::pagination::PageRequest{.limit = 5});
    assert(channel);
    assert(channel->casts.size() == 1);
    assert(channel->casts.front().parentUrl ==
        std::optional<std::string>{"https://farcaster.group/dev/chat"});
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/feed/channels?channel_ids=dev%2Fchat&limit=5");
    assert(transport.requests().back().timeout == std::chrono::milliseconds{12'000});

    transport.enqueue(200, test::hypersnap::fixtures::rootPostFeedResponse());
    auto userPosts = feeds.userPosts(
        domain::identity::Fid{42},
        capability::pagination::PageRequest{.limit = 10});
    assert(userPosts && userPosts->casts.size() == 1);
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/feed/user/casts?fid=42&limit=10");
    assert(transport.requests().back().timeout == std::chrono::milliseconds{12'000});

    transport.enqueue(200, test::hypersnap::fixtures::quoteFeedResponse());
    auto userQuotes = feeds.userQuotes(
        domain::identity::Fid{42},
        capability::pagination::PageRequest{.limit = 10});
    assert(userQuotes && userQuotes->casts.size() == 1);

    transport.enqueue(200, test::hypersnap::fixtures::repliesWithoutMetricsFeedResponse());
    transport.enqueue(200, test::hypersnap::fixtures::conversationResponse());
    auto userReplies = feeds.userReplies(
        domain::identity::Fid{42},
        capability::pagination::PageRequest{.limit = 10});
    assert(userReplies);
    assert(userReplies->casts.size() == 1);
    assert(userReplies->casts.front().parent.has_value());
    assert(userReplies->casts.front().engagement.replies == 2U);
    assert(userReplies->entries.size() == 1);
    assert(transport.urls()[transport.urls().size() - 2] ==
        "https://node.test/v2/farcaster/feed/user/replies_and_recasts?fid=42&limit=10");
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/cast/conversation?identifier=0xabc123&type=hash&reply_depth=1");

    transport.enqueue(200, test::hypersnap::fixtures::reactionListResponse("like-cursor"));
    transport.enqueue(200, test::hypersnap::fixtures::castResponse());
    auto userLikes = feeds.userLikes(
        domain::identity::Fid{42},
        capability::pagination::PageRequest{.limit = 6});
    assert(userLikes && userLikes->casts.size() == 1 && userLikes->next);
    assert(transport.urls()[transport.urls().size() - 2] ==
        "https://node.test/v2/farcaster/reaction/user?fid=42&type=likes&limit=6");
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/cast?identifier=0xabc123&type=hash");

    transport.enqueue(200, test::hypersnap::fixtures::reactionListResponse(""));
    transport.enqueue(200, test::hypersnap::fixtures::reactionListResponse(""));
    auto viewerReactions = feeds.viewerReactions(domain::identity::Fid{42}, 100);
    assert(viewerReactions && viewerReactions->liked.size() == 1 && viewerReactions->recasted.size() == 1);
    assert(transport.urls()[transport.urls().size() - 2] ==
        "https://node.test/v2/farcaster/reaction/user?fid=42&type=likes&limit=100");
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/reaction/user?fid=42&type=recasts&limit=100");

    transport.enqueue(200, test::hypersnap::fixtures::reactionListResponse("", "0xabc123"));
    transport.enqueue(200, test::hypersnap::fixtures::reactionListResponse("", "0xdef456"));
    const std::vector<domain::cast::CastReference> exactTargets{
        {.author = domain::identity::Fid{42}, .id = domain::cast::CastId{"0xabc123"}},
        {.author = domain::identity::Fid{7}, .id = domain::cast::CastId{"0xdef456"}},
    };
    auto exactReactions = feeds.viewerReactionsFor(domain::identity::Fid{42}, exactTargets);
    assert(exactReactions);
    assert(exactReactions->liked.size() == 1U);
    assert(exactReactions->liked.front().value == "0xabc123");
    assert(exactReactions->recasted.size() == 1U);
    assert(exactReactions->recasted.front().value == "0xdef456");
    assert(transport.urls()[transport.urls().size() - 2] ==
        "https://node.test/v2/farcaster/reaction/user?fid=42&type=likes&limit=10000");
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/reaction/user?fid=42&type=recasts&limit=10000");

    transport.enqueue(200, test::hypersnap::fixtures::feedResponse(""));
    auto userPopular = feeds.userPopular(
        domain::identity::Fid{42},
        capability::pagination::PageRequest{.limit = 10});
    assert(userPopular);
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/feed/user/popular?fid=42&limit=10");

    provider::hypersnap::notification::NotificationSource notifications{transport, store, testConfig()};
    transport.enqueue(200, test::hypersnap::fixtures::notificationResponse("notify-next"));
    auto notificationPage = notifications.notifications(
        domain::identity::Fid{42}, capability::pagination::PageRequest{.limit = 20});
    assert(notificationPage && notificationPage->items.size() == 1U && notificationPage->next);
    assert(notificationPage->items.front().kind == capability::notification::NotificationKind::Reply);
    assert(notificationPage->items.front().cast.has_value());
    assert(notificationPage->items.front().actors.size() == 1U);
    assert(notificationPage->items.front().actors.front().fid.value == 7U);
    assert(notificationPage->items.front().observedAt.time_since_epoch().count() > 0);
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/notifications?fid=42&limit=20");
}

void testSocialGraphSemantics()
{
    test::hypersnap::fixtures::TemporaryDatabase database{};
    auto storeResult = storage::continuation::openContinuationStore(database.path());
    assert(storeResult);
    auto store = std::move(*storeResult);

    test::hypersnap::transport::FakeTransport transport{};
    provider::hypersnap::social::SocialGraphSource graph{transport, store, testConfig()};

    transport.enqueue(200, test::hypersnap::fixtures::socialResponse(7, "bob"));
    auto followers = graph.followers(
        domain::identity::Fid{42},
        capability::pagination::PageRequest{.limit = 10});
    assert(followers && followers->relations.size() == 1);
    assert(followers->relations.front().source.value == 7);
    assert(followers->relations.front().target.value == 42);
    assert(followers->relations.front().kind == domain::social::SocialRelationKind::Follows);
    assert(transport.urls().back() ==
        "https://node.test/v2/farcaster/user/followers?fid=42&limit=10");

    transport.enqueue(200, test::hypersnap::fixtures::socialResponse(9, "carol"));
    auto following = graph.following(
        domain::identity::Fid{42},
        capability::pagination::PageRequest{.limit = 10});
    assert(following && following->relations.size() == 1);
    assert(following->relations.front().source.value == 42);
    assert(following->relations.front().target.value == 9);

    transport.enqueue(200, "{}");
    auto followsCarol = graph.isFollowing(domain::identity::Fid{42}, domain::identity::Fid{9});
    assert(followsCarol && *followsCarol);
    assert(transport.urls().back() ==
        "https://node.test/v1/linkById?fid=42&target_fid=9&link_type=follow");

    transport.enqueue(404, R"({"error":"not_found"})");
    auto doesNotFollowBob = graph.isFollowing(domain::identity::Fid{42}, domain::identity::Fid{7});
    assert(doesNotFollowBob && !*doesNotFollowBob);
}

void testSearchSemantics()
{
    test::hypersnap::transport::FakeTransport transport{};
    provider::hypersnap::search::SearchSource search{transport, testConfig()};

    transport.enqueue(200, R"json({
      "users": [{
        "object": "user",
        "fid": 42,
        "username": "alice",
        "display_name": "Alice",
        "pfp_url": "https://cdn.test/alice.png",
        "profile": {"bio": {"text": "builder"}},
        "follower_count": 12,
        "following_count": 8
      }]
    })json");
    transport.enqueue(200, R"json({
      "channels": [{
        "object": "channel",
        "id": "dev",
        "url": "https://warpcast.com/~/channel/dev",
        "name": "dev",
        "image_url": "https://cdn.test/dev.png",
        "description": "Developer conversations",
        "created_at": "2026-08-27T00:00:00.000Z",
        "follower_count": 99
      }],
      "next": {"cursor": null}
    })json");

    const auto result = search.search(" dev ", 4);
    assert(result);
    assert(result->profiles.size() == 1U);
    assert(result->profiles.front().fid.value == 42U);
    assert(result->channels.size() == 1U);
    assert(result->channels.front().id.value == "dev");
    assert(result->channels.front().followerCount == 99U);
    assert(transport.urls().size() == 2U);
    assert(transport.urls()[0] == "https://node.test/v2/farcaster/user/search?q=dev&limit=4");
    assert(transport.urls()[1] == "https://node.test/v2/farcaster/channel/search?q=dev&limit=4");
}

void testCurlTransportConstruction()
{
    auto transport = farcaster::transport::curl::makeTransport();
    assert(transport);
    assert(*transport != nullptr);
}

void testCapabilityRegistration()
{
    const auto capabilities = provider::hypersnap::registration::capabilities();
    assert(capabilities.size() == 19);

    const auto has = [&](const capability::model::CapabilityId& id) {
        return std::ranges::any_of(capabilities, [&](const auto& item) {
            return item.id.value == id.value;
        });
    };

    assert(has(capability::catalog::identityFidResolve()));
    assert(has(capability::catalog::profileRead()));
    assert(has(capability::catalog::castRead()));
    assert(has(capability::catalog::graphFollowersRead()));
    assert(has(capability::catalog::graphFollowingRead()));
    assert(has(capability::catalog::feedFollowingRead()));
    assert(has(capability::catalog::feedTrendingRead()));
    assert(has(capability::catalog::feedRecentRead()));
    assert(has(capability::catalog::feedConversationsRead()));
    assert(has(capability::catalog::feedNftsRead()));
    assert(has(capability::catalog::feedDevelopersRead()));
    assert(has(capability::catalog::feedChannelRead()));
    assert(has(capability::catalog::feedUserPostsRead()));
    assert(has(capability::catalog::feedUserQuotesRead()));
    assert(has(capability::catalog::feedUserRepliesRead()));
    assert(has(capability::catalog::feedUserLikesRead()));
    assert(has(capability::catalog::feedUserPopularRead()));
    assert(has(capability::catalog::searchRead()));
    assert(has(capability::catalog::notificationsRead()));
    assert(!has(capability::catalog::feedForYouRead()));

    for (const auto& item : capabilities) {
        assert(item.isSelfHostable);
        assert(item.quality == capability::model::CapabilityQuality::Native);
    }
}

}

int main()
{
    testCanonicalCodec();
    testRecentNetworkEventDecoding();
    testRecentAsyncFeedAndCursor();
    testProfileCastAndIdentityEndpoints();
    testHttpErrorMappingAndConfigValidation();
    testFeedPaginationAffinity();
    testSocialGraphSemantics();
    testSearchSemantics();
    testCapabilityRegistration();
    testCurlTransportConstruction();
    return 0;
}
