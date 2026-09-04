module;

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

module farcaster.provider.emoji.noto_animated_emoji;

import farcaster.application.emoji.catalog;
import farcaster.capability.animated_emoji;
import farcaster.domain.emoji;
import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::provider::emoji::noto {
namespace {

[[nodiscard]] foundation::error::Error error(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] bool isHexDigit(const char ch) noexcept
{
    const auto value = static_cast<unsigned char>(ch);
    return std::isdigit(value) != 0 || (value >= 'a' && value <= 'f');
}

} // namespace

NotoAnimatedEmojiProvider::NotoAnimatedEmojiProvider(
    application::emoji::catalog::EmojiCatalog& catalog,
    NotoEmojiConfig config)
    : m_catalog(catalog),
      m_config(std::move(config))
{
    if (m_config.providerId.empty()) {
        m_config.providerId = "noto";
    }
}

std::string_view NotoAnimatedEmojiProvider::providerId() const
{
    return m_config.providerId;
}

bool NotoAnimatedEmojiProvider::supports(const std::string_view key) const
{
    return wellFormedKey(key) && m_catalog.find(key) != nullptr;
}

bool NotoAnimatedEmojiProvider::wellFormedKey(const std::string_view key) noexcept
{
    if (key.empty() || key.size() > 96U) {
        return false;
    }
    std::size_t componentLength{};
    for (const char ch : key) {
        if (ch == '_') {
            if (componentLength == 0U) {
                return false;
            }
            componentLength = 0;
            continue;
        }
        if (!isHexDigit(ch)) {
            return false;
        }
        ++componentLength;
    }
    return componentLength > 0U;
}

foundation::result::Result<domain::emoji::AnimatedEmojiAsset>
NotoAnimatedEmojiProvider::resolveAsset(
    const capability::animated_emoji::EmojiAssetRequest& request) const
{
    if (request.sequence.empty() || !wellFormedKey(request.key)) {
        return std::unexpected(error(
            foundation::error::ErrorCode::InvalidArgument,
            "Noto emoji asset request carries an invalid normalized key."));
    }
    if (!supports(request.key)) {
        return std::unexpected(error(
            foundation::error::ErrorCode::Unsupported,
            "No Noto animated asset exists for normalized key '" + request.key + "'."));
    }

    domain::emoji::AnimatedEmojiAsset asset{};
    asset.unicodeSequence = request.sequence;
    asset.provider = m_config.providerId;
    asset.assetKey = request.key;
    asset.revision = m_config.revision;
    asset.animated = true;
    if (m_config.bundledPath) {
        if (auto path = m_config.bundledPath(request.key); path) {
            asset.localPath = std::move(*path);
            asset.availableOffline = true;
        }
    }
    return asset;
}

std::optional<domain::emoji::StaticEmojiFallback>
NotoAnimatedEmojiProvider::staticFallback(const std::string_view sequence) const
{
    const auto* entry = m_catalog.find(sequence);
    if (entry == nullptr) {
        return std::nullopt;
    }
    return domain::emoji::StaticEmojiFallback{
        .unicodeSequence = std::string{sequence},
        .provider = std::string{m_config.providerId},
        .assetKey = std::string{entry->key},
    };
}

std::string NotoAnimatedEmojiProvider::downloadUrlFor(const std::string_view key) const
{
    if (!wellFormedKey(key)) {
        return {};
    }
    std::string url = m_config.cdnUrlPattern;
    const std::string_view placeholder = "{key}";
    const auto position = url.find(placeholder);
    if (position == std::string::npos) {
        return {};
    }
    url.replace(position, placeholder.size(), key);
    return url;
}

} // namespace farcaster::provider::emoji::noto