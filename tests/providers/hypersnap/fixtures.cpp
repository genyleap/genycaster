module;

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

module farcaster.test.hypersnap.fixtures;

namespace farcaster::test::hypersnap::fixtures {
namespace {

const std::string kUserResponse = R"json({
  "user": {
    "object": "user",
    "fid": 42,
    "username": "alice",
    "display_name": "Alice",
    "custody_address": "0x1234",
    "pfp_url": "https://cdn.test/alice.png",
    "registered_at": "2026-08-27T00:00:00.000Z",
    "profile": {
      "bio": {"text": "hello from hypersnap"},
      "location": {"latitude": 37.77, "longitude": -122.42, "description": "San Francisco"},
      "banner": {"url": "https://cdn.test/alice-banner.png"}
    },
    "follower_count": 12,
    "following_count": 8,
    "score": 0.83,
    "verifications": ["0x1111111111111111111111111111111111111111"],
    "auth_addresses": [],
    "verified_addresses": {
      "eth_addresses": [
        "0x1111111111111111111111111111111111111111",
        "0x2222222222222222222222222222222222222222"
      ],
      "sol_addresses": ["So11111111111111111111111111111111111111112"],
      "primary": {}
    },
    "verified_accounts": []
  }
})json";

const std::string kUserWithoutBannerResponse = R"json({
  "user": {
    "object": "user",
    "fid": 42,
    "username": "alice",
    "display_name": "Alice",
    "custody_address": "0x1234",
    "pfp_url": "https://cdn.test/alice.png",
    "profile": {
      "bio": {"text": "hello from hypersnap"},
      "location": {"description": "San Francisco"}
    },
    "follower_count": 12,
    "following_count": 8
  }
})json";

const std::string kUserWithSyntheticUsernameResponse = R"json({
  "user": {
    "object": "user",
    "fid": 42,
    "username": "fid:42",
    "display_name": "Alice",
    "custody_address": "0x1234",
    "pfp_url": "https://cdn.test/alice.png",
    "profile": {
      "bio": {"text": "hello from hypersnap"}
    },
    "follower_count": 12,
    "following_count": 8
  }
})json";

const std::string kUserBannerResponse = R"json({
  "data": {
    "type": "MESSAGE_TYPE_USER_DATA_ADD",
    "fid": 42,
    "timestamp": 83433831,
    "network": "FARCASTER_NETWORK_MAINNET",
    "userDataBody": {
      "type": "USER_DATA_TYPE_BANNER",
      "value": "https://cdn.test/alice-banner-fallback.png"
    }
  },
  "hash": "0x1234",
  "hashScheme": "HASH_SCHEME_BLAKE3",
  "signature": "fixture",
  "signatureScheme": "SIGNATURE_SCHEME_ED25519",
  "signer": "0x5678"
})json";

const std::string kUserUrlResponse = R"json({
  "data": {
    "type": "MESSAGE_TYPE_USER_DATA_ADD",
    "fid": 42,
    "timestamp": 83433831,
    "network": "FARCASTER_NETWORK_MAINNET",
    "userDataBody": {
      "type": "USER_DATA_TYPE_URL",
      "value": "https://example.test/alice"
    }
  },
  "hash": "0x1234",
  "hashScheme": "HASH_SCHEME_BLAKE3",
  "signature": "fixture",
  "signatureScheme": "SIGNATURE_SCHEME_ED25519",
  "signer": "0x5678"
})json";


const std::string kUserDataByFidResponse = R"json({
  "messages": [
    {"data":{"userDataBody":{"type":"USER_DATA_TYPE_URL","value":"https://example.test/alice"}}},
    {"data":{"userDataBody":{"type":"USER_DATA_TYPE_USERNAME","value":"alice"}}},
    {"data":{"userDataBody":{"type":11,"value":"https://cdn.test/not-a-banner.png"}}},
    {"data":{"userDataBody":{"type":10,"value":"https://cdn.test/alice-banner-fallback.png"}}}
  ]
})json";

const std::string kCastObject = R"json({
  "object": "cast",
  "hash": "0xabc123",
  "parent_hash": "0xparent",
  "parent_author": {"fid": 7},
  "author": {
    "fid": 42,
    "username": "alice",
    "display_name": "Alice",
    "custody_address": "0x1234",
    "pfp_url": "https://cdn.test/alice.png",
    "registered_at": "2026-08-27T00:00:00.000Z",
    "profile": {
      "bio": {"text": "hello from hypersnap"},
      "location": {"latitude": 37.77, "longitude": -122.42, "description": "San Francisco"},
      "banner": {"url": "https://cdn.test/alice-banner.png"}
    },
    "follower_count": 12,
    "following_count": 8,
    "verifications": [],
    "auth_addresses": [],
    "verified_addresses": {"eth_addresses": [], "sol_addresses": [], "primary": {}},
    "verified_accounts": []
  },
  "text": "hello native",
  "timestamp": "2026-08-27T12:34:56.789Z",
  "embeds": [
    {"url": "https://example.test/post"},
    {"cast_id": {"fid": 7, "hash": "0xquoted"}}
  ],
  "type": "cast-reply",
  "reactions": {"likes_count": 2, "recasts_count": 1, "likes": [], "recasts": []},
  "replies": {"count": 3},
  "mentioned_profiles": [],
  "mentioned_profiles_ranges": [],
  "mentioned_channels": [],
  "mentioned_channels_ranges": []
})json";

}

