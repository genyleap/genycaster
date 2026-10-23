module;

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module farcaster.domain.cast;

import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.time;

export namespace farcaster::domain::cast {

/**
 * @brief Canonical identifier for a cast.
 */
struct CastId final {
    std::string value{};
};

/**
 * @brief Protocol-native reference to an existing cast.
 *
 * Farcaster parent_cast_id requires both the parent author FID and the exact
 * 20-byte cast hash. Keeping the pair canonical prevents Presentation or
 * providers from reconstructing protocol parent identity.
 */
struct CastReference final {
    identity::Fid author{};
    CastId id{};
};

/** @brief Canonical kind of a protocol-native cast embed. */
enum class EmbedKind : std::uint8_t {
    Url,
    CastReference,
};

/**
 * @brief Provider-neutral protocol embed represented as a tagged canonical value.
 *
 * For Url, value contains the external URL and referencedAuthor is empty.
 * For CastReference, value contains the referenced canonical cast hash and
 * referencedAuthor preserves the protocol-native FID from cast_id.
 */
struct Embed final {
    EmbedKind kind{EmbedKind::Url};
    std::string value{};
    std::optional<identity::Fid> referencedAuthor{};
};

/**
 * @brief Provider-neutral canonical cast snapshot.
 *
 * Provider DTOs, ranking metadata, and provider cursors are intentionally not
 * represented here.
 */
struct Engagement final {
    std::uint64_t replies{0};
    std::uint64_t recasts{0};
    std::uint64_t likes{0};
};

struct Cast final {
    CastId id{};
    identity::Fid author{};
    std::string text{};
    /** @brief Provider-resolved mention display; raw protocol text remains immutable. */
    std::optional<std::string> displayText{};
    /** @brief Optional explicitly supplied signing-client label; never inferred from author. */
    std::optional<std::string> sourceClient{};
    std::optional<CastId> parent{};
    /** @brief Protocol-native parent URL used by channel/root casts when no parent cast is present. */
    std::optional<std::string> parentUrl{};
    /** @brief Provider-resolved channel metadata; no fixed host inference. */
    std::optional<std::string> channelId{};
    std::optional<std::string> channelName{};
    std::optional<std::string> channelImageUrl{};
    std::vector<Embed> embeds{};
    /** @brief Direct conversation replies when a thread-aware read supplied them. */
    std::vector<Cast> directReplies{};
    Engagement engagement{};
    foundation::time::Timestamp createdAt{};
    /**
     * @brief Author profile snapshot carried beside a cast when the read supplied it.
     *
     * Thread/conversation reads return the full author object for every nested
     * cast. Retaining it here lets consumers hydrate reply authors without
     * issuing a profile fetch per FID; plain feed/cast reads leave it empty.
     */
    std::optional<profile::Profile> authorProfile{};
};

}
