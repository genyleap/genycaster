module;

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module farcaster.domain.write;

import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.foundation.time;

export namespace farcaster::domain::write {

/**
 * @brief Canonical intent for a protocol-native write operation.
 *
 * Provider DTOs, SDK types, raw signatures, and provider cursors are never
 * represented here. An intent is the smallest provider-neutral description of
 * what the user asked the client to publish.
 */
enum class IntentKind : std::uint8_t {
    PublishCast,
    ReplyToCast,
    React,
    Follow,
    Unfollow,
};

/** @brief Canonical payload for casting or replying to another cast. */
struct CastWrite final {
    domain::identity::Fid author{};
    std::string text{};
    /** @brief Parent cast to reply to, when this is a reply. */
    std::optional<domain::cast::CastId> parent{};
    /** @brief Protocol-native channel/root parent URL when no parent cast is set. */
    std::optional<std::string> parentUrl{};
    std::vector<domain::cast::Embed> embeds{};
    foundation::time::Timestamp draftedAt{};
};

/** @brief Canonical payload for liking or recasting a cast. */
struct ReactionWrite final {
    domain::identity::Fid author{};
    domain::cast::CastId target{};
    /** @brief Protocol-native reaction type: "like" or "recast". */
    std::string type{};
    foundation::time::Timestamp draftedAt{};
};

/** @brief Canonical payload for following or unfollowing an FID. */
struct FollowWrite final {
    domain::identity::Fid follower{};
    domain::identity::Fid target{};
    bool unfollow{false};
    foundation::time::Timestamp draftedAt{};
};

/**
 * @brief Provider-neutral description of one write intent.
 *
 * Only the kind-relevant payload is populated. This type is owned by Domain and
 * is the only write shape Application is allowed to reason about.
 */
struct WriteIntent final {
    IntentKind kind{IntentKind::PublishCast};
    CastWrite cast{};
    ReactionWrite reaction{};
    FollowWrite follow{};
};

/**
 * @brief Canonical bytes intended to be signed as one protocol write.
 *
 * Produced by the application message builder before signing. Never contains a
 * private key; it is the signed artifact submitted to a provider/node.
 */
struct SignedBytes final {
    std::string value{};
};

/**
 * @brief Stable client-owned identity of one writer/signer.
 *
 * This is an opaque handle, never a key. Implementations may wrap a keychain
 * reference or an ephemeral in-memory slot depending on the platform.
 */
struct SignerId final {
    std::string value{};
};

/**
 * @brief Completed canonical signature over SignedBytes.
 *
 * The bytes produced by the local signer are provider-neutral and travel with
 * the canonical message. No raw key material is stored here.
 */
struct CanonicalSignature final {
    std::string value{};
};

/** @brief Stable client-owned idempotency key derived from signed message identity. */
struct IdempotencyKey final {
    std::string value{};
};

/**
 * @brief Fully signed, provider-neutral protocol write ready for the outbox.
 *
 * This is what a durable outbox record stores and what a routed submitter
 * transports. It carries canonical bytes plus the completed signature and a
 * deterministic idempotency key.
 */
struct SignedOperation final {
    WriteIntent intent{};
    SignedBytes bytes{};
    SignerId signer{};
    CanonicalSignature signature{};
    IdempotencyKey idempotencyKey{};
    foundation::time::Timestamp signedAt{};
};

}