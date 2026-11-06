/*!
 * @file        src/domain/media.cppm
 * @brief       Canonical media asset representation.
 *
 * @details
 * Separates cast media kinds and uploaded asset identity from any storage
 * provider's internal object paths.
 *
 * @author      Kambiz Asadzadeh
 * @since       31 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstdint>
#include <string>

export module farcaster.domain.media;

export namespace farcaster::domain::media {

/** @brief Media family attached to a Farcaster cast through a public URL embed. */
enum class Kind : std::uint8_t {
    Image,
    Video,
};

/**
 * @brief Provider-neutral result of a completed media upload.
 *
 * The URL is the stable public delivery URL owned by Genycaster/Genyleap, not
 * a temporary object-storage upload URL. No storage credential is represented.
 */
struct Asset final {
    std::string url{};
    std::string contentType{};
    std::string fileName{};
    std::uint64_t sizeBytes{};
    Kind kind{Kind::Image};
};

} // namespace farcaster::domain::media
