module;

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>
#include <utility>

module farcaster.presentation.media.storage_policy;

namespace farcaster::presentation::media::storage_policy {
namespace {

constexpr auto categoryCount = static_cast<std::size_t>(MediaCategory::Count);
constexpr auto networkCount = static_cast<std::size_t>(NetworkClass::Count);

[[nodiscard]] constexpr std::size_t categoryIndex(const MediaCategory category) noexcept
{
    return static_cast<std::size_t>(category);
}

[[nodiscard]] constexpr std::size_t networkIndex(const NetworkClass networkClass) noexcept
{
    return static_cast<std::size_t>(networkClass);
}

[[nodiscard]] constexpr std::size_t usageIndex(
    const NetworkClass networkClass, const MediaCategory category) noexcept
{
    return networkIndex(networkClass) * categoryCount + categoryIndex(category);
}

[[nodiscard]] PolicySnapshot defaultPolicy()
{
    PolicySnapshot policy{};
    policy.diskCacheEnabled = true;
    policy.cacheBudgetBytes = 1024U * 1024U * 1024U;
    policy.retentionDays = {
        30, // Photos
        7,  // Video previews
        30, // NFTs
        7,  // Mini Apps
        7,  // Link previews
        90, // Avatars
        90, // Animated emoji
        30, // Other
    };

    constexpr std::uint64_t mib = 1024U * 1024U;
    const auto setRule = [&](const NetworkClass network, const MediaCategory category,
                             const bool enabled, const std::uint64_t maximumMiB) {
        policy.downloadRules[networkIndex(network)][categoryIndex(category)] = DownloadRule{
            .enabled = enabled,
            .maximumBytes = maximumMiB * mib,
        };
    };

    for (std::size_t network = 0; network < networkCount; ++network) {
        for (std::size_t category = 0; category < categoryCount; ++category) {
            policy.downloadRules[network][category] = DownloadRule{
                .enabled = true,
                .maximumBytes = 12U * mib,
            };
        }
    }

    setRule(NetworkClass::Wifi, MediaCategory::Photos, true, 24);
    setRule(NetworkClass::Wifi, MediaCategory::VideoPreviews, true, 24);
    setRule(NetworkClass::Wifi, MediaCategory::Nfts, true, 24);
    setRule(NetworkClass::Wifi, MediaCategory::MiniApps, true, 16);
    setRule(NetworkClass::Wifi, MediaCategory::LinkPreviews, true, 12);
    setRule(NetworkClass::Wifi, MediaCategory::Avatars, true, 8);
    setRule(NetworkClass::Wifi, MediaCategory::AnimatedEmoji, true, 8);
    setRule(NetworkClass::Wifi, MediaCategory::Other, true, 12);

    setRule(NetworkClass::Cellular, MediaCategory::Photos, true, 6);
    setRule(NetworkClass::Cellular, MediaCategory::VideoPreviews, false, 8);
    setRule(NetworkClass::Cellular, MediaCategory::Nfts, false, 8);
    setRule(NetworkClass::Cellular, MediaCategory::MiniApps, false, 8);
    setRule(NetworkClass::Cellular, MediaCategory::LinkPreviews, true, 4);
    setRule(NetworkClass::Cellular, MediaCategory::Avatars, true, 4);
    setRule(NetworkClass::Cellular, MediaCategory::AnimatedEmoji, true, 4);
    setRule(NetworkClass::Cellular, MediaCategory::Other, false, 4);

    // Unknown/unclassified links use a conservative but usable profile. Ethernet is mapped to
    // Wifi by the Qt adapter, so this path mainly covers backends that cannot classify transport.
    setRule(NetworkClass::Other, MediaCategory::Photos, true, 6);
    setRule(NetworkClass::Other, MediaCategory::VideoPreviews, false, 8);
    setRule(NetworkClass::Other, MediaCategory::Nfts, false, 8);
    setRule(NetworkClass::Other, MediaCategory::MiniApps, false, 8);
    setRule(NetworkClass::Other, MediaCategory::LinkPreviews, true, 4);
    setRule(NetworkClass::Other, MediaCategory::Avatars, true, 4);
    setRule(NetworkClass::Other, MediaCategory::AnimatedEmoji, true, 4);
    setRule(NetworkClass::Other, MediaCategory::Other, false, 4);

    return policy;
}

} // namespace

RuntimeState::RuntimeState()
    : m_policy(defaultPolicy())
{
    for (auto& counter : m_usage) {
        counter.store(0U, std::memory_order_relaxed);
    }
}

PolicySnapshot RuntimeState::snapshot() const
{
    const std::scoped_lock lock{m_policyMutex};
    return m_policy;
}

void RuntimeState::apply(PolicySnapshot snapshot)
{
    const std::scoped_lock lock{m_policyMutex};
    m_policy = std::move(snapshot);
}

void RuntimeState::setNetworkClass(const NetworkClass networkClass)
{
    const std::scoped_lock lock{m_policyMutex};
    m_policy.networkClass = networkClass;
}

void RuntimeState::recordDownloadedBytes(
    const MediaCategory category,
    const NetworkClass networkClass,
    const std::uint64_t bytes) noexcept
{
    if (category == MediaCategory::Count || networkClass == NetworkClass::Count || bytes == 0U) {
        return;
    }
    m_usage[usageIndex(networkClass, category)].fetch_add(bytes, std::memory_order_relaxed);
}

UsageDeltas RuntimeState::takeUsageDeltas() noexcept
{
    UsageDeltas result{};
    for (std::size_t network = 0; network < networkCount; ++network) {
        for (std::size_t category = 0; category < categoryCount; ++category) {
            result.bytes[network][category] =
                m_usage[network * categoryCount + category].exchange(0U, std::memory_order_acq_rel);
        }
    }
    return result;
}

MediaCategory mediaCategoryFromId(const std::string_view id) noexcept
{
    if (id == "photos") return MediaCategory::Photos;
    if (id == "video-previews") return MediaCategory::VideoPreviews;
    if (id == "nfts") return MediaCategory::Nfts;
    if (id == "miniapps") return MediaCategory::MiniApps;
    if (id == "link-previews") return MediaCategory::LinkPreviews;
    if (id == "avatars") return MediaCategory::Avatars;
    if (id == "animated-emoji") return MediaCategory::AnimatedEmoji;
    return MediaCategory::Other;
}

std::string_view mediaCategoryId(const MediaCategory category) noexcept
{
    switch (category) {
    case MediaCategory::Photos: return "photos";
    case MediaCategory::VideoPreviews: return "video-previews";
    case MediaCategory::Nfts: return "nfts";
    case MediaCategory::MiniApps: return "miniapps";
    case MediaCategory::LinkPreviews: return "link-previews";
    case MediaCategory::Avatars: return "avatars";
    case MediaCategory::AnimatedEmoji: return "animated-emoji";
    case MediaCategory::Other:
    case MediaCategory::Count: return "other";
    }
    return "other";
}

std::string_view networkClassId(const NetworkClass networkClass) noexcept
{
    switch (networkClass) {
    case NetworkClass::Wifi: return "wifi";
    case NetworkClass::Cellular: return "cellular";
    case NetworkClass::Other:
    case NetworkClass::Count: return "other";
    }
    return "other";
}

DownloadRule downloadRule(
    const PolicySnapshot& policy,
    const MediaCategory category,
    const NetworkClass networkClass) noexcept
{
    const auto safeCategory = category == MediaCategory::Count ? MediaCategory::Other : category;
    const auto safeNetwork = networkClass == NetworkClass::Count ? NetworkClass::Other : networkClass;
    return policy.downloadRules[networkIndex(safeNetwork)][categoryIndex(safeCategory)];
}

int retentionDays(const PolicySnapshot& policy, const MediaCategory category) noexcept
{
    const auto safeCategory = category == MediaCategory::Count ? MediaCategory::Other : category;
    return std::max(0, policy.retentionDays[categoryIndex(safeCategory)]);
}

} // namespace farcaster::presentation::media::storage_policy
