module;

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module farcaster.domain.cast;

import farcaster.domain.identity;
import farcaster.foundation.time;

export namespace farcaster::domain::cast {

/**
 * @brief Canonical identifier for a cast.
 */
struct CastId final {
    std::string value{};
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
    std::optional<CastId> parent{};
    /** @brief Protocol-native parent URL used by channel/root casts when no parent cast is present. */
    std::optional<std::string> parentUrl{};
    std::vector<Embed> embeds{};
    Engagement engagement{};
    foundation::time::Timestamp createdAt{};
};

}
