module;

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>

export module farcaster.presentation.media.storage_policy;

export namespace farcaster::presentation::media::storage_policy {

/** @brief Media classes used by Genycaster's disk-cache and auto-download policy. */
enum class MediaCategory : std::uint8_t {
    Photos,
    VideoPreviews,
    Nfts,
    MiniApps,
    LinkPreviews,
    Avatars,
    AnimatedEmoji,
    Other,
    Count,
};

/** @brief Coarse network classes relevant to automatic media downloads. */
enum class NetworkClass : std::uint8_t {
    Wifi,
    Cellular,
    Other,
    Count,
};

/** @brief One network/category automatic-download rule. */
struct DownloadRule final {
    bool enabled{true};
    std::uint64_t maximumBytes{12U * 1024U * 1024U};
};

/** @brief Thread-safe policy snapshot consumed by the asynchronous media pipeline. */
struct PolicySnapshot final {
    bool diskCacheEnabled{true};
    std::uint64_t cacheBudgetBytes{1024U * 1024U * 1024U};
    std::array<int, static_cast<std::size_t>(MediaCategory::Count)> retentionDays{};
    std::array<std::array<DownloadRule, static_cast<std::size_t>(MediaCategory::Count)>,
               static_cast<std::size_t>(NetworkClass::Count)> downloadRules{};
    NetworkClass networkClass{NetworkClass::Other};
};

/** @brief Byte deltas accumulated by network workers since the previous drain. */
struct UsageDeltas final {
    std::array<std::array<std::uint64_t, static_cast<std::size_t>(MediaCategory::Count)>,
               static_cast<std::size_t>(NetworkClass::Count)> bytes{};
};

/**
 * @brief Shared synchronization point between QML-facing storage settings and image workers.
 *
 * The object contains no Qt state. Policy reads are snapshot-based and download accounting uses
 * atomics, so network/decode workers never touch QSettings or GUI-owned QObjects.
 */
class RuntimeState final {
public:
    RuntimeState();

    RuntimeState(const RuntimeState&) = delete;
    auto operator=(const RuntimeState&) -> RuntimeState& = delete;

    /** @brief Returns a coherent copy of the current policy. */
    [[nodiscard]] PolicySnapshot snapshot() const;

    /** @brief Replaces the complete policy atomically with respect to readers. */
    void apply(PolicySnapshot snapshot);

    /** @brief Updates only the current network classification. */
    void setNetworkClass(NetworkClass networkClass);

    /** @brief Records successfully received media payload bytes for later persistence. */
    void recordDownloadedBytes(MediaCategory category, NetworkClass networkClass,
                               std::uint64_t bytes) noexcept;

    /** @brief Atomically drains byte deltas accumulated by workers. */
    [[nodiscard]] UsageDeltas takeUsageDeltas() noexcept;

private:
    mutable std::mutex m_policyMutex{};
    PolicySnapshot m_policy{};
    std::array<std::atomic<std::uint64_t>,
               static_cast<std::size_t>(NetworkClass::Count) *
                   static_cast<std::size_t>(MediaCategory::Count)> m_usage{};
};

/** @brief Parses a stable lowercase media-category id, returning Other for unknown input. */
[[nodiscard]] MediaCategory mediaCategoryFromId(std::string_view id) noexcept;

/** @brief Returns the stable lowercase id used in cache paths and provider request descriptors. */
[[nodiscard]] std::string_view mediaCategoryId(MediaCategory category) noexcept;

/** @brief Returns the stable lowercase network id used for settings and accounting. */
[[nodiscard]] std::string_view networkClassId(NetworkClass networkClass) noexcept;

/** @brief Returns the rule applicable to one category/network pair. */
[[nodiscard]] DownloadRule downloadRule(const PolicySnapshot& policy,
                                        MediaCategory category,
                                        NetworkClass networkClass) noexcept;

/** @brief Returns the retention window in days; zero means never auto-remove. */
[[nodiscard]] int retentionDays(const PolicySnapshot& policy, MediaCategory category) noexcept;

} // namespace farcaster::presentation::media::storage_policy
