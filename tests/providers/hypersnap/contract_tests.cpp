#include <algorithm>
#include <atomic>
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
import farcaster.provider.hypersnap.async_request;
import farcaster.capability.catalog;
import farcaster.capability.feed;
import farcaster.capability.model;
import farcaster.capability.pagination;
import farcaster.capability.notification;
import farcaster.domain.cast;
import farcaster.domain.feed;
import farcaster.domain.channel;
import farcaster.domain.identity;
import farcaster.domain.social;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.cast;
import farcaster.provider.hypersnap.codec;
import farcaster.provider.hypersnap.config;
import farcaster.provider.hypersnap.continuation;
import farcaster.provider.hypersnap.feed;
import farcaster.provider.hypersnap.identity;
import farcaster.provider.hypersnap.notification;
import farcaster.provider.hypersnap.profile;
import farcaster.provider.hypersnap.registration;
import farcaster.provider.hypersnap.request;
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
    // HyperSnap gateways may JSON-encode 64-bit FIDs as decimal strings.
    const auto stringFid = provider::hypersnap::codec::decodeUser(
        R"json({"user":{"fid":"4294967296","username":"fid:4294967296"}})json");
    assert(stringFid && stringFid->fid.value == 4294967296ULL);
    assert(!stringFid->username && "node-generated fid: placeholders are not usernames");
    for (const auto malformed : {
             R"json({"user":{"fid":"-1"}})json",
             R"json({"user":{"fid":"1.5"}})json",
             R"json({"user":{"fid":"18446744073709551616"}})json",
             R"json({"user":{"fid":" 42"}})json",
             R"json({"user":{"fid":"42abc"}})json"}) {
        assert(!provider::hypersnap::codec::decodeUser(malformed));
    }
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
    assert(user->socialCountsKnown);

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
    assert(!cast->sourceClient); // Never infer an app from the author or node.
    std::string withClient = test::hypersnap::fixtures::castResponse();
    const auto hashAt = withClient.find("\"hash\"");
    assert(hashAt != std::string::npos);
    withClient.insert(hashAt, R"json("app":{"name":"Quorum"},)json");
    const auto attributed = provider::hypersnap::codec::decodeCast(withClient);
    assert(attributed && attributed->sourceClient && *attributed->sourceClient == "Quorum");
    // The display name takes precedence only when explicitly returned by the
    // selected provider. Both the regular feed and reply projections preserve it.
    auto withDisplayName = withClient;
    const std::string appName{R"json("app":{"name":"Quorum"})json"};
    const auto appOffset = withDisplayName.find(appName);
    assert(appOffset != std::string::npos);
    withDisplayName.replace(appOffset, appName.size(),
        R"json("app":{"name":"Quorum","display_name":"Genycaster"})json");
    const auto namedClient = provider::hypersnap::codec::decodeCast(withDisplayName);
    assert(namedClient && namedClient->sourceClient
           && *namedClient->sourceClient == "Genycaster");

    // Empty and unreasonable attribution data cannot create client badges.
    auto emptyClient = withClient;
    const auto emptyAt = emptyClient.find(appName);
    assert(emptyAt != std::string::npos);
    emptyClient.replace(emptyAt, appName.size(), R"json("app":{})json");
    const auto unknownClient = provider::hypersnap::codec::decodeCast(emptyClient);
    assert(unknownClient && !unknownClient->sourceClient);

    auto excessiveClient = withClient;
    const auto excessiveAt = excessiveClient.find(appName);
    assert(excessiveAt != std::string::npos);
    excessiveClient.replace(excessiveAt, appName.size(),
        std::string{R"json("app":{"name":")json"} + std::string(60, 'X')
            + R"json("})json");
    const auto untrustedClient = provider::hypersnap::codec::decodeCast(excessiveClient);
    assert(untrustedClient && !untrustedClient->sourceClient);

    // Protocol text has no human-visible @handle. Farcaster positions count
    // UTF-8 bytes (the 💯 symbol consumes four bytes), not UTF-16 indexes.
    std::string withMention = test::hypersnap::fixtures::castResponse();
    const auto textAt = withMention.find("hello native");
    assert(textAt != std::string::npos);
    withMention.replace(textAt, std::string{"hello native"}.size(), "Right! 💯 ");
    const auto profilesAt = withMention.find("\"mentioned_profiles\": []");
    assert(profilesAt != std::string::npos);
    withMention.replace(profilesAt, std::string{"\"mentioned_profiles\": []"}.size(),
        "\"mentioned_profiles\": [{\"fid\": 12345,\"username\":\"vrypan.eth\"}],"
        "\"mentions_positions\": [12]");
    auto hydrated = provider::hypersnap::codec::decodeCast(withMention);
    assert(hydrated);
    assert(hydrated->text == "Right! 💯 ");
    assert(hydrated->displayText && *hydrated->displayText == "Right! 💯 @vrypan.eth");
    // A self-hosted node may return canonical mention FIDs without enriched
    // username objects. Make the mention visible and navigable by FID,
    // preserving exact signed bytes and respecting the UTF-8 boundary.
    auto unhydrated = withMention;
    const auto profileStart = unhydrated.find(R"json("mentioned_profiles": [{"fid": 12345,"username":"vrypan.eth"}])json");
    assert(profileStart != std::string::npos);
    unhydrated.replace(profileStart,
        std::string{R"json("mentioned_profiles": [{"fid": 12345,"username":"vrypan.eth"}])json"}.size(),
        R"json("mentions": [12345], "mentioned_profiles": [])json");
    const auto fidProjection = provider::hypersnap::codec::decodeCast(unhydrated);
    assert(fidProjection && fidProjection->text == "Right! 💯 ");
    assert(fidProjection->displayText && *fidProjection->displayText == "Right! 💯 @fid:12345");
    auto splitUtf8 = unhydrated;
    const auto validPosition = splitUtf8.find(R"json("mentions_positions": [12])json");
    assert(validPosition != std::string::npos);
    splitUtf8.replace(validPosition, std::string{R"json("mentions_positions": [12])json"}.size(),
        R"json("mentions_positions": [9])json");
    const auto badFidOffset = provider::hypersnap::codec::decodeCast(splitUtf8);
    assert(badFidOffset && !badFidOffset->displayText);

    // Real v2 cast/bulk payload shape: the search index drops the mention
    // projection, but canonical UTF-8 range 12 belongs after Right! + emoji
    // while the original signed text also contains an X URL embed.
    std::string exactXCast = test::hypersnap::fixtures::castResponse();
    const std::string originalJsonText{R"json("text": "hello native")json"};
    const auto exactTextAt = exactXCast.find(originalJsonText);
    assert(exactTextAt != std::string::npos);
    exactXCast.replace(exactTextAt, originalJsonText.size(),
        R"json("text": "Right! 💯  \n\nhttps://x.com/vrypan/status/2113262913767317648?s=46")json");
    const std::string emptyMentionList{R"json("mentioned_profiles": [])json"};
    const auto exactProfilesAt = exactXCast.find(emptyMentionList);
    assert(exactProfilesAt != std::string::npos);
    exactXCast.replace(exactProfilesAt, emptyMentionList.size(),
        R"json("mentioned_profiles": [{"fid":280,"username":"vrypan.eth"}])json");
    const std::string emptyRanges{R"json("mentioned_profiles_ranges": [])json"};
    const auto exactRangesAt = exactXCast.find(emptyRanges);
    assert(exactRangesAt != std::string::npos);
    exactXCast.replace(exactRangesAt,emptyRanges.size(),
        R"json("mentioned_profiles_ranges": [{"start":12,"end":12}])json");
    const auto precise = provider::hypersnap::codec::decodeCast(exactXCast);
    assert(precise && precise->text == "Right! 💯  \n\nhttps://x.com/vrypan/status/2113262913767317648?s=46");
    assert(precise->displayText);
    assert(precise->displayText->find("Right! 💯 @vrypan.eth") == 0U);
    assert(precise->displayText->find("https://x.com/vrypan/status/") != std::string::npos);

    // The same cast in the public search index is deliberately sparse.
    // It must not claim to contain a mention until cast/bulk hydrates it.
    auto sparseExact = exactXCast;
    const std::string hydratedProfiles{R"json("mentioned_profiles": [{"fid":280,"username":"vrypan.eth"}])json"};
    const auto sparseProfilesAt = sparseExact.find(hydratedProfiles);
    assert(sparseProfilesAt != std::string::npos);
    sparseExact.replace(sparseProfilesAt, hydratedProfiles.size(), emptyMentionList);
    const std::string hydratedRanges{R"json("mentioned_profiles_ranges": [{"start":12,"end":12}])json"};
    const auto sparseRangesAt = sparseExact.find(hydratedRanges);
    assert(sparseRangesAt != std::string::npos);
    sparseExact.replace(sparseRangesAt, hydratedRanges.size(), emptyRanges);
    const auto sparseRead = provider::hypersnap::codec::decodeCast(sparseExact);
    assert(sparseRead && sparseRead->text == precise->text);
    assert(!sparseRead->displayText);

    // Preserve an unmodified fixture for subsequent valid-range tests.
    const auto validWithMention = withMention;
    // Invalid positions within a UTF-8 code point must never split an emoji.
    const auto badAt = withMention.find("\"mentions_positions\": [12]");
    assert(badAt != std::string::npos);
    withMention.replace(badAt, std::string{"\"mentions_positions\": [12]"}.size(),
        "\"mentions_positions\": [9]");
    auto invalidMention = provider::hypersnap::codec::decodeCast(withMention);
    assert(invalidMention && !invalidMention->displayText);

    // Hydrated ranges use character boundaries after the multi-byte 💯.
    auto ranged = validWithMention;
    const auto rangeAt = ranged.find(R"json("mentions_positions": [12])json");
    assert(rangeAt != std::string::npos);
    ranged.replace(rangeAt, std::string{R"json("mentions_positions": [12])json"}.size(),
                   R"json("mentioned_profiles_ranges": [{"start": 9, "end": 9}])json");
    const auto byCharacters = provider::hypersnap::codec::decodeCast(ranged);
    assert(byCharacters && byCharacters->displayText);
    assert(*byCharacters->displayText == "Right! 💯 @vrypan.eth");

    std::string withoutPositions = validWithMention;
    const auto omittedAt = withoutPositions.find(R"json(,"mentions_positions": [12])json");
    assert(omittedAt != std::string::npos);
    withoutPositions.erase(omittedAt, std::string{R"json(,"mentions_positions": [12])json"}.size());
    const auto recovered = provider::hypersnap::codec::decodeCast(withoutPositions);
    assert(recovered && recovered->displayText);
    assert(*recovered->displayText == "Right! 💯 @vrypan.eth");

    std::string hydratedChannel = test::hypersnap::fixtures::castResponse();
    const std::string targetText{R"json("text": "hello native")json"};
    const auto channelAt = hydratedChannel.find(targetText);
    assert(channelAt != std::string::npos);
    hydratedChannel.insert(channelAt,
        R"json("channel": {"id":"test","name":"Testing Channel","image_url":"https://cdn.test/channel.png"},)json");
    const auto decodedChannel = provider::hypersnap::codec::decodeCast(hydratedChannel);
    assert(decodedChannel && decodedChannel->channelId && *decodedChannel->channelId == "test");
    assert(decodedChannel->channelName && *decodedChannel->channelName == "Testing Channel");
    assert(decodedChannel->channelImageUrl && *decodedChannel->channelImageUrl == "https://cdn.test/channel.png");
    // Neynar ranges already pointing to a visible username must not double it.
    std::string visibleMention = test::hypersnap::fixtures::castResponse();
    const auto visibleTextAt = visibleMention.find(targetText);
    assert(visibleTextAt != std::string::npos);
    visibleMention.replace(visibleTextAt,targetText.size(),R"json("text": "Hello @vrypan.eth!")json");
    const std::string emptyProfiles{R"json("mentioned_profiles": [])json"};
    const auto position = visibleMention.find(emptyProfiles);
    assert(position != std::string::npos);
    visibleMention.replace(position,emptyProfiles.size(),
        R"json("mentioned_profiles": [{"fid":12345,"username":"vrypan.eth"}], "mentioned_profiles_ranges": [{"start":6,"end":17}])json");
    const auto intact = provider::hypersnap::codec::decodeCast(visibleMention);
    assert(intact && intact->text == "Hello @vrypan.eth!");
    assert(!intact->displayText);

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

    // A malformed nested cast must fail truthfully, rather than returning
    // an apparently complete conversation with silently missing replies.
    auto malformedConversation = test::hypersnap::fixtures::conversationResponse();
    const std::string malformedHash{R"json("hash": "0xreply-one")json"};
    const auto malformedOffset = malformedConversation.find(malformedHash);
    assert(malformedOffset != std::string::npos);
    malformedConversation.replace(malformedOffset, malformedHash.size(), R"json("hash": null)json");
    const auto incomplete = provider::hypersnap::codec::decodeConversationPayload(
        malformedConversation);
    assert(!incomplete);
    assert(incomplete.error().code == foundation::error::ErrorCode::ProtocolFailure);

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
    assert(replies->front().parentHash && *replies->front().parentHash == "0xdead");
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
    explicit RecentAsyncTransport(
        const bool probeParallelShards = false, const bool corruptSecondShard = false,
        const bool partialReactionPages = false,
        const bool channelFeedUnavailable = false)
        : m_probeParallelShards{probeParallelShards},
          m_corruptSecondShard{corruptSecondShard},
          m_partialReactionPages{partialReactionPages},
          m_channelFeedUnavailable{channelFeedUnavailable},
          m_worker{[this] { drain(); }} {}
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
            if (m_probeParallelShards) {
                if (request.url.find("/v1/events") != std::string::npos
                    && request.url.find("shard_index=1") != std::string::npos) {
                    m_firstShardScheduled = true;
                }
                if (request.url.find("/v1/events") != std::string::npos
                    && request.url.find("shard_index=2") != std::string::npos
                    && m_firstShardScheduled && !m_firstShardCompleted) {
                    m_parallelShardsObserved = true;
                }
            }
            m_pending.emplace_back(std::move(request), std::move(completion));
        }
        m_cv.notify_all();
        return transport::async_http::Operation{[] {}};
    }

    [[nodiscard]] std::vector<std::string> requestedUrls() const {
        const std::scoped_lock lock{m_mutex};
        return m_urls;
    }

    [[nodiscard]] bool parallelShardsObserved() const {
        const std::scoped_lock lock{m_mutex};
        return m_parallelShardsObserved;
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
            if (m_probeParallelShards
                && pending.first.url.find("/v1/events") != std::string::npos
                && pending.first.url.find("shard_index=1") != std::string::npos) {
                std::unique_lock lock{m_mutex};
                m_cv.wait_for(lock, std::chrono::milliseconds{400}, [this] {
                    return m_parallelShardsObserved || m_closed;
                });
                m_firstShardCompleted = true;
            }
            const bool corruptShard = m_corruptSecondShard
                && pending.first.url.find("/v1/events") != std::string::npos
                && pending.first.url.find("shard_index=2") != std::string::npos;
            std::string responseBody = corruptShard
                ? R"json({"events":"malformed-shard"})json"
                : responseFor(pending.first.url);
            if (m_partialReactionPages
                && pending.first.url.find("/v2/farcaster/reaction/user") != std::string::npos) {
                responseBody = pending.first.url.find("type=likes") != std::string::npos
                    ? R"json({"reactions":[{"cast":{"hash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","fid":1}}],"next":{"cursor":"page-2"}})json"
                    : R"json({"reactions":[],"next":null})json";
            }
            const bool unsupportedChannel = m_channelFeedUnavailable
                && pending.first.url.find("/v2/farcaster/feed/channels")
                    != std::string::npos;
            auto response = transport::http::HttpResponse{
                .statusCode = static_cast<std::uint16_t>(unsupportedChannel ? 404U : 200U),
                .body = unsupportedChannel ? R"json({"error":"not found"})json"
                                           : std::move(responseBody),
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
        if (url.find("/v2/farcaster/feed/channels") != std::string::npos) {
            if (url.find("channel_ids=dev") != std::string::npos)
                return test::hypersnap::fixtures::rootPostFeedResponse();
            auto cast = test::hypersnap::fixtures::rootPostFeedResponse();
            const bool cryptoart = url.find("channel_ids=cryptoart&") != std::string::npos;
            const bool objkt = url.find("channel_ids=objkt&") != std::string::npos;
            if (!cryptoart && !objkt)
                return R"json({"casts":[],"next":null})json";
            const bool next = url.find("cursor=crypto-next") != std::string::npos;
            const std::string hash = cryptoart
                ? (next ? "0xcccccccccccccccccccccccccccccccccccccccc"
                        : "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
                : "0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
            cast.replace(cast.find("0xabc123"), std::string{"0xabc123"}.size(), hash);
            const std::string timestamp = cryptoart
                ? (next ? "2026-08-28T12:34:56.789Z" : "2026-08-30T12:34:56.789Z")
                : "2026-08-29T12:34:56.789Z";
            cast.replace(cast.find("2026-08-27T12:34:56.789Z"),
                std::string{"2026-08-27T12:34:56.789Z"}.size(), timestamp);
            if (cryptoart && !next) {
                const auto cursor = cast.find(R"json("cursor":null)json");
                cast.replace(cursor, std::string{R"json("cursor":null)json"}.size(),
                    R"json("cursor":"crypto-next")json");
            }
            return cast;
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
    bool m_probeParallelShards{};
    bool m_corruptSecondShard{};
    bool m_partialReactionPages{};
    bool m_channelFeedUnavailable{};
    bool m_firstShardScheduled{};
    bool m_firstShardCompleted{};
    bool m_parallelShardsObserved{};
    std::thread m_worker;
};

void testDecodeAdmissionFailureCompletesOnce()
{
    RecentAsyncTransport transport{};
    // The decoded body cannot fit this one-byte queue weight budget. The
    // completion must survive its move into the (rejected) executor job.
    execution::weighted_thread_pool::WeightedThreadPoolExecutor decode{1, 1, 1};
    provider::hypersnap::async_request::Client client{transport, decode, testConfig()};
    std::promise<foundation::result::Result<std::string>> delivered{};
    auto ready = delivered.get_future();
    std::atomic_int completions{0};
    auto request = client.getJson("/v1/info", {},
        [&delivered, &completions](foundation::result::Result<std::string> result) {
            completions.fetch_add(1, std::memory_order_relaxed);
            delivered.set_value(std::move(result));
        });
    assert(request);
    assert(ready.wait_for(std::chrono::seconds{3}) == std::future_status::ready);
    auto result = ready.get();
    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::RateLimited);
    assert(completions.load(std::memory_order_relaxed) == 1);
}

void testRejectedAsyncDecodeDeliversExactlyOnce()
{
    // A two-byte response exceeds the one-byte weighted decode budget.
    // The original implementation moved its callback into the rejected job,
    // then attempted to call a moved-from std::function.
    RecentAsyncTransport transport{};
    execution::weighted_thread_pool::WeightedThreadPoolExecutor decode{1, 1, 1};
    provider::hypersnap::async_request::Client client{transport, decode, testConfig()};
    const auto verify = [&](const bool post) {
        auto delivered = std::make_shared<std::atomic_int>(0);
        auto promised = std::make_shared<
            std::promise<foundation::result::Result<std::string>>>();
        auto future = promised->get_future();
        const auto callback = [delivered, promised](
            foundation::result::Result<std::string> result) mutable {
            if (delivered->fetch_add(1) == 0) {
                promised->set_value(std::move(result));
            }
        };
        auto request = post
            ? client.postReadJson("/v2/farcaster/batch/cast-interactions",
                                 "{}", callback)
            : client.getJson("/v2/farcaster/cast", {}, callback);
        assert(request);
        assert(future.wait_for(std::chrono::seconds{3}) == std::future_status::ready);
        auto result = future.get();
        assert(!result && result.error().code == foundation::error::ErrorCode::RateLimited);
        assert(delivered->load() == 1);
    };
    verify(false);
    verify(true);
}

void testIncompatibleViewerBatchFallsBackWithoutRepeatedProbe()
{
    const auto path = std::filesystem::temp_directory_path() /
        ("genycaster-reaction-probe-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".sqlite3");
    auto continuation = storage::continuation::openContinuationStore(path);
    assert(continuation);
    RecentAsyncTransport transport{};
    execution::weighted_thread_pool::WeightedThreadPoolExecutor decode{1, 32, 4U * 1024U * 1024U};
    execution::serial::SerialExecutor serial{32};
    provider::hypersnap::async_gateway::AsyncGateway gateway{
        transport, decode, serial, *continuation, testConfig()};
    using Snapshot = domain::feed::ViewerReactionSnapshot;
    const auto verifyFailure = [&] {
        auto delivered = std::make_shared<std::promise<foundation::result::Result<Snapshot>>>();
        auto future = delivered->get_future();
        auto request = gateway.viewerReactionsFor(
            domain::identity::Fid{42},
            {domain::cast::CastReference{
                .author = domain::identity::Fid{1},
                .id = domain::cast::CastId{"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
            }},
            [delivered](foundation::result::Result<Snapshot> result) mutable {
                delivered->set_value(std::move(result));
            });
        assert(request);
        assert(future.wait_for(std::chrono::seconds{5}) == std::future_status::ready);
        assert(!future.get()); // No response has sufficient scope for negatives.
    };
    verifyFailure();
    verifyFailure();
    const auto urls = transport.requestedUrls();
    assert(std::ranges::count_if(urls, [](const std::string& url) {
        return url.find("/v2/farcaster/batch/cast-interactions") != std::string::npos;
    }) == 1);
    assert(std::ranges::count_if(urls, [](const std::string& url) {
        return url.find("/v2/farcaster/reaction/user") != std::string::npos;
    }) == 4);
    std::error_code ignored{};
    std::filesystem::remove(path, ignored);
}


void testPartialReactionPagesPreserveIndependentEvidence()
{
    const auto path = std::filesystem::temp_directory_path() /
        ("genycaster-partial-reactions-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".sqlite3");
    auto continuation = storage::continuation::openContinuationStore(path);
    assert(continuation);
    RecentAsyncTransport transport{false, false, true};
    execution::weighted_thread_pool::WeightedThreadPoolExecutor decode{
        1, 32, 4U * 1024U * 1024U};
    execution::serial::SerialExecutor serial{32};
    provider::hypersnap::async_gateway::AsyncGateway gateway{
        transport, decode, serial, *continuation, testConfig()};
    auto promised = std::make_shared<
        std::promise<foundation::result::Result<domain::feed::ViewerReactionSnapshot>>>();
    auto resultFuture = promised->get_future();
    constexpr std::string_view a{"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
    constexpr std::string_view b{"0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"};
    auto request = gateway.viewerReactionsFor(
        domain::identity::Fid{42},
        {
            domain::cast::CastReference{
                .author = domain::identity::Fid{1},
                .id = domain::cast::CastId{std::string{a}},
            },
            domain::cast::CastReference{
                .author = domain::identity::Fid{2},
                .id = domain::cast::CastId{std::string{b}},
            },
        },
        [promised](foundation::result::Result<domain::feed::ViewerReactionSnapshot> result) {
            promised->set_value(std::move(result));
        });
    assert(request);
    assert(resultFuture.wait_for(std::chrono::seconds{5}) == std::future_status::ready);
    auto snapshot = resultFuture.get();
    assert(snapshot);
    assert(snapshot->liked.size() == 1U && snapshot->liked.front().value == a);
    assert(snapshot->recasted.empty());
    assert(snapshot->likeExamined.size() == 1U && snapshot->likeExamined.front().value == a);
    assert(snapshot->recastExamined.size() == 2U);
    assert(snapshot->examined.size() == 1U && snapshot->examined.front().value == a);
    // Cast B is not liked in the first account page: incomplete like coverage
    // cannot establish false, although complete recast coverage does.
    assert(std::ranges::none_of(snapshot->likeExamined,
        [](const domain::cast::CastId& id) {
            return id.value == "0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
        }));
    const auto urls = transport.requestedUrls();
    assert(std::ranges::count_if(urls, [](const std::string& url) {
        return url.find("/v2/farcaster/reaction/user") != std::string::npos;
    }) == 2);
    std::error_code ignored{};
    std::filesystem::remove(path, ignored);
}

void testParallelRecentShardFailureCompletesOnlyOnce()
{
    const auto path = std::filesystem::temp_directory_path() /
        ("genycaster-shard-error-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".sqlite3");
    auto continuation = storage::continuation::openContinuationStore(path);
    assert(continuation);
    RecentAsyncTransport transport{true, true};
    execution::weighted_thread_pool::WeightedThreadPoolExecutor decode{
        1, 32, 4U * 1024U * 1024U};
    execution::serial::SerialExecutor serial{32};
    provider::hypersnap::async_gateway::AsyncGateway gateway{
        transport, decode, serial, *continuation, testConfig()};
    using Result = foundation::result::Result<capability::feed::FeedPage>;
    auto delivered = std::make_shared<std::promise<Result>>();
    auto future = delivered->get_future();
    std::atomic_int completions{};
    auto request = gateway.feed(
        application::read::model::FeedQuery{
            .kind = application::read::model::FeedQuery::Kind::Recent,
            .limit = 1U,
        },
        [delivered, &completions](Result result) mutable {
            if (completions.fetch_add(1, std::memory_order_relaxed) == 0) {
                delivered->set_value(std::move(result));
            }
        });
    assert(request);
    assert(future.wait_for(std::chrono::seconds{5}) == std::future_status::ready);
    const auto result = future.get();
    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::ProtocolFailure);
    assert(transport.parallelShardsObserved());
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    assert(completions.load(std::memory_order_relaxed) == 1);
    std::error_code ignored{};
    std::filesystem::remove(path, ignored);
}

void testRecentAsyncFeedAndCursor()
{
    const auto path = std::filesystem::temp_directory_path() /
        ("genycaster-recent-contract-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".sqlite3");
    auto continuation = storage::continuation::openContinuationStore(path);
    assert(continuation);
    RecentAsyncTransport transport{true};
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
    // The first shard's network completion was deliberately delayed. The
    // second independent shard must already have been admitted.
    assert(transport.parallelShardsObserved());
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
    // Both pages share the same confirmed two-shard event window. Only cast
    // hydration advances; no duplicate event download/sort is needed.
    assert(std::ranges::count_if(paths, [](const auto& url) {
        return url.find("/v1/events?") != std::string::npos; }) == 2);
    assert(std::ranges::count_if(paths, [](const auto& url) {
        return url.find("/v2/farcaster/cast/bulk?") != std::string::npos; }) == 2);
    // Explicit first-page refreshes must not serve a 45s cached shard
    // window; a new cast may have arrived without any head height change.
    auto refreshed = requestPage(std::nullopt);
    assert(refreshed.casts.size() == 1U);
    const auto refreshPaths = transport.requestedUrls();
    assert(std::ranges::count_if(refreshPaths, [](const auto& url) {
        return url.find("/v1/events?") != std::string::npos; }) == 4);
    const auto querySemantic = [&](const application::read::model::FeedQuery::Kind kind,
                                   std::optional<capability::pagination::ContinuationToken> token = std::nullopt) {
        auto pending = std::make_shared<
            std::promise<foundation::result::Result<capability::feed::FeedPage>>>();
        auto future = pending->get_future();
        auto request = gateway.feed(
            application::read::model::FeedQuery{.kind = kind, .limit = 3U,
                                                  .continuation = std::move(token)},
            [pending](foundation::result::Result<capability::feed::FeedPage> page) {
                pending->set_value(std::move(page));
            });
        assert(request);
        assert(future.wait_for(std::chrono::seconds{5}) == std::future_status::ready);
        auto result = future.get();
        assert(result && !result->casts.empty());
        assert(result->casts.front().id.value.starts_with("0x"));
        return result->next;
    };
    // The public bulk index fixture returns B without parent_hash. The chain
    // event still proves it is a reply: Conversations must retain that parent.
    {
        auto pending = std::make_shared<
            std::promise<foundation::result::Result<capability::feed::FeedPage>>>();
        auto future = pending->get_future();
        auto request = gateway.feed(
            application::read::model::FeedQuery{
                .kind = application::read::model::FeedQuery::Kind::Conversations,
                .limit = 4U},
            [pending](foundation::result::Result<capability::feed::FeedPage> page) {
                pending->set_value(std::move(page));
            });
        assert(request);
        assert(future.wait_for(std::chrono::seconds{5}) == std::future_status::ready);
        auto result = future.get();
        assert(result && result->casts.size() == 1U);
        assert(result->casts.front().parent);
        assert(result->casts.front().parent->value == "0xparent");
    }
    const auto requestArtwork = [&](std::optional<capability::pagination::ContinuationToken> token) {
        auto pending = std::make_shared<
            std::promise<foundation::result::Result<capability::feed::FeedPage>>>();
        auto future = pending->get_future();
        auto request = gateway.feed(application::read::model::FeedQuery{
            .kind = application::read::model::FeedQuery::Kind::Nfts,
            .limit = 3U, .continuation = std::move(token)},
            [pending](foundation::result::Result<capability::feed::FeedPage> page) {
                pending->set_value(std::move(page));
            });
        assert(request);
        assert(future.wait_for(std::chrono::seconds{5}) == std::future_status::ready);
        auto page = future.get();
        assert(page);
        return std::move(*page);
    };
    const auto artFirst = requestArtwork(std::nullopt);
    // Stop at an unread channel frontier, even when another source has older
    // ready results. That source's next page may contain a newer post.
    assert(artFirst.casts.size() == 1U && artFirst.next);
    assert(artFirst.casts[0].id.value == "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    const auto artSecond = requestArtwork(artFirst.next);
    assert(artSecond.casts.size() == 2U && !artSecond.next);
    assert(artSecond.casts[0].id.value == "0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    assert(artSecond.casts[1].id.value == "0xcccccccccccccccccccccccccccccccccccccccc");
    assert(artFirst.casts[0].createdAt > artSecond.casts[0].createdAt);
    assert(artSecond.casts[0].createdAt > artSecond.casts[1].createdAt);
    querySemantic(application::read::model::FeedQuery::Kind::Developers);
    const auto searches = transport.requestedUrls();
    assert(std::ranges::count_if(searches, [](const auto& url) {
        return url.find("channel_ids=cryptoart&") != std::string::npos; }) == 2);
    assert(std::ranges::count_if(searches, [](const auto& url) {
        return url.find("channel_ids=objkt&") != std::string::npos; }) == 1);
    assert(std::ranges::none_of(searches, [](const auto& url) {
        return url.find("/v2/farcaster/cast/search") != std::string::npos
            && (url.find("q=mint") != std::string::npos || url.find("q=art") != std::string::npos); }));
    assert(std::ranges::any_of(searches, [](const auto& url) {
        return url.find("/v2/farcaster/feed/channels") != std::string::npos
            && url.find("channel_ids=dev%2Cdevelopers%2Cbuild%2Cprogramming")
                != std::string::npos; }));
    // A node without the chronological channel endpoint must report that
    // NFT source as unavailable rather than silently returning stale search.
    {
        RecentAsyncTransport unavailable{false, false, false, true};
        provider::hypersnap::async_gateway::AsyncGateway compatibilityGateway{
            unavailable, decode, serial, *continuation, testConfig()};
        auto pending = std::make_shared<
            std::promise<foundation::result::Result<capability::feed::FeedPage>>>();
        auto future = pending->get_future();
        auto started = compatibilityGateway.feed(
            application::read::model::FeedQuery{
                .kind = application::read::model::FeedQuery::Kind::Nfts, .limit = 3U},
            [pending](foundation::result::Result<capability::feed::FeedPage> page) {
                pending->set_value(std::move(page));
            });
        assert(started);
        assert(future.wait_for(std::chrono::seconds{5}) == std::future_status::ready);
        const auto unavailablePage = future.get();
        assert(!unavailablePage);
        assert(unavailablePage.error().code == foundation::error::ErrorCode::Unavailable);
        const auto urls = unavailable.requestedUrls();
        assert(std::ranges::none_of(urls, [](const auto& url) {
            return url.find("/v2/farcaster/cast/search") != std::string::npos; }));
        auto devPending = std::make_shared<
            std::promise<foundation::result::Result<capability::feed::FeedPage>>>();
        auto devFuture = devPending->get_future();
        auto devRequest = compatibilityGateway.feed(
            application::read::model::FeedQuery{
                .kind = application::read::model::FeedQuery::Kind::Developers, .limit = 3U},
            [devPending](foundation::result::Result<capability::feed::FeedPage> page) {
                devPending->set_value(std::move(page));
            });
        assert(devRequest);
        assert(devFuture.wait_for(std::chrono::seconds{5}) == std::future_status::ready);
        const auto devPage = devFuture.get();
        assert(devPage && !devPage->casts.empty());
        const auto fallbackUrls = unavailable.requestedUrls();
        assert(std::ranges::any_of(fallbackUrls, [](const auto& url) {
            return url.find("/v2/farcaster/cast/search") != std::string::npos
                && url.find("q=github") != std::string::npos; }));
    }
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

    // This is the live native-registry response shape: decimal-string FID,
    // server-assigned fid: locator, no editable public metadata yet.
    test::hypersnap::transport::FakeTransport nativeTransport{};
    nativeTransport.enqueue(200,
        R"json({"user":{"fid":"4294967296","username":"fid:4294967296"}})json");
    nativeTransport.enqueue(200,
        R"json({"messages":[{"data":{"userDataBody":{"type":"USER_DATA_TYPE_USERNAME","value":"fid:4294967296"}}}]})json");
    provider::hypersnap::profile::ProfileSource nativeProfiles{nativeTransport, config};
    const auto nativeProfile = nativeProfiles.get(domain::identity::Fid{4294967296ULL});
    assert(nativeProfile && nativeProfile->fid.value == 4294967296ULL);
    assert(!nativeProfile->username);
    assert(nativeTransport.urls().front()
        == "https://node.test/v2/farcaster/user?fid=4294967296");

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

void testIndexedEndpointBoundContinuation()
{
    test::hypersnap::fixtures::TemporaryDatabase database{};
    auto backing = storage::continuation::openContinuationStore(database.path());
    assert(backing);
    const auto id = provider::model::ProviderId{"stable-node-id"};
    const auto capabilityId = capability::catalog::feedUserPostsRead();

    provider::hypersnap::continuation::Store first{
        *backing, id, "https://first-operator.example", "fid:42"};
    auto token = first.bind(capabilityId, std::string{"opaque+/cursor"});
    assert(token && *token);
    auto page = first.resolve(capabilityId, {
        .limit = 12,
        .continuation = *token,
    });
    assert(page && *page && **page == "opaque+/cursor");

    // A reconfigured node with the same ID must reject the old cursor without
    // making a network request to a different operator.
    provider::hypersnap::continuation::Store differentEndpoint{
        *backing, id, "https://second-operator.example", "fid:42"};
    auto retargeted = differentEndpoint.resolve(capabilityId, {
        .limit = 12,
        .continuation = *token,
    });
    assert(!retargeted);
    assert(retargeted.error().code == foundation::error::ErrorCode::Conflict);

    // Another instance ID and another canonical capability are disjoint.
    provider::hypersnap::continuation::Store otherInstance{
        *backing, provider::model::ProviderId{"other-node"},
        "https://first-operator.example", "fid:42"};
    assert(!otherInstance.resolve(capabilityId, {
        .limit = 12, .continuation = *token,
    }));
    assert(!first.resolve(capability::catalog::feedUserQuotesRead(), {
        .limit = 12, .continuation = *token,
    }));

    provider::hypersnap::continuation::Store differentFid{
        *backing, id, "https://first-operator.example", "fid:43"};
    auto crossUser = differentFid.resolve(capabilityId, {
        .limit = 12, .continuation = *token,
    });
    assert(!crossUser && crossUser.error().code == foundation::error::ErrorCode::Conflict);
    assert(provider::hypersnap::continuation::selectorScope({
        {"fid", "42"}, {"limit", "12"}, {"cursor", "opaque"}}) == "fid:42");
    assert(provider::hypersnap::continuation::selectorScope({
        {"channel_ids", "science"}, {"limit", "12"}}) == "channel_ids:science");
    provider::hypersnap::continuation::Store differentChannel{
        *backing, id, "https://first-operator.example", "channel_ids:art"};
    auto channelCursor = differentChannel.bind(capability::catalog::feedChannelRead(),
        std::string{"channel-cursor"});
    assert(channelCursor && *channelCursor);
    provider::hypersnap::continuation::Store scienceChannel{
        *backing, id, "https://first-operator.example", "channel_ids:science"};
    assert(!scienceChannel.resolve(capability::catalog::feedChannelRead(), {
        .limit = 12, .continuation = *channelCursor,
    }));

    // Pre-migration cursors without the endpoint prefix are not replayed.
    auto legacy = backing->bind(capabilityId, id, "legacy-cursor");
    assert(legacy);
    assert(!first.resolve(capabilityId, {
        .limit = 12, .continuation = *legacy,
    }));

    auto empty = first.bind(capabilityId, std::nullopt);
    assert(empty && !*empty);
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
    assert(binding->providerCursor ==
           "indexed-v3:https://node.test///\n6:fid:42\nraw-feed-cursor+/=");
    assert(binding->providerId.value == "hypersnap-test");
    assert(binding->capabilityId.value == capability::catalog::feedFollowingRead().value);

    // Replaying the same token for another user on the *same* indexed node
    // must fail before transport dispatch, not leak another user's page.
    const auto requestsBeforeReplay = transport.urls().size();
    auto foreignUser = feeds.following(
        domain::identity::Fid{43},
        capability::pagination::PageRequest{
            .limit = 2, .continuation = first->next,
        });
    assert(!foreignUser && foreignUser.error().code
        == foundation::error::ErrorCode::Conflict);
    assert(transport.urls().size() == requestsBeforeReplay);

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

    // Quote and recast notifications must survive provider decoding. Use a
    // raw string for the JSON token so C++ and Clang agree on quote escaping.
    constexpr std::string_view replyType = R"json("type":"reply")json";
    std::string quoteBody = test::hypersnap::fixtures::notificationResponse();
    const auto quotePosition = quoteBody.find(replyType);
    assert(quotePosition != std::string::npos);
    quoteBody.replace(quotePosition, replyType.size(), R"json("type":"cast-quote")json");
    transport.enqueue(200, quoteBody);
    auto quotePage = notifications.notifications(
        domain::identity::Fid{42}, capability::pagination::PageRequest{.limit = 20});
    assert(quotePage && quotePage->items.size() == 1U);
    assert(quotePage->items.front().kind == capability::notification::NotificationKind::Quote);
    assert(quotePage->items.front().cast.has_value());

    std::string recastBody = test::hypersnap::fixtures::notificationResponse();
    const auto recastPosition = recastBody.find(replyType);
    assert(recastPosition != std::string::npos);
    recastBody.replace(recastPosition, replyType.size(), R"json("type":"cast-recast")json");
    transport.enqueue(200, recastBody);
    auto recastPage = notifications.notifications(
        domain::identity::Fid{42}, capability::pagination::PageRequest{.limit = 20});
    assert(recastPage && recastPage->items.size() == 1U);
    assert(recastPage->items.front().kind == capability::notification::NotificationKind::Recast);

    // The notification endpoint sometimes omits a cast's author/timestamp and
    // supplies those fields only on its envelope. A direct @mention must keep
    // its actual body rather than rendering a blank notification.
    transport.enqueue(200, R"json({"notifications":[{
      "type":"cast-mention",
      "timestamp":"2026-08-27T12:34:56.789Z",
      "user":{"fid":7,"username":"bob","display_name":"Bob"},
      "cast":{"hash":"0xabc123","text":"Hi @alice, please see this!"}
    }],"next":{"cursor":null}})json");
    auto mentionPage = notifications.notifications(
        domain::identity::Fid{42}, capability::pagination::PageRequest{.limit = 20});
    assert(mentionPage && mentionPage->items.size() == 1U);
    assert(mentionPage->items.front().kind == capability::notification::NotificationKind::Mention);
    assert(mentionPage->items.front().cast.has_value());
    assert(mentionPage->items.front().cast->text == "Hi @alice, please see this!");
    assert(mentionPage->items.front().cast->author.value == 7U);
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
        "parent_url": "https://warpcast.com/~/channel/dev",
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
    assert(result->channels.front().parentUrl
           && *result->channels.front().parentUrl == "https://warpcast.com/~/channel/dev");
    assert(result->channels.front().followerCount == 99U);
    assert(transport.urls().size() == 2U);
    assert(transport.urls()[0] == "https://node.test/v2/farcaster/user/search?q=dev&limit=4");
    assert(transport.urls()[1] == "https://node.test/v2/farcaster/channel/search?q=dev&limit=4");
}

void testReadApiKeysStayOnTheirConfiguredIndexedInstance()
{
    using namespace farcaster;
    auto firstConfig = testConfig();
    firstConfig.baseUrl = "https://indexed-alpha.test";
    firstConfig.apiKey = "alpha-secret-test";
    test::hypersnap::transport::FakeTransport alphaTransport{};
    alphaTransport.enqueue(200, test::hypersnap::fixtures::userResponse());
    provider::hypersnap::profile::ProfileSource alpha{alphaTransport, firstConfig};
    assert(alpha.get(domain::identity::Fid{42}));
    assert(!alphaTransport.requests().empty());
    const auto& alphaHeaders = alphaTransport.requests().front().headers;
    assert(std::ranges::find(alphaHeaders, std::string{"x-api-key: alpha-secret-test"})
        != alphaHeaders.end());
    assert(alphaTransport.urls().front().starts_with("https://indexed-alpha.test/"));

    auto secondConfig = testConfig();
    secondConfig.baseUrl = "https://indexed-beta.test";
    secondConfig.apiKey = "beta-secret-test";
    test::hypersnap::transport::FakeTransport betaTransport{};
    betaTransport.enqueue(200, test::hypersnap::fixtures::userResponse());
    provider::hypersnap::profile::ProfileSource beta{betaTransport, secondConfig};
    assert(beta.get(domain::identity::Fid{42}));
    const auto& betaHeaders = betaTransport.requests().front().headers;
    assert(std::ranges::find(betaHeaders, std::string{"x-api-key: beta-secret-test"})
        != betaHeaders.end());
    assert(std::ranges::find(betaHeaders, std::string{"x-api-key: alpha-secret-test"})
        == betaHeaders.end());
    assert(betaTransport.urls().front().starts_with("https://indexed-beta.test/"));

    // The synchronous indexed POST path must send the *same instance's* key.
    betaTransport.enqueue(200, "{}");
    provider::hypersnap::request::Client betaRequests{betaTransport, secondConfig};
    assert(betaRequests.postJson("/v2/farcaster/example", "{}"));
    assert(betaTransport.postRequests().size() == 1U);
    const auto& postHeaders = betaTransport.postRequests().front().headers;
    assert(std::ranges::find(postHeaders, std::string{"x-api-key: beta-secret-test"})
        != postHeaders.end());
    assert(std::ranges::find(postHeaders, std::string{"x-api-key: alpha-secret-test"})
        == postHeaders.end());

    auto anonymousConfig = testConfig();
    test::hypersnap::transport::FakeTransport anonymousTransport{};
    anonymousTransport.enqueue(200, test::hypersnap::fixtures::userResponse());
    provider::hypersnap::profile::ProfileSource anonymous{
        anonymousTransport, anonymousConfig};
    assert(anonymous.get(domain::identity::Fid{42}));
    const auto& anonymousHeaders = anonymousTransport.requests().front().headers;
    assert(anonymousHeaders.size() == 1U);
    assert(anonymousHeaders.front() == "Accept: application/json");

    // Reject header injection and plaintext credential dispatch before I/O.
    auto insecure = firstConfig;
    insecure.baseUrl = "http://indexed-alpha.test";
    provider::hypersnap::profile::ProfileSource insecureSource{alphaTransport, insecure};
    const auto requestCount = alphaTransport.requests().size();
    auto insecureResult = insecureSource.get(domain::identity::Fid{42});
    assert(!insecureResult
        && insecureResult.error().code == foundation::error::ErrorCode::InvalidArgument);
    assert(alphaTransport.requests().size() == requestCount);
    provider::hypersnap::request::Client invalidPost{alphaTransport, insecure};
    auto refusedPost = invalidPost.postJson("/v2/farcaster/example", "{}");
    assert(!refusedPost && refusedPost.error().code
        == foundation::error::ErrorCode::InvalidArgument);
    assert(alphaTransport.postRequests().empty());
    auto injection = firstConfig;
    injection.apiKey = "bad\r\nInjected: value";
    provider::hypersnap::profile::ProfileSource invalidKeySource{alphaTransport, injection};
    auto invalidResult = invalidKeySource.get(domain::identity::Fid{42});
    assert(!invalidResult
        && invalidResult.error().code == foundation::error::ErrorCode::InvalidArgument);
    assert(alphaTransport.requests().size() == requestCount);
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
    testDecodeAdmissionFailureCompletesOnce();
    testRejectedAsyncDecodeDeliversExactlyOnce();
    testIncompatibleViewerBatchFallsBackWithoutRepeatedProbe();
    testPartialReactionPagesPreserveIndependentEvidence();
    testParallelRecentShardFailureCompletesOnlyOnce();
    testRecentAsyncFeedAndCursor();
    testProfileCastAndIdentityEndpoints();
    testHttpErrorMappingAndConfigValidation();
    testIndexedEndpointBoundContinuation();
    testFeedPaginationAffinity();
    testSocialGraphSemantics();
    testSearchSemantics();
    testReadApiKeysStayOnTheirConfiguredIndexedInstance();
    testCapabilityRegistration();
    testCurlTransportConstruction();
    return 0;
}