TemporaryDatabase::TemporaryDatabase()
{
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    m_path = (std::filesystem::temp_directory_path()
        / ("farcaster-hypersnap-contract-" + std::to_string(unique) + ".sqlite3"))
                 .string();
}

TemporaryDatabase::~TemporaryDatabase()
{
    std::error_code error{};
    std::filesystem::remove(m_path, error);
    std::filesystem::remove(m_path + "-wal", error);
    std::filesystem::remove(m_path + "-shm", error);
}

const std::string& TemporaryDatabase::path() const noexcept
{
    return m_path;
}

const std::string& userResponse()
{
    return kUserResponse;
}

const std::string& userWithoutBannerResponse()
{
    return kUserWithoutBannerResponse;
}

const std::string& userWithSyntheticUsernameResponse()
{
    return kUserWithSyntheticUsernameResponse;
}

const std::string& userUrlResponse()
{
    return kUserUrlResponse;
}

const std::string& userBannerResponse()
{
    return kUserBannerResponse;
}

const std::string& userDataByFidResponse()
{
    return kUserDataByFidResponse;
}

std::string castResponse()
{
    return std::string{"{\"cast\":"} + kCastObject + "}";
}

std::string conversationResponse()
{
    const auto replaceOnce = [](std::string& value, const std::string_view from, const std::string_view to) {
        if (const auto position = value.find(from); position != std::string::npos) {
            value.replace(position, from.size(), to);
        }
    };

    std::string grandchild{kCastObject};
    replaceOnce(grandchild, R"json("hash": "0xabc123")json", R"json("hash": "0xreply-grandchild")json");
    replaceOnce(grandchild, R"json("parent_hash": "0xparent")json", R"json("parent_hash": "0xreply-one")json");
    replaceOnce(grandchild, "hello native", "nested answer");
    replaceOnce(grandchild, R"json("replies": {"count": 3})json", R"json("replies": {"count": 0})json");

    std::string firstReply{kCastObject};
    replaceOnce(firstReply, R"json("hash": "0xabc123")json", R"json("hash": "0xreply-one")json");
    replaceOnce(firstReply, R"json("parent_hash": "0xparent")json", R"json("parent_hash": "0xabc123")json");
    replaceOnce(firstReply, "hello native", "first direct reply");
    replaceOnce(firstReply, R"json("replies": {"count": 3})json", R"json("replies": {"count": 1})json");
    firstReply.insert(firstReply.size() - 1U, std::string{R"json(,"direct_replies":[)json"} + grandchild + "]");

    std::string secondReply{kCastObject};
    replaceOnce(secondReply, R"json("hash": "0xabc123")json", R"json("hash": "0xreply-two")json");
    replaceOnce(secondReply, R"json("parent_hash": "0xparent")json", R"json("parent_hash": "0xabc123")json");
    replaceOnce(secondReply, "hello native", "second direct reply");
    replaceOnce(secondReply, R"json("replies": {"count": 3})json", R"json("replies": {"count": 0})json");

    std::string castJson{kCastObject};
    replaceOnce(castJson, R"json("replies": {"count": 3})json", R"json("replies": {"count": 0})json");
    castJson.insert(
        castJson.size() - 1U,
        std::string{R"json(,"direct_replies":[)json"} + firstReply + "," + secondReply + "]");
    return std::string{"{\"conversation\":{\"cast\":"} + castJson + "}}";
}

std::string feedResponse(const std::string_view cursor)
{
    std::string result{"{\"casts\":["};
    result += kCastObject;
    result += "],\"next\":{\"cursor\":";
    if (cursor.empty()) {
        result += "null";
    } else {
        result += "\"";
        result += cursor;
        result += "\"";
    }
    result += "}}";
    return result;
}

