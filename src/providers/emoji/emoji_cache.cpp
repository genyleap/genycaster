module;

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module farcaster.provider.emoji.emoji_cache;

import farcaster.capability.animated_emoji;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.emoji.emoji_asset_validator;

namespace farcaster::provider::emoji::cache {
namespace {

namespace fs = std::filesystem;

[[nodiscard]] foundation::error::Error error(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

constexpr std::string_view MetaSeparator = "=";

[[nodiscard]] std::string hexDigest(const validator::Sha256& digest)
{
    static constexpr char Hex[] = "0123456789abcdef";
    std::string result{};
    result.reserve(64);
    for (const std::byte raw : digest) {
        const auto value = std::to_integer<unsigned char>(raw);
        result.push_back(Hex[value >> 4U]);
        result.push_back(Hex[value & 0xFU]);
    }
    return result;
}

[[nodiscard]] validator::Sha256 digestFromHex(const std::string_view hex)
{
    validator::Sha256 result{};
    if (hex.size() != 64U) {
        return result;
    }
    const auto nibble = [](const char ch) -> std::uint8_t {
        if (ch >= '0' && ch <= '9') return static_cast<std::uint8_t>(ch - '0');
        if (ch >= 'a' && ch <= 'f') return static_cast<std::uint8_t>(ch - 'a' + 10);
        if (ch >= 'A' && ch <= 'F') return static_cast<std::uint8_t>(ch - 'A' + 10);
        return 0xFFU;
    };
    for (std::size_t index = 0; index < 32U; ++index) {
        const auto hi = nibble(hex[index * 2U]);
        const auto lo = nibble(hex[index * 2U + 1U]);
        if (hi == 0xFFU || lo == 0xFFU) {
            return validator::Sha256{};
        }
        result[index] = std::byte{static_cast<unsigned char>((hi << 4U) | lo)};
    }
    return result;
}

[[nodiscard]] std::string metaFileContent(
    const std::string_view provider,
    const std::string_view key,
    const std::uint32_t revision,
    const std::size_t sizeBytes,
    const validator::Sha256& sha256)
{
    std::string content{};
    content.reserve(128 + provider.size() + key.size());
    content.append("provider").append(MetaSeparator).append(provider).push_back('\n');
    content.append("key").append(MetaSeparator).append(key).push_back('\n');
    content.append("revision").append(MetaSeparator).append(std::to_string(revision)).push_back('\n');
    content.append("size").append(MetaSeparator).append(std::to_string(sizeBytes)).push_back('\n');
    content.append("sha256").append(MetaSeparator).append(hexDigest(sha256)).push_back('\n');
    return content;
}

[[nodiscard]] std::optional<std::string> metaField(
    const std::string_view content,
    const std::string_view name)
{
    const std::string prefix = std::string{name} + std::string{MetaSeparator};
    std::size_t offset{};
    while (offset < content.size()) {
        const auto end = content.find('\n', offset);
        const auto line = content.substr(offset, end == std::string_view::npos ? end : end - offset);
        if (line.size() > prefix.size() && line.substr(0, prefix.size()) == prefix) {
            return std::string{line.substr(prefix.size())};
        }
        if (end == std::string_view::npos) {
            break;
        }
        offset = end + 1U;
    }
    return std::nullopt;
}

[[nodiscard]] std::size_t parseSize(const std::string_view value) noexcept
{
    std::size_t result{};
    for (const char ch : value) {
        if (ch < '0' || ch > '9') {
            return 0U;
        }
        if (result > (std::numeric_limits<std::size_t>::max() - 9U) / 10U) {
            return 0U;
        }
        result = result * 10U + static_cast<std::size_t>(ch - '0');
    }
    return result;
}

} // namespace

EmojiAssetCache::EmojiAssetCache(EmojiCacheConfig config)
    : m_config(std::move(config))
{
}

void EmojiAssetCache::prepareDirectories() noexcept
{
    std::error_code ec{};
    if (!m_config.rootDirectory.empty()) {
        fs::create_directories(m_config.rootDirectory, ec);
    }
}

std::filesystem::path EmojiAssetCache::metaPathFor(
    const std::string_view provider,
    const std::string_view key,
    const std::uint32_t revision) const
{
    // Combine the provider/key digests with the revision into one bounded name.
    std::string digestInput = hexDigest(validator::sha256(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(provider.data()),
        provider.size(),
    }));
    digestInput += hexDigest(validator::sha256(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(key.data()),
        key.size(),
    }));
    digestInput += std::to_string(revision);
    const auto digest = validator::sha256(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(digestInput.data()),
        digestInput.size(),
    });
    return m_config.rootDirectory /
        (hexDigest(digest) + std::string{".meta"});
}

