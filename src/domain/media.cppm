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
