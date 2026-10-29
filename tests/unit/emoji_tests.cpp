#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

import farcaster.application.emoji.animation_policy;
import farcaster.application.emoji.asset_repository;
import farcaster.application.emoji.catalog;
import farcaster.application.emoji.unicode_normalizer;
import farcaster.application.rich_format;
import farcaster.capability.animated_emoji;
import farcaster.domain.emoji;
import farcaster.domain.rich_document;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.emoji.emoji_asset_validator;
import farcaster.provider.emoji.emoji_cache;
import farcaster.provider.emoji.noto_animated_emoji;

namespace {

using namespace farcaster::application::emoji;
using namespace farcaster::domain::emoji;
using farcaster::application::emoji::unicode::normalizedKey;
using farcaster::application::emoji::unicode::scanEmojiClusters;

int failures = 0;

void expect(const bool condition, const std::string_view what)
{
    if (!condition) {
        std::cerr << "FAIL: " << what << "\n";
        ++failures;
    }
}

[[nodiscard]] std::string utf8Of(const std::vector<char32_t>& codePoints)
{
    std::string result{};
    for (const char32_t cp : codePoints) {
        if (cp <= 0x7FU) {
            result.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FFU) {
            result.push_back(static_cast<char>(0xC0U | (cp >> 6U)));
            result.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        } else if (cp <= 0xFFFFU) {
            result.push_back(static_cast<char>(0xE0U | (cp >> 12U)));
            result.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
            result.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        } else {
            result.push_back(static_cast<char>(0xF0U | (cp >> 18U)));
            result.push_back(static_cast<char>(0x80U | ((cp >> 12U) & 0x3FU)));
            result.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
            result.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        }
    }
    return result;
}

constexpr char32_t Joy = 0x1F602U;
constexpr char32_t Fire = 0x1F525U;
constexpr char32_t ThumbsUp = 0x1F44DU;
constexpr char32_t SkinMedium = 0x1F3FDU;
constexpr char32_t Heart = 0x2764U;
constexpr char32_t Vs16 = 0xFE0FU;
constexpr char32_t Sun = 0x2600U;
constexpr char32_t Zwj = 0x200DU;
constexpr char32_t Laptop = 0x1F4BBU;
constexpr char32_t Woman = 0x1F469U;
constexpr char32_t Girl = 0x1F467U;
constexpr char32_t Man = 0x1F468U;
constexpr char32_t FlagUsA = 0x1F1FAU;
constexpr char32_t FlagUsB = 0x1F1F8U;
constexpr char32_t DigitThree = 0x0033U;
constexpr char32_t Keycap = 0x20E3U;

[[nodiscard]] std::string sequence(const std::initializer_list<char32_t> codePoints)
{
    return utf8Of(std::vector<char32_t>{codePoints});
}

void testUnicodeNormalization()
{
    expect(normalizedKey(sequence({Joy})) == std::optional<std::string>{"1f602"}, "simple emoji key");
    expect(normalizedKey(sequence({Heart, Vs16})) == std::optional<std::string>{"2764_fe0f"}, "VS16 kept for text-default base");
    expect(normalizedKey(sequence({Sun, Vs16})) == std::optional<std::string>{"2600_fe0f"}, "sun with VS16");
    expect(normalizedKey(sequence({Sun})).has_value() == false, "bare text-default sun is not a cluster");
    expect(normalizedKey(sequence({ThumbsUp, SkinMedium})) == std::optional<std::string>{"1f44d_1f3fd"}, "skin tone");
    expect(normalizedKey(sequence({Man, Zwj, Laptop})) == std::optional<std::string>{"1f468_200d_1f4bb"}, "ZWJ sequence");
    expect(
        normalizedKey(sequence({Man, Zwj, Woman, Zwj, Girl})) ==
            std::optional<std::string>{"1f468_200d_1f469_200d_1f467"},
        "family ZWJ sequence");
    expect(
        normalizedKey(sequence({FlagUsA, FlagUsB})) == std::optional<std::string>{"1f1fa_1f1f8"},
        "flag sequence");
    expect(
        normalizedKey(sequence({DigitThree, Vs16, Keycap})) ==
            std::optional<std::string>{"33_fe0f_20e3"},
        "keycap sequence");
    expect(normalizedKey("plain text") == std::nullopt, "plain text is not an emoji cluster");
    expect(normalizedKey("") == std::nullopt, "empty sequence rejected");
    expect(normalizedKey("\xC0\xAF") == std::nullopt, "invalid UTF-8 rejected");

    const std::string mixed = "a " + sequence({Fire}) + " b " + sequence({Joy}) + "!";
    const auto clusters = scanEmojiClusters(mixed);
    expect(clusters.size() == 2U, "scan finds two clusters in mixed text");
    if (clusters.size() == 2U) {
        expect(clusters[0].sequence == sequence({Fire}), "first cluster sequence");
        expect(clusters[0].key == "1f525", "first cluster key");
        expect(clusters[0].byteOffset == 2U, "first cluster byte offset");
        expect(clusters[1].sequence == sequence({Joy}), "second cluster sequence");
    }

    const auto adjacent = scanEmojiClusters(sequence({Fire}) + sequence({Joy}));
    expect(adjacent.size() == 2U, "adjacent clusters are separate");

    const auto trailing = scanEmojiClusters(sequence({Fire}) + "abc");
    expect(trailing.size() == 1U && trailing[0].byteLength == 4U, "cluster then text");

    const auto skinAfterBase = scanEmojiClusters(
        sequence({ThumbsUp, SkinMedium}) + sequence({Fire}));
    expect(skinAfterBase.size() == 2U, "skin-tone cluster ends before next emoji");
}

/** @brief Minimal valid Noto-style Lottie document used by validation tests. */
[[nodiscard]] std::string validLottie()
{
    return R"({"v":"5.8.1","fr":60,"ip":0,"op":118,"w":1024,"h":1024,"nm":"test","ddd":0,)"
           R"("layers":[{"ddd":0,"ind":1,"ty":4,"nm":"shape","ks":{}}],"assets":[]})";
}

void testValidator()
{
    using namespace farcaster::provider::emoji::validator;
    const auto bytes = [](const std::string_view text) {
        return std::vector<std::byte>{
            reinterpret_cast<const std::byte*>(text.data()),
            reinterpret_cast<const std::byte*>(text.data()) + text.size(),
        };
    };

    const auto valid = validateEmojiAsset(
        "noto", "1f602", std::span<const std::byte>{bytes(validLottie())});
    expect(valid.has_value(), "valid lottie accepted");
    if (valid) {
        expect(valid->width == 1024U, "width extracted");
        expect(valid->height == 1024U, "height extracted");
        expect(valid->frameRate == 60.0, "frame rate extracted");
        expect(valid->frameCount == 118U, "frame count extracted");
        expect(valid->layerCount == 1U, "layer count extracted");
        expect(valid->durationSeconds > 1.9 && valid->durationSeconds < 2.0, "duration computed");
    }

    // Portable floating-point JSON parsing must work with the Android NDK
    // libc++, which does not implement std::from_chars(double).
    auto fractional = validLottie();
    const auto frameRatePosition = fractional.find("\"fr\":60");
    fractional.replace(frameRatePosition, 7, "\"fr\":59.5");
    const auto widthPosition = fractional.find("\"w\":1024");
    fractional.replace(widthPosition, 8, "\"w\":1.024e3");
    const auto fractionalAsset = validateEmojiAsset("noto", "x", bytes(fractional));
    expect(fractionalAsset.has_value(), "fractional/scientific JSON numbers accepted");
    if (fractionalAsset) {
        expect(fractionalAsset->frameRate == 59.5, "fractional frame rate preserved");
        expect(fractionalAsset->width == 1024U, "scientific-notation width preserved");
    }

    auto overflowed = validLottie();
    const auto overflowRatePosition = overflowed.find("\"fr\":60");
    overflowed.replace(overflowRatePosition, 7, "\"fr\":1e309");
    expect(!validateEmojiAsset("noto", "x", bytes(overflowed)),
        "non-finite or out-of-range JSON numbers rejected");

    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{bytes("")}).has_value() == false,
        "empty asset rejected");

    auto oversized = bytes(validLottie());
    ValidationLimits tinyLimits{};
    tinyLimits.maxFileBytes = 16U;
    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{oversized}, tinyLimits).has_value() == false,
        "oversized asset rejected");

    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{bytes("{not json")}).has_value() == false,
        "invalid JSON rejected");
    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{bytes("[1,2,3]")}).has_value() == false,
        "non-object root rejected");
    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{bytes("{}")}).has_value() == false,
        "empty object rejected");

    auto withUrl = validLottie();
    const auto urlPosition = withUrl.find("\"ddd\"");
    withUrl.insert(urlPosition, "\"nm2\":\"https://evil.example/x\",");
    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{bytes(withUrl)}).has_value() == false,
        "unexpected URL rejected");

    auto withImageLayer = validLottie();
    const auto imagePosition = withImageLayer.find("\"ty\":4");
    withImageLayer.replace(imagePosition, 6, "\"ty\":2");
    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{bytes(withImageLayer)}).has_value() == false,
        "image layer rejected");

    auto withTextLayer = validLottie();
    const auto textPosition = withTextLayer.find("\"ty\":4");
    withTextLayer.replace(textPosition, 6, "\"ty\":5");
    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{bytes(withTextLayer)}).has_value() == false,
        "text layer rejected");

    auto with3d = validLottie();
    const auto dddPosition = with3d.find("\"ddd\":0");
    with3d.replace(dddPosition, 7, "\"ddd\":1");
    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{bytes(with3d)}).has_value() == false,
        "3D layer rejected");

    auto withExternal = validLottie();
    const auto assetPosition = withExternal.find("\"assets\":[]");
    withExternal.replace(assetPosition, 11, R"("assets":[{"id":"a","u":"images/","p":"face.png"}])");
    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{bytes(withExternal)}).has_value() == false,
        "external resource rejected");

    auto withNul = validLottie();
    withNul.push_back('\0');
    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{bytes(withNul)}).has_value() == false,
        "embedded NUL rejected");

    auto withHugeDims = validLottie();
    const auto wPosition = withHugeDims.find("\"w\":1024");
    withHugeDims.replace(wPosition, 8, "\"w\":99999");
    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{bytes(withHugeDims)}).has_value() == false,
        "unbounded dimensions rejected");

    auto withLongDuration = validLottie();
    const auto opPosition = withLongDuration.find("\"op\":118");
    withLongDuration.replace(opPosition, 8, "\"op\":99999");
    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{bytes(withLongDuration)}).has_value() == false,
        "unbounded duration rejected");

    for (const std::string_view unsafe : {
             R"({"v":"5","fr":60,"ip":0,"op":10,"w":100,"h":100,"layers":[{"\u0074y":2}]})",
             R"({"v":"5","fr":60,"ip":0,"op":10,"w":100,"h":100,"layers":[],"assets":[{"p":"\u0068ttps://example.test/a"}]})",
             R"({"v":"5","fr":60,"ip":0,"op":10,"w":100,"h":100,"layers":[{"x":"time*2"}]})",
             R"({"v":"5","fr":60,"ip":0,"op":10,"w":100,"h":100,"layers":[],"nm":"\uZZZZ"})",
             R"({"v":"5","fr":60,"ip":0,"op":10,"w":100,"h":100,"layers":[],"nm":"\uD800"})"}) {
        expect(!validateEmojiAsset("noto", "x", bytes(unsafe)), "unsafe escaped resources/expressions rejected");
    }
    ValidationLimits layerBudget{};
    layerBudget.maxLayers = 1;
    expect(!validateEmojiAsset("noto", "x", bytes(
        R"({"v":"5","fr":60,"ip":0,"op":10,"w":100,"h":100,"layers":[{"ty":0}],"assets":[{"layers":[{"ty":4}]}]})"), layerBudget),
        "nested compositions share one layer budget");

    const auto digest = sha256(std::span<const std::byte>{bytes(validLottie())});
    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{bytes(validLottie())}, {}, digest).has_value(),
        "matching checksum accepted");
    Sha256 wrong = digest;
    wrong[0] = std::byte{static_cast<unsigned char>(
        std::to_integer<unsigned char>(wrong[0]) ^ 0x01U)};
    expect(
        validateEmojiAsset("noto", "x", std::span<const std::byte>{bytes(validLottie())}, {}, wrong).has_value() == false,
        "mismatched checksum rejected");
}