std::filesystem::path EmojiAssetCache::pathFor(
    const std::string_view provider,
    const std::string_view key,
    const std::uint32_t revision) const
{
    std::string digestInput = hexDigest(validator::sha256(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(provider.data()),
        provider.size(),
    }));
    digestInput += hexDigest(validator::sha256(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(key.data()),
        key.size(),
    }));
    digestInput += std::to_string(revision);
    const auto digest = validator::sha256(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(digestInput.data()),
        digestInput.size(),
    });
    return m_config.rootDirectory /
        (hexDigest(digest) + (m_config.format == EmojiCacheFormat::SpritePng ? ".png" : ".lottie"));
}

std::vector<EmojiAssetCache::EntryState> EmojiAssetCache::enumerateEntries() const noexcept
{
    std::vector<EntryState> entries{};
    std::error_code ec{};
    if (!fs::exists(m_config.rootDirectory, ec)) {
        return entries;
    }
    for (const auto& item : fs::directory_iterator(m_config.rootDirectory, ec)) {
        if (!item.is_regular_file(ec)) {
            continue;
        }
        const auto path = item.path();
        if (path.extension() != (m_config.format == EmojiCacheFormat::SpritePng ? ".png" : ".lottie")) {
            continue;
        }
        std::error_code sizeEc{};
        const auto sizeBytes = fs::file_size(path, sizeEc);
        if (sizeEc) {
            continue;
        }
        entries.push_back(EntryState{
            .assetPath = path,
            .metaPath = std::filesystem::path{path}.replace_extension(".meta"),
            .sizeBytes = static_cast<std::uint64_t>(sizeBytes),
            .lastWrite = fs::last_write_time(path, ec),
        });
    }
    return entries;
}

void EmojiAssetCache::removeEntry(const EntryState& entry) const noexcept
{
    std::error_code ec{};
    fs::remove(entry.assetPath, ec);
    fs::remove(entry.metaPath, ec);
}

std::optional<CacheLookup> EmojiAssetCache::readEntry(const EntryState& entry) const noexcept
{
    if (entry.sizeBytes == 0U || entry.sizeBytes > m_config.maxEntryBytes) {
        removeEntry(entry);
        return std::nullopt;
    }
    std::error_code ec{};
    if (!fs::is_regular_file(entry.metaPath, ec)) {
        removeEntry(entry);
        return std::nullopt;
    }
    std::ifstream metaStream{entry.metaPath};
    if (!metaStream) {
        removeEntry(entry);
        return std::nullopt;
    }
    std::string meta((std::istreambuf_iterator<char>(metaStream)), std::istreambuf_iterator<char>());
    if (meta.empty() || meta.size() > 2048U) {
        removeEntry(entry);
        return std::nullopt;
    }
    const auto provider = metaField(meta, "provider");
    const auto key = metaField(meta, "key");
    const auto revisionText = metaField(meta, "revision");
    const auto sizeText = metaField(meta, "size");
    const auto shaText = metaField(meta, "sha256");
    if (!provider || !key || !revisionText || !sizeText || !shaText) {
        removeEntry(entry);
        return std::nullopt;
    }
    const std::size_t declaredSize = parseSize(*sizeText);
    if (declaredSize == 0U || declaredSize != entry.sizeBytes) {
        removeEntry(entry);
        return std::nullopt;
    }
    const auto storedDigest = digestFromHex(*shaText);
    if (storedDigest == validator::Sha256{}) {
        removeEntry(entry);
        return std::nullopt;
    }
    std::ifstream assetStream{entry.assetPath, std::ios::binary};
    if (!assetStream) {
        removeEntry(entry);
        return std::nullopt;
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(entry.sizeBytes));
    assetStream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (assetStream.gcount() != static_cast<std::streamsize>(bytes.size())) {
        removeEntry(entry);
        return std::nullopt;
    }
    if (validator::sha256(bytes) != storedDigest) {
        removeEntry(entry);
        return std::nullopt;
    }
    return CacheLookup{
        .path = entry.assetPath,
        .sizeBytes = entry.sizeBytes,
        .revision = static_cast<std::uint32_t>(std::strtoul(revisionText->c_str(), nullptr, 10)),
        .provider = *provider,
        .assetKey = *key,
        .sha256 = storedDigest,
    };
}

bool EmojiAssetCache::contains(
    const std::string_view provider,
    const std::string_view key,
    const std::uint32_t revision) noexcept
{
    return read(provider, key, revision).has_value();
}

foundation::result::Result<capability::animated_emoji::CachedEmojiAsset> EmojiAssetCache::read(
    const std::string_view provider,
    const std::string_view key,
    const std::uint32_t revision) noexcept
{
    const auto assetPath = pathFor(provider, key, revision);
    std::error_code ec{};
    if (!fs::is_regular_file(assetPath, ec)) {
        return std::unexpected(error(
            foundation::error::ErrorCode::NotFound,
            "Emoji asset is not present in the cache."));
    }
    const auto entries = enumerateEntries();
    const auto found = std::ranges::find_if(entries, [&](const EntryState& entry) {
        return entry.assetPath == assetPath;
    });
    if (found == entries.end()) {
        return std::unexpected(error(
            foundation::error::ErrorCode::NotFound,
            "Emoji asset is not present in the cache."));
    }
    if (auto lookup = readEntry(*found)) {
        return capability::animated_emoji::CachedEmojiAsset{
            .path = lookup->path,
            .sizeBytes = lookup->sizeBytes,
            .revision = lookup->revision,
            .provider = std::move(lookup->provider),
            .assetKey = std::move(lookup->assetKey),
            .sha256 = lookup->sha256,
        };
    }
    return std::unexpected(error(
        foundation::error::ErrorCode::NotFound,
        "Cached emoji asset failed validation and was evicted."));
}

