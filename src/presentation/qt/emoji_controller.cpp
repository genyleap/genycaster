#include "emoji_controller.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

import farcaster.application.emoji.catalog;
import farcaster.application.emoji.animation_policy;

namespace {
namespace catalog = farcaster::application::emoji::catalog;
constexpr auto RecentsKey = "emoji/recentsV1";
[[nodiscard]] QString categoryIcon(const catalog::EmojiCategory category)
{
    switch (category) {
    case catalog::EmojiCategory::Smileys: return QStringLiteral("social/smile");
    case catalog::EmojiCategory::People: return QStringLiteral("social/users");
    case catalog::EmojiCategory::Animals: return QStringLiteral("social/mood-excited");
    case catalog::EmojiCategory::Food: return QStringLiteral("reactions/art");
    case catalog::EmojiCategory::Activities: return QStringLiteral("system/bolt");
    case catalog::EmojiCategory::Travel: return QStringLiteral("system/pin");
    case catalog::EmojiCategory::Objects: return QStringLiteral("system/clock");
    case catalog::EmojiCategory::Symbols: return QStringLiteral("social/sparkles");
    case catalog::EmojiCategory::Flags: return QStringLiteral("social/verified");
    }
    return QStringLiteral("social/smile");
}

} // namespace

class EmojiController::Impl final {
public:
    Impl(QSettings& settings, EmojiController& owner)
        : settings(settings), owner(owner)
    {
        restoreRecents();
    }
    QSettings& settings;
    EmojiController& owner;
    catalog::EmojiCatalog catalog{};
    catalog::RecentEmojiStore recentStore{};
    AssetBackend backend{};

    void recordUsage(const QString& sequence)
    {
        if (sequence.trimmed().isEmpty()) {
            return;
        }
        recentStore.recordUsage(
            sequence.toUtf8().toStdString(),
            QDateTime::currentMSecsSinceEpoch());
        persistRecents();
        emit owner.recentsChanged();
        emit owner.emojiGroupsChanged();
    }

    void persistRecents()
    {
        QJsonArray rows{};
        for (const auto& record : recentStore.snapshot()) {
            rows.append(QJsonObject{
                {QStringLiteral("unicode"), QString::fromUtf8(record.unicode)},
                {QStringLiteral("count"), static_cast<double>(record.useCount)},
                {QStringLiteral("lastUsedMs"), static_cast<double>(record.lastUsedEpochMs)},
            });
        }
        // QSettings::setValue updates the settings object immediately and lets
        // Qt coalesce the physical write. An explicit sync() here put disk I/O
        // directly on the emoji-selection UI path and caused a visible hitch.
        settings.setValue(QString::fromLatin1(RecentsKey), QJsonDocument{rows}.toJson(QJsonDocument::Compact));
    }

    void restoreRecents()
    {
        const auto stored = settings.value(QString::fromLatin1(RecentsKey)).toByteArray();
        if (stored.isEmpty()) {
            return;
        }
        QJsonParseError error{};
        const auto document = QJsonDocument::fromJson(stored, &error);
        if (error.error != QJsonParseError::NoError || !document.isArray()) {
            settings.remove(QString::fromLatin1(RecentsKey));
            return;
        }
        std::vector<catalog::RecentEmojiRecord> records{};
        for (const auto& value : document.array()) {
            const auto row = value.toObject();
            const QString unicode = row.value(QStringLiteral("unicode")).toString().trimmed();
            if (unicode.isEmpty()) {
                continue;
            }
            records.push_back(catalog::RecentEmojiRecord{
                .unicode = unicode.toUtf8().toStdString(),
                .useCount = static_cast<std::uint64_t>(row.value(QStringLiteral("count")).toDouble()),
                .lastUsedEpochMs = static_cast<std::int64_t>(row.value(QStringLiteral("lastUsedMs")).toDouble()),
            });
        }
        recentStore.restore(records);
    }
};

EmojiController::EmojiController(QSettings& settings, QObject* parent)
    : QObject(parent),
      m_impl(std::make_unique<Impl>(settings, *this))
{
}

EmojiController::~EmojiController() = default;

bool EmojiController::available() const noexcept
{
    return true;
}

bool EmojiController::lottieAvailable() const noexcept
{
#ifdef FARCASTER_HAS_LOTTIE
    return true;
#else
    return false;
#endif
}

QString EmojiController::styleId() const
{
    return QStringLiteral("noto-animated");
}

