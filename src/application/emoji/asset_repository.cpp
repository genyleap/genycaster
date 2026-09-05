module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

module farcaster.application.emoji.asset_repository;

import farcaster.application.emoji.catalog;
import farcaster.application.emoji.unicode_normalizer;
import farcaster.capability.animated_emoji;
import farcaster.domain.emoji;

namespace farcaster::application::emoji::repository {
namespace {

constexpr std::size_t DefaultFailureCooldownSeconds = 30U * 60U;

} // namespace

EmojiAssetRepository::EmojiAssetRepository(
    capability::animated_emoji::IAnimatedEmojiProvider& provider,
    capability::animated_emoji::IEmojiAssetCache& cache,
    catalog::EmojiCatalog& catalog,
    const std::size_t memoryBudget)
    : m_provider(provider),
      m_cache(cache),
      m_catalog(catalog),
      m_memoryBudget(memoryBudget > 0U ? memoryBudget : 1U)
{
}

std::string EmojiAssetRepository::memoryKey(
    const std::string_view provider,
    const std::string_view key,
    const std::uint32_t revision) const
{
    std::string result{};
    result.reserve(provider.size() + key.size() + 16U);
    result.append(provider);
    result.push_back('/');
    result.append(key);
    result.push_back('/');
    result.append(std::to_string(revision));
    return result;
}

std::optional<std::string> EmojiAssetRepository::normalizedKey(
    const std::string_view sequence) const
{
    return unicode::normalizedKey(sequence);
}

bool EmojiAssetRepository::isAvailable(
    const std::string_view sequence,
    const std::uint32_t revision) const
{
    const auto key = unicode::normalizedKey(sequence);
    if (!key) {
        return false;
    }
    if (!m_provider.supports(*key)) {
        return false;
    }
    const std::string memoryEntry = memoryKey(m_provider.providerId(), *key, revision);
    if (m_registry.contains(memoryEntry)) {
        return true;
    }
    capability::animated_emoji::EmojiAssetRequest request{
        .sequence = std::string{sequence},
        .key = *key,
        .revision = revision,
        .preferOffline = true,
    };
    const auto resolved = m_provider.resolveAsset(request);
    return resolved && resolved->availableOffline;
}

AssetDecision EmojiAssetRepository::decide(
    const std::string_view sequence,
    const std::uint32_t revision) const
{
    AssetDecision decision{};
    decision.presentation = domain::emoji::EmojiPresentation::SystemFallback;
    decision.staticFallback = m_provider.staticFallback(sequence);

    const auto key = unicode::normalizedKey(sequence);
    if (!key || !m_provider.supports(*key)) {
        return decision;
    }

    domain::emoji::AnimatedEmojiAsset asset{};
    asset.unicodeSequence = std::string{sequence};
    asset.provider = std::string{m_provider.providerId()};
    asset.assetKey = *key;
    asset.revision = revision;

    // 1. Bundled assets resolve without any I/O.
    capability::animated_emoji::EmojiAssetRequest request{
        .sequence = std::string{sequence},
        .key = *key,
        .revision = revision,
        .preferOffline = true,
    };
    const auto resolved = m_provider.resolveAsset(request);
    if (resolved && resolved->availableOffline && !resolved->localPath.empty()) {
        asset = *resolved;
        decision.asset = asset;
        decision.presentation = domain::emoji::EmojiPresentation::AnimatedAsset;
        return decision;
    }

    // 2. Assets validated earlier in this session are immediately renderable.
    const std::string memoryEntry = memoryKey(m_provider.providerId(), *key, revision);
    if (const auto known = m_registry.find(memoryEntry); known != m_registry.end()) {
        decision.asset = known->second;
        decision.presentation = known->second.animated
            ? domain::emoji::EmojiPresentation::AnimatedAsset
            : domain::emoji::EmojiPresentation::StaticAsset;
        return decision;
    }

    // 3. The disk cache may already hold a validated asset; the asynchronous
    //    pipeline discovers it without blocking this call.
    decision.presentation = domain::emoji::EmojiPresentation::SystemFallback;
    return decision;
}

void EmojiAssetRepository::registerValidatedAsset(
    domain::emoji::AnimatedEmojiAsset asset) noexcept
{
    if (asset.provider.empty() || asset.assetKey.empty() || asset.localPath.empty()) {
        return;
    }
    const std::string memoryEntry = memoryKey(asset.provider, asset.assetKey, asset.revision);
    m_failedKeys.erase(memoryKey(asset.provider, asset.assetKey, 0U));
    m_registry[memoryEntry] = std::move(asset);
    pruneMemoryRegistry();
}

void EmojiAssetRepository::markUnavailable(const std::string_view sequence) noexcept
{
    const auto key = unicode::normalizedKey(sequence);
    if (!key) {
        return;
    }
    const std::string memoryEntry = memoryKey(m_provider.providerId(), *key, 0U);
    m_failedKeys[memoryEntry] = std::chrono::steady_clock::now();
}

bool EmojiAssetRepository::recentlyFailed(const std::string_view sequence) const noexcept
{
    const auto key = unicode::normalizedKey(sequence);
    if (!key) {
        return false;
    }
    const std::string memoryEntry = memoryKey(m_provider.providerId(), *key, 0U);
    const auto found = m_failedKeys.find(memoryEntry);
    if (found == m_failedKeys.end()) {
        return false;
    }
    const auto elapsed = std::chrono::steady_clock::now() - found->second;
    return elapsed < std::chrono::seconds{DefaultFailureCooldownSeconds};
}

std::size_t EmojiAssetRepository::registeredCount() const noexcept
{
    return m_registry.size();
}

void EmojiAssetRepository::clearMemoryRegistry() noexcept
{
    m_registry.clear();
}

void EmojiAssetRepository::pruneMemoryRegistry() noexcept
{
    if (m_registry.size() <= m_memoryBudget) {
        return;
    }
    // Small FIFO pruning: unordered_map iteration order is unstable, so evict
    // by removing the first N entries. The disk cache remains authoritative;
    // memory registry eviction only costs a re-read from disk.
    std::size_t excess = m_registry.size() - m_memoryBudget;
    for (auto iterator = m_registry.begin(); excess > 0U && iterator != m_registry.end();) {
        iterator = m_registry.erase(iterator);
        --excess;
    }
}

} // namespace farcaster::application::emoji::repository