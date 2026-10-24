module;

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

export module farcaster.provider.snapchain.read;

import farcaster.application.read.async_gateway;
import farcaster.application.read.model;
import farcaster.capability.feed;
import farcaster.capability.social;
import farcaster.capability.model;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.result;
import farcaster.execution.weighted_thread_pool;
import farcaster.provider.model;
import farcaster.storage.continuation;
import farcaster.transport.async_http;

export namespace farcaster::provider::snapchain::read {

/**
 * @brief Explicit raw Snapchain HTTP contract; never an indexed-v2 API alias.
 *
 * Credentials stay attached only to this endpoint. No heuristic detection of
 * node capabilities is performed. All network I/O is event-driven and all
 * JSON decoding is scheduled on the bounded CPU pool.
 */
struct Config final {
    std::string baseUrl{};
    std::string authorizationHeaderName{};
    std::string authorizationHeaderValue{};
    provider::model::ProviderId providerId{};
    std::chrono::milliseconds timeout{15'000};
    std::size_t maximumResponseBytes{4U * 1024U * 1024U};
};

/**
 * @brief Advertises only directly supported raw-node operations.
 *
 * Trending/search/notifications/cast hash lookups are not claimed.
 * The Following feed is a bounded derived aggregation: it checks the entire
 * Follow graph (up to 256 followed accounts across four graph pages)
 * and up to eight reverse-ordered 100-message windows per author.
 * When coverage cannot be established,
 * it rejects the request instead of publishing an incomplete live feed.
 */
[[nodiscard]] std::vector<capability::model::CapabilityDescriptor> capabilities();

/**
 * @brief Normalizes the /v1/userDataByFid response into a protocol-native profile.
 * @return Structured protocol error on malformed or mismatched FID/shape.
 */
[[nodiscard]] foundation::result::Result<domain::profile::Profile> decodeUserData(
    std::string_view jsonBody, domain::identity::Fid expectedFid);

/**
 * @brief Normalizes a /v1/castsByFid page with a canonical feed classifier.
 *
 * The page contains only actual root posts, replies or quoted-root casts
 * corresponding to the requested category; raw provider page cursors remain
 * in infrastructure and are not stored in canonical Casts.
 */
[[nodiscard]] foundation::result::Result<capability::feed::FeedPage> decodeUserCasts(
    std::string_view jsonBody,
    domain::identity::Fid expectedFid,
    farcaster::application::read::model::FeedQuery::Kind kind);

/** @brief Verify a raw node's unified signer record against exact author/key.
 * Missing/legacy/inactive app attribution returns no FID; corrupted or
 * mismatched records are rejected. This relies on Snapchain validating
 * SignedKeyRequest custody signatures; it is NOT a local cryptographic proof.
 */
[[nodiscard]] foundation::result::Result<std::optional<domain::identity::Fid>>
decodeSignerAppFid(std::string_view jsonBody,
    domain::identity::Fid author, std::string_view signerKeyHex,
    std::uint64_t currentFarcasterSeconds);

/** @brief Decode effective Follow links from an authenticated raw-node response. */
[[nodiscard]] foundation::result::Result<capability::social::SocialGraphPage> decodeFollowLinks(
    std::string_view jsonBody, domain::identity::Fid expectedFid,
    farcaster::application::read::model::SocialQuery::Kind kind);

/**
 * @brief Constructs an event-driven raw-node reader with provider-affine
 * opaque pagination and cancellation.
 *
 * The borrowed transport, decode executor and continuation store must outlive
 * the gateway and all in-flight requests.
 */
[[nodiscard]] std::unique_ptr<application::read::async_gateway::IAsyncReadGateway> makeGateway(
    transport::async_http::IAsyncHttpTransport& transport,
    execution::weighted_thread_pool::WeightedThreadPoolExecutor& decodeExecutor,
    storage::continuation::ContinuationStore& continuations,
    Config config);

} // namespace farcaster::provider::snapchain::read
