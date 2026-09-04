module;

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.application.emoji.catalog;

namespace farcaster::application::emoji::catalog {
namespace {

[[nodiscard]] std::string lowerAscii(std::string_view input)
{
    std::string result{};
    result.reserve(input.size());
    for (const char raw : input) {
        const auto ch = static_cast<unsigned char>(raw);
        result.push_back(static_cast<char>(std::tolower(ch)));
    }
    return result;
}

[[nodiscard]] bool startsWith(std::string_view value, std::string_view prefix) noexcept
{
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

} // namespace

EmojiCatalog::EmojiCatalog()
{
    m_entries.reserve(entryCount());
    for (std::size_t index = 0; index < entryCount(); ++index) {
        const auto& raw = entryAt(index);
        EmojiCategory category = EmojiCategory::Smileys;
        if (raw.category == "People") category = EmojiCategory::People;
        else if (raw.category == "Animals") category = EmojiCategory::Animals;
        else if (raw.category == "Food") category = EmojiCategory::Food;
        else if (raw.category == "Activities") category = EmojiCategory::Activities;
        else if (raw.category == "Travel") category = EmojiCategory::Travel;
        else if (raw.category == "Objects") category = EmojiCategory::Objects;
        else if (raw.category == "Symbols") category = EmojiCategory::Symbols;
        else if (raw.category == "Flags") category = EmojiCategory::Flags;
        m_entries.push_back(EmojiEntry{
            .unicode = raw.unicode,
            .key = raw.key,
            .category = category,
            .name = raw.name,
            .tags = raw.tags,
        });
    }
}

std::span<const EmojiEntry> EmojiCatalog::entries() const noexcept
{
    return m_entries;
}

std::span<const EmojiEntry> EmojiCatalog::byCategory(const EmojiCategory category) const noexcept
{
    const auto begin = std::ranges::lower_bound(
        m_entries, category, {}, [](const EmojiEntry& entry) { return entry.category; });
    const auto end = std::ranges::upper_bound(
        m_entries, category, {}, [](const EmojiEntry& entry) { return entry.category; });
    return std::span{begin, end};
}

std::vector<std::string_view> EmojiCatalog::search(
    const std::string_view query,
    const std::size_t limit) const
{
    std::vector<std::string_view> result{};
    if (query.empty() || limit == 0U) {
        return result;
    }
    const std::string needle = lowerAscii(query);

    // Exact identity matches always rank first regardless of limit budget.
    for (const auto& entry : m_entries) {
        if (entry.unicode == query || entry.key == needle || entry.key == query) {
            result.push_back(entry.unicode);
        }
    }

    std::vector<const EmojiEntry*> prefixMatches{};
    std::vector<const EmojiEntry*> substringMatches{};
    for (const auto& entry : m_entries) {
        const std::string name = lowerAscii(entry.name);
        const std::string tags = lowerAscii(entry.tags);
        if (startsWith(name, needle) || name.find(needle) != std::string_view::npos ||
            tags.find(needle) != std::string_view::npos) {
            if (startsWith(name, needle)) {
                prefixMatches.push_back(&entry);
            } else {
                substringMatches.push_back(&entry);
            }
        }
    }

    const auto appendMatches = [&](const std::vector<const EmojiEntry*>& matches) {
        for (const EmojiEntry* entry : matches) {
            if (result.size() >= limit) {
                return;
            }
            if (std::ranges::find(result, entry->unicode) == result.end()) {
                result.push_back(entry->unicode);
            }
        }
    };
    appendMatches(prefixMatches);
    appendMatches(substringMatches);
    if (result.size() > limit) {
        result.resize(limit);
    }
    return result;
}

const EmojiEntry* EmojiCatalog::find(const std::string_view unicodeOrKey) const noexcept
{
    for (const auto& entry : m_entries) {
        if (entry.unicode == unicodeOrKey || entry.key == unicodeOrKey) {
            return &entry;
        }
    }
    return nullptr;
}

std::string_view EmojiCatalog::categoryId(const EmojiCategory category) noexcept
{
    switch (category) {
    case EmojiCategory::Smileys: return "smileys";
    case EmojiCategory::People: return "people";
    case EmojiCategory::Animals: return "animals";
    case EmojiCategory::Food: return "food";
    case EmojiCategory::Activities: return "activities";
    case EmojiCategory::Travel: return "travel";
    case EmojiCategory::Objects: return "objects";
    case EmojiCategory::Symbols: return "symbols";
    case EmojiCategory::Flags: return "flags";
    }
    return "smileys";
}

std::optional<EmojiCategory> EmojiCatalog::categoryFromId(const std::string_view id) noexcept
{
    if (id == "smileys") return EmojiCategory::Smileys;
    if (id == "people") return EmojiCategory::People;
    if (id == "animals") return EmojiCategory::Animals;
    if (id == "food") return EmojiCategory::Food;
    if (id == "activities") return EmojiCategory::Activities;
    if (id == "travel") return EmojiCategory::Travel;
    if (id == "objects") return EmojiCategory::Objects;
    if (id == "symbols") return EmojiCategory::Symbols;
    if (id == "flags") return EmojiCategory::Flags;
    return std::nullopt;
}

std::string_view EmojiCatalog::categoryLabelId(const EmojiCategory category) noexcept
{
    switch (category) {
    case EmojiCategory::Smileys: return "genycaster.emoji.category.smileys";
    case EmojiCategory::People: return "genycaster.emoji.category.people";
    case EmojiCategory::Animals: return "genycaster.emoji.category.animals";
    case EmojiCategory::Food: return "genycaster.emoji.category.food";
    case EmojiCategory::Activities: return "genycaster.emoji.category.activities";
    case EmojiCategory::Travel: return "genycaster.emoji.category.travel";
    case EmojiCategory::Objects: return "genycaster.emoji.category.objects";
    case EmojiCategory::Symbols: return "genycaster.emoji.category.symbols";
    case EmojiCategory::Flags: return "genycaster.emoji.category.flags";
    }
    return "genycaster.emoji.category.smileys";
}

void RecentEmojiStore::recordUsage(std::string unicode, const std::int64_t nowEpochMs)
{
    if (unicode.empty()) {
        return;
    }
    auto existing = std::ranges::find_if(m_records, [&](const RecentEmojiRecord& record) {
        return record.unicode == unicode;
    });
    if (existing != m_records.end()) {
        existing->useCount += 1U;
        existing->lastUsedEpochMs = nowEpochMs;
    } else {
        m_records.push_back(RecentEmojiRecord{
            .unicode = std::move(unicode),
            .useCount = 1U,
            .lastUsedEpochMs = nowEpochMs,
        });
    }
    evict();
}

std::vector<RecentEmojiRecord> RecentEmojiStore::top(const std::size_t limit) const
{
    auto ranked = m_records;
    std::ranges::sort(ranked, [](const RecentEmojiRecord& lhs, const RecentEmojiRecord& rhs) {
        if (lhs.useCount != rhs.useCount) {
            return lhs.useCount > rhs.useCount;
        }
        return lhs.lastUsedEpochMs > rhs.lastUsedEpochMs;
    });
    if (ranked.size() > limit) {
        ranked.resize(limit);
    }
    return ranked;
}

std::vector<RecentEmojiRecord> RecentEmojiStore::snapshot() const
{
    return m_records;
}

void RecentEmojiStore::restore(const std::span<const RecentEmojiRecord> records)
{
    m_records.clear();
    m_records.reserve(records.size());
    for (const auto& record : records) {
        if (!record.unicode.empty()) {
            m_records.push_back(record);
        }
    }
    evict();
}

void RecentEmojiStore::clear() noexcept
{
    m_records.clear();
}

std::size_t RecentEmojiStore::size() const noexcept
{
    return m_records.size();
}

void RecentEmojiStore::evict()
{
    if (m_records.size() <= MaximumRecords) {
        return;
    }
    auto ranked = top(MaximumRecords);
    m_records = std::move(ranked);
}

} // namespace farcaster::application::emoji::catalog