std::string rootPostFeedResponse()
{
    std::string castJson{kCastObject};
    const std::string parent{R"json(  "parent_hash": "0xparent",
  "parent_author": {"fid": 7},
)json"};
    if (const auto position = castJson.find(parent); position != std::string::npos) {
        castJson.erase(position, parent.size());
    }
    const std::string quote{R"json(    {"cast_id": {"fid": 7, "hash": "0xquoted"}})json"};
    if (const auto position = castJson.find(quote); position != std::string::npos) {
        const auto comma = castJson.rfind(',', position);
        if (comma != std::string::npos) { castJson.erase(comma, position + quote.size() - comma); }
    }
    return "{\"casts\":[" + castJson + "],\"next\":{\"cursor\":null}}";
}

std::string quoteFeedResponse()
{
    std::string castJson{kCastObject};
    const std::string parent{R"json(  "parent_hash": "0xparent",
  "parent_author": {"fid": 7},
)json"};
    if (const auto position = castJson.find(parent); position != std::string::npos) {
        castJson.erase(position, parent.size());
    }
    return "{\"casts\":[" + castJson + "],\"next\":{\"cursor\":null}}";
}


std::string repliesAndRecastsFeedResponse()
{
    std::string recastLikeRoot{kCastObject};
    const std::string parent{R"json(  "parent_hash": "0xparent",
  "parent_author": {"fid": 7},
)json"};
    if (const auto position = recastLikeRoot.find(parent); position != std::string::npos) {
        recastLikeRoot.erase(position, parent.size());
    }
    const std::string oldHash{"\"hash\": \"0xabc123\""};
    if (const auto position = recastLikeRoot.find(oldHash); position != std::string::npos) {
        recastLikeRoot.replace(position, oldHash.size(), "\"hash\": \"0xrecast-target\"");
    }
    return "{\"casts\":[" + kCastObject + "," + recastLikeRoot + "],\"next\":{\"cursor\":null}}";
}

std::string repliesWithoutMetricsFeedResponse()
{
    std::string reply{kCastObject};
    const std::string replyCount{R"json("replies": {"count": 3})json"};
    if (const auto position = reply.find(replyCount); position != std::string::npos) {
        reply.replace(position, replyCount.size(), R"json("replies": {"count": 0})json");
    }

    std::string root{reply};
    const std::string parent{R"json(  "parent_hash": "0xparent",
  "parent_author": {"fid": 7},
)json"};
    if (const auto position = root.find(parent); position != std::string::npos) {
        root.erase(position, parent.size());
    }
    const std::string oldHash{"\"hash\": \"0xabc123\""};
    if (const auto position = root.find(oldHash); position != std::string::npos) {
        root.replace(position, oldHash.size(), "\"hash\": \"0xrecast-target\"");
    }
    return "{\"casts\":[" + reply + "," + root + "],\"next\":{\"cursor\":null}}";
}

std::string reactionListResponse(const std::string_view cursor, const std::string_view hash)
{
    std::string result = R"json({"reactions":[{"object":"reaction","reaction_type":"like","reaction_timestamp":"2026-08-27T12:34:56.789Z","user":{"fid":42,"username":"alice","profile":{"bio":{"text":""}}},"cast":{"hash":")json";
    result += hash;
    result += R"json(","fid":42}}],"next":{"cursor":)json";
    result += cursor.empty() ? "null" : "\"" + std::string{cursor} + "\"";
    result += "}}";
    return result;
}

std::string castInteractionsResponse()
{
    return R"json({"42":[{"hash":"0xabc123","liked":true,"recasted":false,"replied":false},{"hash":"0xdef456","liked":false,"recasted":true,"replied":true}]})json";
}

std::string notificationResponse(const std::string_view cursor)
{
    std::string result = R"json({"notifications":[{"type":"reply","timestamp":"2026-08-27T12:34:56.789Z","user":{"fid":7,"username":"bob","display_name":"Bob","pfp_url":"https://cdn.test/bob.png"},"cast":)json";
    result += kCastObject;
    result += R"json(}],"next":{"cursor":)json";
    result += cursor.empty() ? "null" : "\"" + std::string{cursor} + "\"";
    result += "}}";
    return result;
}

std::string channelFeedResponse()
{
    std::string castJson{kCastObject};
    const std::string parentHash{R"json("parent_hash": "0xparent",)json"};
    const std::string parentUrl{R"json("parent_url": "https://farcaster.group/dev/chat",)json"};
    const auto position = castJson.find(parentHash);
    assert(position != std::string::npos);
    castJson.replace(position, parentHash.size(), parentUrl);
    return "{\"casts\":[" + castJson + "],\"next\":{\"cursor\":null}}";
}

std::string socialResponse(const std::uint64_t fid, const std::string_view username)
{
    return "{\"users\":[{\"fid\":" + std::to_string(fid) +
        ",\"username\":\"" + std::string{username} +
        "\",\"profile\":{\"bio\":{\"text\":\"\"}}}],\"next\":{\"cursor\":null}}";
}

}
