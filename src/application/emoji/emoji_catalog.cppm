module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module farcaster.application.emoji.catalog;

export namespace farcaster::application::emoji::catalog {

// Module-linkage raw catalog row used by the generated data table. Not part of
// the public API; declared here so both implementation units share one type.
struct RawEntry final {
    std::string_view unicode;
    std::string_view key;
    std::string_view category;
    std::string_view name;
    std::string_view tags;
};

/** @brief Returns the number of generated catalog rows. */
std::size_t entryCount() noexcept;

/** @brief Returns one generated catalog row by index. */
const RawEntry& entryAt(std::size_t index) noexcept;

/**
 * @brief Picker categories. Order matches the product category bar.
 */
enum class EmojiCategory : std::uint8_t {
    Smileys,
    People,
    Animals,
    Food,
    Activities,
    Travel,
    Objects,
    Symbols,
    Flags,
};

/**
 * @brief One catalog entry.
 *
 * unicode is the canonical emoji text and the only content identity. key is
 * the normalized provider asset identifier used for animation lookup.
 */
struct EmojiEntry final {
    std::string_view unicode{};
    std::string_view key{};
    EmojiCategory category{EmojiCategory::Smileys};
    std::string_view name{};
    std::string_view tags{};
};

/**
 * @brief Immutable in-memory emoji catalog derived from the Noto manifest.
 *
 * The catalog is a value type: composition code owns one instance. Search and
 * ranking logic live here, not in QML.
 */
class EmojiCatalog final {
public:
    EmojiCatalog();

    /** @brief All entries ordered by category then key. */
    [[nodiscard]] std::span<const EmojiEntry> entries() const noexcept;

    /** @brief Entries belonging to one category, ordered by key. */
    [[nodiscard]] std::span<const EmojiEntry> byCategory(EmojiCategory category) const noexcept;

    /**
     * @brief Case-insensitive search across name, tags, key and unicode text.
     *
     * Exact matches rank first, then name prefixes, then substring matches.
     */
    [[nodiscard]] std::vector<std::string_view> search(
        std::string_view query,
        std::size_t limit = 64U) const;

    /** @brief Finds one entry by canonical unicode text or normalized key. */
    [[nodiscard]] const EmojiEntry* find(std::string_view unicodeOrKey) const noexcept;

    /** @brief Stable machine identifier for a category, e.g. "smileys". */
    [[nodiscard]] static std::string_view categoryId(EmojiCategory category) noexcept;

    /** @brief Resolves a machine identifier back to a category. */
    [[nodiscard]] static std::optional<EmojiCategory> categoryFromId(std::string_view id) noexcept;

    /** @brief Localization key used by the presentation layer for a category label. */
    [[nodiscard]] static std::string_view categoryLabelId(EmojiCategory category) noexcept;

private:
    std::vector<EmojiEntry> m_entries{};
};

/**
 * @brief One persisted recent-emoji usage record.
 *
 * Identity is the Unicode sequence; animation assets are never the identity.
 */
struct RecentEmojiRecord final {
    std::string unicode{};
    std::uint64_t useCount{};
    std::int64_t lastUsedEpochMs{};
};

/**
 * @brief Bounded recent-emoji ranking with frequency and recency weighting.
 *
 * Pure logic: persistence (QSettings on desktop) lives in the Qt boundary.
 */
class RecentEmojiStore final {
public:
    static constexpr std::size_t MaximumRecords = 48U;

    /**
     * @brief Records one usage of a Unicode emoji sequence.
     *
     * Duplicate sequences merge; the bounded store keeps the most relevant
     * records (frequency, then recency).
     */
    void recordUsage(std::string unicode, std::int64_t nowEpochMs);

    /** @brief Returns the most relevant records, best first, up to limit. */
    [[nodiscard]] std::vector<RecentEmojiRecord> top(std::size_t limit) const;

    /** @brief Returns all records for durable persistence. */
    [[nodiscard]] std::vector<RecentEmojiRecord> snapshot() const;

    /** @brief Replaces all records (restore from persistence). */
    void restore(std::span<const RecentEmojiRecord> records);

    /** @brief Removes all records. */
    void clear() noexcept;

    [[nodiscard]] std::size_t size() const noexcept;

private:
    void evict();
    std::vector<RecentEmojiRecord> m_records{};
};

} // namespace farcaster::application::emoji::catalog