foundation::result::Status EmojiAssetCache::store(
    const std::string_view provider,
    const std::string_view key,
    const std::uint32_t revision,
    const std::span<const std::byte> bytes,
    const validator::Sha256 sha256) noexcept
{
    if (provider.empty() || key.empty()) {
        return std::unexpected(error(
            foundation::error::ErrorCode::InvalidArgument,
            "Emoji cache store requires a provider and asset key."));
    }
    if (bytes.empty() || bytes.size() > m_config.maxEntryBytes) {
        return std::unexpected(error(
            foundation::error::ErrorCode::InvalidArgument,
            "Emoji cache entry exceeds the bounded entry size."));
    }
    prepareDirectories();
    pruneOrphanTemps();
    const auto assetPath = pathFor(provider, key, revision);
    const auto metaPath = metaPathFor(provider, key, revision);

    // Atomic write: temp file in the same directory, then rename over target.
    std::error_code ec{};
    const auto tempPath = assetPath;
    std::string tempName = tempPath.filename().string();
    tempName += ".tmp";
    const auto tempFile = tempPath.parent_path() / tempName;

    {
        std::ofstream stream{tempFile, std::ios::binary | std::ios::trunc};
        if (!stream) {
            return std::unexpected(error(
                foundation::error::ErrorCode::StorageFailure,
                "Emoji cache could not create its temporary write file."));
        }
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        stream.flush();
        if (!stream) {
            fs::remove(tempFile, ec);
            return std::unexpected(error(
                foundation::error::ErrorCode::StorageFailure,
                "Emoji cache write failed mid-entry."));
        }
    }
    fs::rename(tempFile, assetPath, ec);
    if (ec) {
        // Windows cannot rename over an existing file; replace explicitly.
        fs::remove(assetPath, ec);
        ec.clear();
        fs::rename(tempFile, assetPath, ec);
    }
    if (ec) {
        fs::remove(tempFile, ec);
        return std::unexpected(error(
            foundation::error::ErrorCode::StorageFailure,
            "Emoji cache could not atomically publish an entry."));
    }

    std::ofstream metaStream{metaPath, std::ios::binary | std::ios::trunc};
    if (!metaStream) {
        fs::remove(assetPath, ec);
        return std::unexpected(error(
            foundation::error::ErrorCode::StorageFailure,
            "Emoji cache could not persist entry metadata."));
    }
    metaStream << metaFileContent(provider, key, revision, bytes.size(), sha256);
    metaStream.flush();
    if (!metaStream) {
        fs::remove(assetPath, ec);
        fs::remove(metaPath, ec);
        return std::unexpected(error(
            foundation::error::ErrorCode::StorageFailure,
            "Emoji cache metadata write failed."));
    }
    evict();
    return {};
}

void EmojiAssetCache::evict() noexcept
{
    auto entries = enumerateEntries();
    std::ranges::sort(entries, [](const EntryState& lhs, const EntryState& rhs) {
        return lhs.lastWrite < rhs.lastWrite;
    });
    std::size_t total{};
    for (const auto& entry : entries) {
        total += entry.sizeBytes;
    }
    std::size_t remaining = entries.size();
    for (const auto& entry : entries) {
        const bool overBytes = total > m_config.maxCacheBytes;
        const bool overEntries = remaining > m_config.maxEntries;
        if (!overBytes && !overEntries) {
            break;
        }
        total -= entry.sizeBytes;
        --remaining;
        removeEntry(entry);
    }
}

void EmojiAssetCache::clear() noexcept
{
    const auto entries = enumerateEntries();
    for (const auto& entry : entries) {
        removeEntry(entry);
    }
}

std::size_t EmojiAssetCache::totalBytes() const noexcept
{
    std::size_t total{};
    for (const auto& entry : enumerateEntries()) {
        total += entry.sizeBytes;
    }
    return total;
}

void EmojiAssetCache::pruneOrphanTemps() noexcept
{
    std::error_code ec{};
    if (!fs::exists(m_config.rootDirectory, ec)) {
        return;
    }
    const auto cutoff = fs::file_time_type::clock::now() - m_config.orphanTempMaxAge;
    for (const auto& item : fs::directory_iterator(m_config.rootDirectory, ec)) {
        const auto path = item.path();
        if (path.extension() != ".tmp") {
            continue;
        }
        std::error_code timeEc{};
        const auto lastWrite = fs::last_write_time(path, timeEc);
        if (!timeEc && lastWrite < cutoff) {
            fs::remove(path, ec);
        }
    }
}

} // namespace farcaster::provider::emoji::cache
