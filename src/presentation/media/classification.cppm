// Canonical embed-media classification for Genycaster presentation surfaces.
//
// This module is the single source of truth for deciding whether an embed URL
// or a normalized media record is a directly playable video, a direct image,
// audio, a file, or a plain web page. Every presentation surface (feed row
// building, EmbedPreview dispatch, semantic grouping, rich-composer media
// blocks) converges on these rules so one Cast cannot render as inline video
// in one surface and as a static link in another.
//
// The module is deliberately Qt-free: it operates on plain strings and is
// therefore unit-testable in provider-free/UI-free builds.
module;

#include <cstdint>
#include <string_view>

export module farcaster.presentation.media;

// Every exported namespace mirrors the dotted module identity.
export namespace farcaster::presentation::media {

/// Semantic family of a cast embed after classification.
enum class MediaKind : std::uint8_t {
    Unknown = 0, //!< Nothing usable could be inferred.
    WebPage = 1, //!< Generic hyperlink / HTML page (including video web pages).
    Image = 2,   //!< Directly decodable image media.
    Video = 3,   //!< Directly playable video stream/file.
    Audio = 4,   //!< Directly playable audio stream/file.
    File = 5,    //!< Generic downloadable file embed.
};

/// Result of one classification pass.
///
/// `kind` is the authoritative family. `visual` marks content that belongs in
/// visual feed/rows. `directPlayable` is true when the source is directly
/// decodable/playable by the first-party media stack (no page extraction).
struct Classification final {
    MediaKind kind{MediaKind::Unknown};
    bool visual{false};
    bool directPlayable{false};
};

/// Classify an embed using the canonical precedence order:
/// explicit semantic kind hint, declared mediaType, HTTP contentType,
/// metadata provider kind, then URL/file extension fallback.
///
/// Precedence is intentionally *not* driven by URL extension alone. A signed
/// CDN video URL without an extension still classifies as video once media
/// metadata (contentType/mediaType) identifies it, while a YouTube or generic
/// page URL stays a WebPage even when related to video content.
///
/// @param kindHint    Optional canonical embed kind ("image", "video", ...).
///                    Non-media hints ("url", "cast-reference", ...) are ignored.
/// @param url         Embed target URL (may be empty for non-URL embeds).
/// @param mediaType   Declared media type from rich blocks/composer.
/// @param contentType HTTP content type from trusted metadata resolution.
/// @param providerKind Trusted metadata provider kind ("video", "image",
///                    "audio"); these are only emitted for direct media bodies.
[[nodiscard]] Classification classify(
    std::string_view kindHint,
    std::string_view url,
    std::string_view mediaType,
    std::string_view contentType,
    std::string_view providerKind);

/// Classify a bare source URL with no surrounding metadata. Extension and
/// trusted media-host rules only; metadata-aware callers should prefer
/// `classify()` so signed/extensionless sources are recognized.
[[nodiscard]] Classification classifyUrl(std::string_view url);

/// True when a URL may be handed to the media stack directly. Only schemes
/// intentionally supported by Genycaster (http, https, file, qrc) qualify;
/// arbitrary custom schemes are never executed by the player.
[[nodiscard]] bool isSupportedPlaybackScheme(std::string_view url);

/// Stable label for diagnostics/UI mapping ("", "web", "image", "video",
/// "audio", "file").
[[nodiscard]] std::string_view kindLabel(MediaKind kind);

} // namespace farcaster::presentation::media
