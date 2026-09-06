module;

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

export module farcaster.provider.emoji.noto_animated_emoji;

import farcaster.application.emoji.catalog;
import farcaster.capability.animated_emoji;
import farcaster.domain.emoji;
import farcaster.foundation.result;

export namespace farcaster::provider::emoji::noto {

/**
 * @brief Configuration for the Noto Animated Emoji provider.
 *
 * cdnUrlPattern uses "{key}" as the placeholder for the normalized asset key.
 * bundledPath resolves bundled bootstrap assets; when it returns a path the
 * asset is available offline with no network involved.
 */
struct NotoEmojiConfig final {
    std::string providerId{"noto"};
    std::string cdnUrlPattern{
        "https://fonts.gstatic.com/s/e/notoemoji/latest/{key}/lottie.json"};
    std::uint32_t revision{1U};
    std::function<std::optional<std::filesystem::path>(std::string_view key)> bundledPath{};
    std::string rasterCdnUrlPattern{
        "https://fonts.gstatic.com/s/e/notoemoji/latest/{key}/512.gif"};
};

/**
 * @brief Google Noto Animated Emoji provider adapter.
 *
 * The provider maps normalized Unicode sequences to Noto asset identifiers,
 * reports which assets exist, resolves bundled/cached/downloadable locations
 * and exposes static fallback metadata. Network and cache implementation
 * details never appear here: the cache lives behind the same provider
 * interface and the repository coordinates it.
 */
class NotoAnimatedEmojiProvider final : public capability::animated_emoji::IAnimatedEmojiProvider {
public:
    /**
     * @brief Creates the provider over one catalog instance (must outlive it).
     */
    explicit NotoAnimatedEmojiProvider(
        application::emoji::catalog::EmojiCatalog& catalog,
        NotoEmojiConfig config = {});

    [[nodiscard]] std::string_view providerId() const override;

    /** @brief True when the Noto animated set contains the normalized key. */
    [[nodiscard]] bool supports(std::string_view key) const override;

    /**
     * @brief Resolves asset metadata for one normalized request.
     *
     * Recoverable failures: Unsupported (no animated asset exists for the
     * key), InvalidArgument (malformed key). Availability of the asset
     * (bundled vs downloadable) is reflected in the descriptor.
     */
    [[nodiscard]] foundation::result::Result<domain::emoji::AnimatedEmojiAsset>
    resolveAsset(const capability::animated_emoji::EmojiAssetRequest& request) const override;

    [[nodiscard]] std::optional<domain::emoji::StaticEmojiFallback>
    staticFallback(std::string_view sequence) const override;

    /**
     * @brief Returns the canonical Noto CDN URL for a normalized key.
     *
     * Returns empty when the key is malformed or unsupported.
     */
    [[nodiscard]] std::string downloadUrlFor(std::string_view key) const override;

    /** @brief Returns the provider's raster animation URL for worker-side frame preparation.
     * Returns empty for unsupported keys. No I/O is performed.
     */
    [[nodiscard]] std::string rasterDownloadUrlFor(std::string_view key) const;

    /** @brief Validates a normalized key shape (lowercase hex joined by _). */
    [[nodiscard]] static bool wellFormedKey(std::string_view key) noexcept;

private:
    application::emoji::catalog::EmojiCatalog& m_catalog;
    NotoEmojiConfig m_config{};
};

} // namespace farcaster::provider::emoji::noto