void testBundledAssets()
{
    const auto directory = std::filesystem::path{EMOJI_ASSET_DIRECTORY};
    std::size_t count{};
    for (const auto& entry : std::filesystem::directory_iterator{directory}) {
        if (entry.path().extension() != ".json") {
            continue;
        }
        std::ifstream input{entry.path(), std::ios::binary};
        const std::string body{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
        const auto result = farcaster::provider::emoji::validator::validateEmojiAsset(
            "noto", entry.path().stem().string(),
            std::as_bytes(std::span{body.data(), body.size()}));
        expect(result.has_value(), entry.path().filename().string());
        if (!result) {
            std::println("{}", result.error().message);
        }
        ++count;
    }
    expect(count == 15U, "all bundled animation assets were validated");
}

/** @brief In-memory cache implementation used by repository tests. */
class FakeCache final : public farcaster::capability::animated_emoji::IEmojiAssetCache {
public:
    struct Entry final {
        std::string provider;
        std::string key;
        std::uint32_t revision;
        std::vector<std::byte> bytes;
        std::array<std::byte, 32> sha256;
    };

    [[nodiscard]] farcaster::foundation::result::Result<
        farcaster::capability::animated_emoji::CachedEmojiAsset>
    read(const std::string_view provider, const std::string_view key, const std::uint32_t revision) override
    {
        const auto found = std::ranges::find_if(entries, [&](const Entry& entry) {
            return entry.provider == provider && entry.key == key && entry.revision == revision;
        });
        if (found == entries.end()) {
            return std::unexpected(farcaster::foundation::error::Error::make(
                farcaster::foundation::error::ErrorCode::NotFound, "missing"));
        }
        return farcaster::capability::animated_emoji::CachedEmojiAsset{
            .path = std::filesystem::path{"fake"} / found->key,
            .sizeBytes = found->bytes.size(),
            .revision = found->revision,
            .provider = found->provider,
            .assetKey = found->key,
            .sha256 = found->sha256,
        };
    }

    [[nodiscard]] farcaster::foundation::result::Status store(
        const std::string_view provider,
        const std::string_view key,
        const std::uint32_t revision,
        const std::span<const std::byte> bytes,
        const std::array<std::byte, 32> sha256) override
    {
        entries.push_back(Entry{
            .provider = std::string{provider},
            .key = std::string{key},
            .revision = revision,
            .bytes = {bytes.begin(), bytes.end()},
            .sha256 = sha256,
        });
        return {};
    }

    [[nodiscard]] bool contains(
        const std::string_view provider,
        const std::string_view key,
        const std::uint32_t revision) override
    {
        return std::ranges::any_of(entries, [&](const Entry& entry) {
            return entry.provider == provider && entry.key == key && entry.revision == revision;
        });
    }

    std::vector<Entry> entries{};
};

void testProviderAndRepository()
{
    using namespace farcaster::provider::emoji::noto;
    using namespace farcaster::application::emoji::repository;

    farcaster::application::emoji::catalog::EmojiCatalog catalog{};
    const auto bundledPath = [](const std::string_view key) -> std::optional<std::filesystem::path> {
        if (key == "1f602") {
            return std::filesystem::path{"qrc:/bundled/1f602.json"};
        }
        return std::nullopt;
    };
    NotoEmojiConfig config{};
    config.bundledPath = bundledPath;
    NotoAnimatedEmojiProvider provider{catalog, config};

    expect(provider.providerId() == "noto", "provider id");
    expect(provider.supports("1f602"), "known key supported");
    expect(provider.supports("1f44d_1f3fd"), "skin-tone key supported");
    expect(provider.supports("999999") == false, "unknown key unsupported");
    expect(provider.supports("not-a-key") == false, "malformed key unsupported");
    expect(provider.downloadUrlFor("1f602") ==
            "https://fonts.gstatic.com/s/e/notoemoji/latest/1f602/lottie.json",
        "download URL built from pattern");

    farcaster::capability::animated_emoji::EmojiAssetRequest request{
        .sequence = sequence({Joy}),
        .key = "1f602",
        .revision = 1U,
        .preferOffline = true,
    };
    const auto resolved = provider.resolveAsset(request);
    expect(resolved.has_value(), "known asset resolves");
    if (resolved) {
        expect(resolved->provider == "noto", "resolved provider");
        expect(resolved->assetKey == "1f602", "resolved key");
        expect(resolved->availableOffline, "bundled asset is offline");
        expect(resolved->localPath == std::filesystem::path{"qrc:/bundled/1f602.json"}, "bundled path");
        expect(resolved->revision == 1U, "revision carried");
    }

    farcaster::capability::animated_emoji::EmojiAssetRequest missing{
        .sequence = sequence({Fire}),
        .key = "1f9ff_z",
        .revision = 1U,
    };
    const auto missingResolved = provider.resolveAsset(missing);
    expect(missingResolved.has_value() == false, "malformed key rejected");

    const auto unknown = provider.resolveAsset(farcaster::capability::animated_emoji::EmojiAssetRequest{
        .sequence = sequence({Fire}),
        .key = "1f525",
        .revision = 1U,
    });
    expect(unknown.has_value(), "supported-but-unbundled asset resolves");

    const auto fallback = provider.staticFallback(sequence({Joy}));
    expect(fallback.has_value() && fallback->provider == "noto", "static fallback metadata");

    FakeCache cache{};
    EmojiAssetRepository repository{provider, cache, catalog};
    expect(
        repository.decide(sequence({Joy}), 1U).presentation ==
            EmojiPresentation::AnimatedAsset,
        "bundled emoji decides animated");
    expect(
        repository.decide(sequence({Fire}), 1U).presentation ==
            EmojiPresentation::SystemFallback,
        "unbundled emoji falls back to system");
    expect(
        repository.decide("plain text", 1U).presentation ==
            EmojiPresentation::SystemFallback,
        "plain text falls back");
    expect(
        repository.decide(sequence({Heart, Vs16}), 1U).presentation ==
            EmojiPresentation::SystemFallback,
        "supported-but-unbundled heart falls back to system glyph");
    expect(repository.isAvailable(sequence({Joy}), 1U), "bundled availability");

    AnimatedEmojiAsset downloaded{};
    downloaded.unicodeSequence = sequence({Fire});
    downloaded.provider = "noto";
    downloaded.assetKey = "1f525";
    downloaded.localPath = std::filesystem::path{"/tmp/1f525.json"};
    downloaded.revision = 1U;
    repository.registerValidatedAsset(downloaded);
    expect(
        repository.decide(sequence({Fire}), 1U).presentation ==
            EmojiPresentation::AnimatedAsset,
        "registered asset renders animated");
    expect(repository.registeredCount() == 1U, "registry count");

    repository.markUnavailable(sequence({Fire}));
    repository.registerValidatedAsset(downloaded);
    expect(!repository.recentlyFailed(sequence({Fire})), "validated asset clears cooldown across revision keys");
    repository.markUnavailable(sequence({Joy}));
    expect(repository.recentlyFailed(sequence({Joy})), "failure cooldown active");
    repository.clearMemoryRegistry();
    expect(repository.registeredCount() == 0U, "registry cleared");
}

void testCache()
{
    using namespace farcaster::provider::emoji::cache;

    const auto root = std::filesystem::temp_directory_path() /
        ("genycaster-emoji-cache-test-" + std::to_string(
            static_cast<unsigned long long>(
                std::chrono::system_clock::now().time_since_epoch().count())));
    std::error_code ec{};
    std::filesystem::remove_all(root, ec);

    EmojiCacheConfig config{};
    config.rootDirectory = root;
    config.maxCacheBytes = 1024U * 1024U;
    EmojiAssetCache cache{config};

    const auto bytes = std::vector<std::byte>{
        std::byte{0x7B}, std::byte{0x7D}, std::byte{0x0A},
    };
    const auto digest = farcaster::provider::emoji::validator::sha256(bytes);

    const auto pathA = cache.pathFor("noto", "1f602", 1U);
    const auto pathB = cache.pathFor("noto", "1f602", 1U);
    expect(pathA == pathB, "deterministic cache keys");
    expect(
        cache.pathFor("noto", "1f602", 1U) != cache.pathFor("noto", "1f602", 2U),
        "revision changes cache key");

    expect(
        cache.read("noto", "1f602", 1U).has_value() == false,
        "cache miss before store");
    expect(
        cache.store("noto", "1f602", 1U, std::span<const std::byte>{bytes}, digest).has_value(),
        "store succeeds");
    expect(cache.contains("noto", "1f602", 1U), "contains after store");
    const auto readBack = cache.read("noto", "1f602", 1U);
    expect(readBack.has_value(), "read after store");
    if (readBack) {
        expect(readBack->assetKey == "1f602", "read key");
        expect(readBack->sizeBytes == bytes.size(), "read size");
        expect(readBack->sha256 == digest, "read digest");
    }

    // Corruption: flip a byte in the asset file.
    {
        const auto path = cache.pathFor("noto", "1f602", 1U);
        std::ofstream stream{path, std::ios::binary | std::ios::in};
        stream.seekp(0);
        stream.put('X');
        stream.flush();
    }
    expect(
        cache.read("noto", "1f602", 1U).has_value() == false,
        "corrupted entry is rejected");
    expect(std::filesystem::exists(cache.pathFor("noto", "1f602", 1U)) == false,
        "corrupted entry evicted");

    // Re-store then tamper with metadata.
    expect(
        cache.store("noto", "1f602", 1U, std::span<const std::byte>{bytes}, digest).has_value(),
        "re-store succeeds");
    expect(cache.read("noto", "1f602", 1U).has_value(), "re-read succeeds");

    // Bounded eviction: fill beyond the entry budget with distinct keys.
    EmojiCacheConfig tight{};
    tight.rootDirectory = root / "tight";
    tight.maxCacheBytes = 256U;
    tight.maxEntries = 2U;
    EmojiAssetCache tightCache{tight};
    for (std::uint32_t index = 0; index < 6U; ++index) {
        const std::string key = "1f6" + std::to_string(index);
        const auto body = std::vector<std::byte>(64U, std::byte{static_cast<unsigned char>(index)});
        const auto bodyDigest = farcaster::provider::emoji::validator::sha256(body);
        expect(
            tightCache.store("noto", key, 1U, std::span<const std::byte>{body}, bodyDigest).has_value(),
            "bounded store");
    }
    expect(tightCache.totalBytes() <= 256U, "byte budget enforced");
    expect(
        tightCache.contains("noto", "1f60", 1U) == false,
        "oldest entries evicted");

    tightCache.clear();
    expect(tightCache.totalBytes() == 0U, "clear removes entries");

    std::filesystem::remove_all(root, ec);
}

void testPolicy()
{
    using namespace farcaster::application::emoji::animation;

    expect(
        MotionPolicy::decide(Surface::Feed, true, false, true).mode == PlaybackMode::Static,
        "reduced motion feed static");
    expect(
        MotionPolicy::decide(Surface::Composer, true, true, true).mode == PlaybackMode::Static,
        "reduced motion composer static");
    expect(
        MotionPolicy::decide(Surface::PickerPreview, true, false, true).mode == PlaybackMode::Static,
        "reduced motion picker static");

    expect(
        MotionPolicy::decide(Surface::Feed, false, false, false).mode == PlaybackMode::Static,
        "invisible feed static");
    expect(
        MotionPolicy::decide(Surface::Feed, false, false, true).mode == PlaybackMode::PlayOnce,
        "visible feed plays once");
    expect(
        MotionPolicy::decide(Surface::PickerCell, false, false, true).mode == PlaybackMode::Static,
        "inactive picker cell static");
    expect(
        MotionPolicy::decide(Surface::PickerCell, false, true, true).mode == PlaybackMode::PlayOnce,
        "active picker cell plays once");
    expect(
        MotionPolicy::decide(Surface::Composer, false, false, true).mode == PlaybackMode::Loop,
        "composer loops");
    expect(
        MotionPolicy::decide(Surface::CastDetail, false, false, true).mode == PlaybackMode::Loop,
        "detail loops");
    expect(
        MotionPolicy::decide(Surface::PickerPreview, false, false, true).mode == PlaybackMode::Loop,
        "picker preview loops");
}

void testCatalogAndRecents()
{
    using namespace farcaster::application::emoji::catalog;

    EmojiCatalog catalog{};
    expect(catalog.entries().size() == 881U, "catalog size matches manifest");

    std::size_t categoryTotal{};
    for (std::size_t index = 0; index <= static_cast<std::size_t>(EmojiCategory::Flags); ++index) {
        const auto category = static_cast<EmojiCategory>(index);
        const auto entries = catalog.byCategory(category);
        expect(!entries.empty(), "category non-empty");
        categoryTotal += entries.size();
    }
    expect(categoryTotal == catalog.entries().size(), "categories partition catalog");

    const auto joy = catalog.find("1f602");
    expect(joy != nullptr && joy->unicode == sequence({Joy}), "find by key");
    expect(
        catalog.find(sequence({Joy})) != nullptr,
        "find by unicode text");

    const auto searchJoy = catalog.search("joy");
    expect(
        std::ranges::find(searchJoy, sequence({Joy})) != searchJoy.end(),
        "search by shortcode");
    const auto searchFire = catalog.search("fire");
    expect(!searchFire.empty(), "search fire");
    const auto searchFlags = catalog.search("flag");
    expect(!searchFlags.empty(), "search flags");
    const auto emptySearch = catalog.search("");
    expect(emptySearch.empty(), "empty query returns nothing");

    expect(
        EmojiCatalog::categoryFromId("smileys") == EmojiCategory::Smileys,
        "category id resolution");
    expect(
        EmojiCatalog::categoryFromId("nope") == std::nullopt,
        "unknown category id");
    expect(
        !EmojiCatalog::categoryLabelId(EmojiCategory::Smileys).empty(),
        "category label id");

    RecentEmojiStore store{};
    store.recordUsage(sequence({Joy}), 1000);
    store.recordUsage(sequence({Fire}), 2000);
    store.recordUsage(sequence({Joy}), 3000);
    auto top = store.top(10);
    expect(top.size() == 2U, "recents dedupe");
    expect(top[0].unicode == sequence({Joy}) && top[0].useCount == 2U, "frequency ranking");

    store.restore(store.snapshot());
    expect(store.size() == 2U, "restore round trip");

    for (std::size_t index = 0; index < 64U; ++index) {
        store.recordUsage(
            sequence({static_cast<char32_t>(0x1F600U + index)}),
            std::int64_t{4000} + static_cast<std::int64_t>(index));
    }
    expect(store.size() <= RecentEmojiStore::MaximumRecords, "recents bounded");
}

void testRichFormatEmojiCompatibility()
{
    using namespace farcaster::application::rich;
    using namespace farcaster::domain::rich;

    // Standard Unicode emoji stays ordinary UTF-8 through the whole pipeline.
    const std::string draft = "GenyCaster is " + sequence({Fire}) + " and " + sequence({Joy});
    MarkdownImporter importer{};
    const auto parsed = importer.parse(draft);
    expect(parsed.has_value(), "markdown import with emoji");
    if (!parsed) {
        return;
    }
    const auto plain = inlinePlainText(
        std::get<TextBlock>(parsed->blocks[0].payload).inlines);
    expect(plain == draft, "inline plain text preserves emoji bytes");

    FarcasterSafeSerializer serializer{};
    const auto layout = serializer.serialize(*parsed);
    expect(layout.has_value(), "farcaster serialization succeeds");
    if (layout) {
        expect(layout->text == draft, "farcaster text keeps standard unicode emoji");
    }

    // An explicit Emoji inline span serializes exactly like plain text.
    RichCastDocument document{};
    document.blocks.push_back(RichBlock{
        .id = makeBlockId(1U, "p"),
        .kind = BlockKind::Paragraph,
        .payload = TextBlock{
            .inlines = {
                RichInline{.kind = InlineKind::Text, .text = "Amazing "},
                RichInline{.kind = InlineKind::Emoji, .text = sequence({Fire})},
            },
        },
    });
    const auto emojiLayout = serializer.serialize(document);
    expect(emojiLayout.has_value(), "emoji-span serialization");
    if (emojiLayout) {
        expect(
            emojiLayout->text == "Amazing " + sequence({Fire}),
            "emoji span serializes to unicode text");
    }

    MarkdownExporter exporter{};
    const auto markdown = exporter.serialize(document);
    expect(
        markdown == "Amazing " + sequence({Fire}),
        "markdown export keeps unicode emoji");
}

} // namespace

int main(int argc, char** argv)
{
    const bool only = argc > 1;
    auto run = [&](const char* name, void (*fn)()) { std::fprintf(stderr, "[test] %s\n", name); if (!only || std::string_view{name} == argv[1]) fn(); };
    run("unicode", testUnicodeNormalization);
    run("validator", testValidator);
    run("bundled", testBundledAssets);
    run("provider", testProviderAndRepository);
    run("cache", testCache);
    run("policy", testPolicy);
    run("catalog", testCatalogAndRecents);
    run("rich", testRichFormatEmojiCompatibility);

    if (failures != 0) {
        std::cerr << failures << " emoji platform test(s) failed.\n";
        return 1;
    }
    std::cout << "emoji platform tests passed.\n";
    return 0;
}