QVariantList EmojiController::emojiGroups() const
{
    QVariantList groups{};

    const auto recentsList = recents();
    if (!recentsList.isEmpty()) {
        groups.push_back(QVariantMap{
            {QStringLiteral("id"), QStringLiteral("recent")},
            {QStringLiteral("titleKey"), QStringLiteral("genycaster.emoji.category.recent")},
            {QStringLiteral("icon"), QStringLiteral("system/clock")},
            {QStringLiteral("values"), recentsList},
        });
    }

    for (std::size_t index = 0; index <= static_cast<std::size_t>(catalog::EmojiCategory::Flags); ++index) {
        const auto category = static_cast<catalog::EmojiCategory>(index);
        QVariantList values{};
        for (const auto& entry : m_impl->catalog.byCategory(category)) {
            values.push_back(QString::fromUtf8(entry.unicode.data(), static_cast<qsizetype>(entry.unicode.size())));
        }
        groups.push_back(QVariantMap{
            {QStringLiteral("id"), QString::fromUtf8(catalog::EmojiCatalog::categoryId(category))},
            {QStringLiteral("titleKey"),
             QString::fromUtf8(catalog::EmojiCatalog::categoryLabelId(category))},
            {QStringLiteral("icon"), categoryIcon(category)},
            {QStringLiteral("values"), values},
        });
    }
    return groups;
}

QVariantList EmojiController::recents() const
{
    QVariantList result{};
    for (const auto& record : m_impl->recentStore.top(32U)) {
        result.push_back(QString::fromUtf8(record.unicode));
    }
    return result;
}

QVariantMap EmojiController::diagnostics() const
{
    return QVariantMap{
        {QStringLiteral("pendingFetches"), pendingFetches()},
        {QStringLiteral("rendererAvailable"), lottieAvailable()},
    };
}

int EmojiController::pendingFetches() const noexcept
{
    return m_impl->backend.pending ? m_impl->backend.pending() : 0;
}

void EmojiController::setAssetBackend(AssetBackend backend)
{
    m_impl->backend = std::move(backend);
}

QString EmojiController::assetSourceFor(const QString& sequence) const
{
    return m_impl->backend.source ? m_impl->backend.source(sequence) : QString{};
}

bool EmojiController::animatedAvailableFor(const QString& sequence) const
{
    return lottieAvailable() && !assetSourceFor(sequence).isEmpty();
}

void EmojiController::requestAsset(const QString& sequence)
{
    if (lottieAvailable() && m_impl->backend.request) {
        m_impl->backend.request(sequence);
    }
}

int EmojiController::playbackLoops(const int surface, const bool reducedMotion,
                                  const bool active, const bool visible) const
{
    namespace animation = farcaster::application::emoji::animation;
    if (surface < 0 || surface > static_cast<int>(animation::Surface::PickerPreview)) {
        return 0;
    }
    const auto decision = animation::MotionPolicy::decide(
        static_cast<animation::Surface>(surface), reducedMotion, active, visible);
    switch (decision.mode) {
    case animation::PlaybackMode::Static: return 0;
    case animation::PlaybackMode::PlayOnce: return 1;
    case animation::PlaybackMode::Loop: return -1;
    }
    return 0;
}

void EmojiController::recordUsage(const QString& sequence)
{
    m_impl->recordUsage(sequence);
}

QVariantList EmojiController::search(const QString& query, const int limit) const
{
    QVariantList result{};
    const auto utf8 = query.toUtf8();
    for (const auto unicode :
         m_impl->catalog.search(std::string_view{utf8.constData(), static_cast<std::size_t>(utf8.size())},
                                static_cast<std::size_t>(std::max(0, limit)))) {
        result.push_back(QString::fromUtf8(unicode.data(), static_cast<qsizetype>(unicode.size())));
    }
    return result;
}

QVariantMap EmojiController::entryFor(const QString& sequence) const
{
    const auto utf8 = sequence.toUtf8();
    const auto* entry = m_impl->catalog.find(
        std::string_view{utf8.constData(), static_cast<std::size_t>(utf8.size())});
    if (entry == nullptr) {
        return {};
    }
    return QVariantMap{
        {QStringLiteral("unicode"), QString::fromUtf8(entry->unicode.data(), static_cast<qsizetype>(entry->unicode.size()))},
        {QStringLiteral("name"), QString::fromUtf8(entry->name.data(), static_cast<qsizetype>(entry->name.size()))},
        {QStringLiteral("category"), QString::fromUtf8(catalog::EmojiCatalog::categoryId(entry->category))},
        {QStringLiteral("key"), QString::fromUtf8(entry->key.data(), static_cast<qsizetype>(entry->key.size()))},
    };
}

QVariantMap EmojiController::sourcesForSequences(const QVariantList& sequences) const
{
    QVariantMap result{};
    for (const auto& value : sequences) {
        const QString sequence = value.toString();
        const QString source = assetSourceFor(sequence);
        if (!source.isEmpty()) {
            result.insert(sequence, source);
        }
    }
    return result;
}

void EmojiController::clearRecents()
{
    m_impl->recentStore.clear();
    m_impl->persistRecents();
    emit recentsChanged();
    emit emojiGroupsChanged();
}