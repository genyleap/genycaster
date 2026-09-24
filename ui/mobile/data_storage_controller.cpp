#include "data_storage_controller.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkInformation>
#include <QSettings>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QtConcurrentRun>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

import farcaster.presentation.media.storage_policy;

namespace storage_policy = farcaster::presentation::media::storage_policy;

namespace {

constexpr std::uint64_t mib = 1024U * 1024U;
constexpr int minimumCacheMiB = 64;
constexpr int maximumCacheMiB = 8192;
constexpr int minimumDownloadMiB = 1;
constexpr int maximumDownloadMiB = 64;
constexpr auto categoryCount = static_cast<std::size_t>(storage_policy::MediaCategory::Count);
constexpr auto networkCount = static_cast<std::size_t>(storage_policy::NetworkClass::Count);
constexpr std::array<int, categoryCount> defaultRetentionDays{
    30, // Photos
    7,  // Video previews
    30, // NFTs
    7,  // Mini Apps
    7,  // Link previews
    90, // Avatars
    90, // Animated emoji
    30, // Other
};
constexpr std::array<int, 7> supportedRetentionDays{0, 1, 3, 7, 30, 90, 365};

struct StorageRow final {
    QString id{};
    qint64 bytes{};
    int files{};
};

struct StorageSnapshot final {
    qint64 totalBytes{};
    qint64 freeBytes{};
    std::vector<StorageRow> rows{};
};

struct CacheEntry final {
    QString path{};
    qint64 bytes{};
    QDateTime modified{};
    storage_policy::MediaCategory category{storage_policy::MediaCategory::Other};
};

[[nodiscard]] QString qtMediaCategoryId(const storage_policy::MediaCategory category)
{
    const auto id = storage_policy::mediaCategoryId(category);
    return QString::fromLatin1(id.data(), static_cast<qsizetype>(id.size()));
}

[[nodiscard]] QString qtNetworkClassId(const storage_policy::NetworkClass networkClass)
{
    const auto id = storage_policy::networkClassId(networkClass);
    return QString::fromLatin1(id.data(), static_cast<qsizetype>(id.size()));
}

[[nodiscard]] QString cacheParentDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

[[nodiscard]] QString cacheRoot()
{
    const QString parent = cacheParentDirectory();
    return parent.isEmpty() ? QString{} : QDir{parent}.filePath(QStringLiteral("remote-media"));
}

[[nodiscard]] QString legacyCacheRoot()
{
    const QString parent = cacheParentDirectory();
    return parent.isEmpty() ? QString{} : QDir{parent}.filePath(QStringLiteral("remote-images"));
}

[[nodiscard]] storage_policy::MediaCategory categoryFromQString(const QString& id)
{
    const QByteArray utf8 = id.trimmed().toLower().toUtf8();
    return storage_policy::mediaCategoryFromId(
        std::string_view{utf8.constData(), static_cast<std::size_t>(utf8.size())});
}

[[nodiscard]] storage_policy::NetworkClass networkFromQString(const QString& id)
{
    const QString normalized = id.trimmed().toLower();
    if (normalized == QStringLiteral("wifi") || normalized == QStringLiteral("ethernet")) {
        return storage_policy::NetworkClass::Wifi;
    }
    if (normalized == QStringLiteral("cellular")) {
        return storage_policy::NetworkClass::Cellular;
    }
    return storage_policy::NetworkClass::Other;
}

[[nodiscard]] constexpr std::size_t categoryIndex(storage_policy::MediaCategory category) noexcept
{
    if (category == storage_policy::MediaCategory::Count) {
        category = storage_policy::MediaCategory::Other;
    }
    return static_cast<std::size_t>(category);
}

[[nodiscard]] constexpr std::size_t networkIndex(storage_policy::NetworkClass networkClass) noexcept
{
    if (networkClass == storage_policy::NetworkClass::Count) {
        networkClass = storage_policy::NetworkClass::Other;
    }
    return static_cast<std::size_t>(networkClass);
}

[[nodiscard]] bool isSupportedRetention(const int days) noexcept
{
    return std::ranges::find(supportedRetentionDays, days) != supportedRetentionDays.end();
}

[[nodiscard]] int sanitizedRetention(const int days, const int fallback) noexcept
{
    return isSupportedRetention(days) ? days : fallback;
}

[[nodiscard]] bool entryExpired(
    const QFileInfo& info,
    const storage_policy::PolicySnapshot& policy,
    const storage_policy::MediaCategory category,
    const QDateTime& now)
{
    const int days = storage_policy::retentionDays(policy, category);
    return days > 0 && info.lastModified().toUTC().daysTo(now) >= days;
}

void appendDirectoryEntries(
    const QString& directoryPath,
    const storage_policy::MediaCategory category,
    const storage_policy::PolicySnapshot& policy,
    const QDateTime& now,
    std::vector<CacheEntry>& entries)
{
    if (directoryPath.isEmpty()) {
        return;
    }

    QDir directory{directoryPath};
    if (!directory.exists()) {
        return;
    }

    const auto files = directory.entryInfoList(
        {QStringLiteral("*.img")}, QDir::Files | QDir::Readable, QDir::Time | QDir::Reversed);
    for (const QFileInfo& info : files) {
        if (entryExpired(info, policy, category, now)) {
            QFile::remove(info.absoluteFilePath());
            continue;
        }
        entries.push_back(CacheEntry{
            .path = info.absoluteFilePath(),
            .bytes = info.size(),
            .modified = info.lastModified().toUTC(),
            .category = category,
        });
    }
}

[[nodiscard]] qint64 sumEntryBytes(const std::vector<CacheEntry>& entries) noexcept
{
    qint64 total = 0;
    for (const auto& entry : entries) {
        total += std::max<qint64>(0, entry.bytes);
    }
    return total;
}

void enforceBudget(
    std::vector<CacheEntry>& entries,
    const storage_policy::PolicySnapshot& policy)
{
    if (!policy.diskCacheEnabled || policy.cacheBudgetBytes == 0U) {
        return;
    }

    qint64 total = sumEntryBytes(entries);
    if (static_cast<std::uint64_t>(std::max<qint64>(0, total)) <= policy.cacheBudgetBytes) {
        return;
    }

    std::ranges::sort(entries, {}, &CacheEntry::modified);
    for (auto& entry : entries) {
        if (static_cast<std::uint64_t>(std::max<qint64>(0, total)) <= policy.cacheBudgetBytes) {
            break;
        }
        if (entry.bytes > 0 && QFile::remove(entry.path)) {
            total -= entry.bytes;
            entry.bytes = 0;
        }
    }
}

[[nodiscard]] StorageSnapshot scanAndMaintain(
    const storage_policy::PolicySnapshot& policy,
    const QString& clearCategoryId,
    const bool clearAll)
{
    const QString root = cacheRoot();
    const QString legacy = legacyCacheRoot();

    if (clearAll) {
        if (!root.isEmpty()) {
            QDir{root}.removeRecursively();
        }
        if (!legacy.isEmpty()) {
            QDir{legacy}.removeRecursively();
        }
    } else if (!clearCategoryId.isEmpty()) {
        if (!root.isEmpty()) {
            QDir{QDir{root}.filePath(clearCategoryId)}.removeRecursively();
        }
        // The v21.5 flat cache carried no category metadata. Surface it as
        // "Other" and remove it when that category is explicitly cleared.
        if (clearCategoryId == qtMediaCategoryId(storage_policy::MediaCategory::Other)
            && !legacy.isEmpty()) {
            QDir{legacy}.removeRecursively();
        }
    }

    std::vector<CacheEntry> entries{};
    entries.reserve(512);
    const QDateTime now = QDateTime::currentDateTimeUtc();

    if (!root.isEmpty()) {
        for (std::size_t index = 0; index < categoryCount; ++index) {
            const auto category = static_cast<storage_policy::MediaCategory>(index);
            appendDirectoryEntries(
                QDir{root}.filePath(qtMediaCategoryId(category)), category, policy, now, entries);
        }
    }

    // Count unmigrated v21.5 entries so Storage Usage reflects the real bytes
    // on disk rather than only the new category-aware directory tree.
    appendDirectoryEntries(
        legacy, storage_policy::MediaCategory::Other, policy, now, entries);

    enforceBudget(entries, policy);

    std::array<qint64, categoryCount> categoryBytes{};
    std::array<int, categoryCount> categoryFiles{};
    qint64 totalBytes = 0;
    for (const auto& entry : entries) {
        if (entry.bytes <= 0) {
            continue;
        }
        const auto index = categoryIndex(entry.category);
        categoryBytes[index] += entry.bytes;
        ++categoryFiles[index];
        totalBytes += entry.bytes;
    }

    StorageSnapshot snapshot{};
    snapshot.totalBytes = totalBytes;
    snapshot.rows.reserve(categoryCount);
    for (std::size_t index = 0; index < categoryCount; ++index) {
        const auto category = static_cast<storage_policy::MediaCategory>(index);
        snapshot.rows.push_back(StorageRow{
            .id = qtMediaCategoryId(category),
            .bytes = categoryBytes[index],
            .files = categoryFiles[index],
        });
    }

    const QString storagePath = cacheParentDirectory().isEmpty()
        ? QDir::homePath()
        : cacheParentDirectory();
    const QStorageInfo storage{storagePath};
    snapshot.freeBytes = storage.isValid() ? storage.bytesAvailable() : 0;
    return snapshot;
}

} // namespace

class DataStorageController::Impl final {
public:
    explicit Impl(DataStorageController& ownerValue, QSettings& settingsValue)
        : owner(ownerValue),
          settings(settingsValue),
          state(std::make_shared<storage_policy::RuntimeState>())
    {
        loadSettings();

        usageTimer.setInterval(1000);
        usageTimer.setTimerType(Qt::CoarseTimer);
        QObject::connect(&usageTimer, &QTimer::timeout, &owner, [this] { flushUsage(); });
        usageTimer.start();

        maintenanceTimer.setInterval(30'000);
        maintenanceTimer.setTimerType(Qt::VeryCoarseTimer);
        QObject::connect(
            &maintenanceTimer, &QTimer::timeout, &owner, [this] { startMaintenance({}, false); });
        maintenanceTimer.start();

        maintenanceDebounce.setSingleShot(true);
        maintenanceDebounce.setInterval(300);
        QObject::connect(
            &maintenanceDebounce, &QTimer::timeout, &owner, [this] { startMaintenance({}, false); });

        static_cast<void>(QNetworkInformation::loadDefaultBackend());
        networkInfo = QNetworkInformation::instance();
        if (networkInfo != nullptr) {
            QObject::connect(
                networkInfo, &QNetworkInformation::transportMediumChanged, &owner,
                [this] { updateNetwork(); });
            QObject::connect(
                networkInfo, &QNetworkInformation::isMeteredChanged, &owner,
                [this] { updateNetwork(); });
            QObject::connect(
                networkInfo, &QNetworkInformation::reachabilityChanged, &owner,
                [this] { updateNetwork(); });
        }

        updateNetwork();
        startMaintenance({}, false);
    }

    ~Impl()
    {
        lifetimeToken->store(false, std::memory_order_release);
        usageTimer.stop();
        maintenanceTimer.stop();
        maintenanceDebounce.stop();
        flushUsage();
        settings.sync();
    }

    void loadSettings()
    {
        auto policy = state->snapshot();
        policy.diskCacheEnabled = settings.value(
            QStringLiteral("dataStorage/cacheEnabled"), true).toBool();

        const int cacheMiB = std::clamp(
            settings.value(QStringLiteral("dataStorage/cacheLimitMiB"), 1024).toInt(),
            minimumCacheMiB,
            maximumCacheMiB);
        policy.cacheBudgetBytes = static_cast<std::uint64_t>(cacheMiB) * mib;

        for (std::size_t categoryIndexValue = 0; categoryIndexValue < categoryCount;
             ++categoryIndexValue) {
            const auto category = static_cast<storage_policy::MediaCategory>(categoryIndexValue);
            const QString key = QStringLiteral("dataStorage/retention/") + qtMediaCategoryId(category);
            const int persisted = settings.value(key, defaultRetentionDays[categoryIndexValue]).toInt();
            policy.retentionDays[categoryIndexValue] = sanitizedRetention(
                persisted, defaultRetentionDays[categoryIndexValue]);
        }

        for (std::size_t networkIndexValue = 0; networkIndexValue < networkCount;
             ++networkIndexValue) {
            for (std::size_t categoryIndexValue = 0; categoryIndexValue < categoryCount;
                 ++categoryIndexValue) {
                const auto networkClass = static_cast<storage_policy::NetworkClass>(networkIndexValue);
                const auto category = static_cast<storage_policy::MediaCategory>(categoryIndexValue);
                auto rule = policy.downloadRules[networkIndexValue][categoryIndexValue];
                const QString base = QStringLiteral("dataStorage/auto/")
                    + qtNetworkClassId(networkClass) + QLatin1Char('/') + qtMediaCategoryId(category);

                rule.enabled = settings.value(base + QStringLiteral("/enabled"), rule.enabled).toBool();
                const int defaultLimitMiB = static_cast<int>(std::clamp<std::uint64_t>(
                    rule.maximumBytes / mib, minimumDownloadMiB, maximumDownloadMiB));
                const int persistedLimitMiB = std::clamp(
                    settings.value(base + QStringLiteral("/maxMiB"), defaultLimitMiB).toInt(),
                    minimumDownloadMiB,
                    maximumDownloadMiB);
                rule.maximumBytes = static_cast<std::uint64_t>(persistedLimitMiB) * mib;
                policy.downloadRules[networkIndexValue][categoryIndexValue] = rule;
            }
        }
        state->apply(std::move(policy));

        usageSinceValue = settings.value(
            QStringLiteral("dataStorage/usageSince"), QDateTime::currentDateTimeUtc()).toDateTime();
        if (!usageSinceValue.isValid()) {
            usageSinceValue = QDateTime::currentDateTimeUtc();
        }

        for (std::size_t networkIndexValue = 0; networkIndexValue < networkCount;
             ++networkIndexValue) {
            for (std::size_t categoryIndexValue = 0; categoryIndexValue < categoryCount;
                 ++categoryIndexValue) {
                const auto networkClass = static_cast<storage_policy::NetworkClass>(networkIndexValue);
                const auto category = static_cast<storage_policy::MediaCategory>(categoryIndexValue);
                const QString key = QStringLiteral("dataStorage/usage/")
                    + qtNetworkClassId(networkClass) + QLatin1Char('/') + qtMediaCategoryId(category);
                usage[networkIndexValue][categoryIndexValue] = settings.value(key, 0).toULongLong();
            }
        }
    }

    void persistPolicy()
    {
        const auto policy = state->snapshot();
        settings.setValue(QStringLiteral("dataStorage/cacheEnabled"), policy.diskCacheEnabled);
        settings.setValue(
            QStringLiteral("dataStorage/cacheLimitMiB"),
            static_cast<qulonglong>(policy.cacheBudgetBytes / mib));

        for (std::size_t categoryIndexValue = 0; categoryIndexValue < categoryCount;
             ++categoryIndexValue) {
            const auto category = static_cast<storage_policy::MediaCategory>(categoryIndexValue);
            settings.setValue(
                QStringLiteral("dataStorage/retention/") + qtMediaCategoryId(category),
                policy.retentionDays[categoryIndexValue]);
        }

        for (std::size_t networkIndexValue = 0; networkIndexValue < networkCount;
             ++networkIndexValue) {
            for (std::size_t categoryIndexValue = 0; categoryIndexValue < categoryCount;
                 ++categoryIndexValue) {
                const auto networkClass = static_cast<storage_policy::NetworkClass>(networkIndexValue);
                const auto category = static_cast<storage_policy::MediaCategory>(categoryIndexValue);
                const auto& rule = policy.downloadRules[networkIndexValue][categoryIndexValue];
                const QString base = QStringLiteral("dataStorage/auto/")
                    + qtNetworkClassId(networkClass) + QLatin1Char('/') + qtMediaCategoryId(category);
                settings.setValue(base + QStringLiteral("/enabled"), rule.enabled);
                settings.setValue(
                    base + QStringLiteral("/maxMiB"),
                    static_cast<qulonglong>(rule.maximumBytes / mib));
            }
        }
        // QSettings owns its normal asynchronous persistence lifecycle. Avoid
        // synchronous sync() calls while sliders or toggles are being edited.
    }

    void flushUsage()
    {
        const auto deltas = state->takeUsageDeltas();
        bool changed = false;
        for (std::size_t networkIndexValue = 0; networkIndexValue < networkCount;
             ++networkIndexValue) {
            for (std::size_t categoryIndexValue = 0; categoryIndexValue < categoryCount;
                 ++categoryIndexValue) {
                const auto delta = deltas.bytes[networkIndexValue][categoryIndexValue];
                if (delta == 0U) {
                    continue;
                }

                changed = true;
                usage[networkIndexValue][categoryIndexValue] += delta;
                const auto networkClass = static_cast<storage_policy::NetworkClass>(networkIndexValue);
                const auto category = static_cast<storage_policy::MediaCategory>(categoryIndexValue);
                settings.setValue(
                    QStringLiteral("dataStorage/usage/") + qtNetworkClassId(networkClass)
                        + QLatin1Char('/') + qtMediaCategoryId(category),
                    static_cast<qulonglong>(usage[networkIndexValue][categoryIndexValue]));
            }
        }

        if (changed) {
            settings.setValue(QStringLiteral("dataStorage/usageSince"), usageSinceValue);
            emit owner.usageChanged();
        }
    }

    void updateNetwork()
    {
        QString nextType = QStringLiteral("unknown");
        bool nextMetered = false;
        auto nextClass = storage_policy::NetworkClass::Other;

        if (networkInfo != nullptr
            && networkInfo->reachability() != QNetworkInformation::Reachability::Disconnected) {
            nextMetered = networkInfo->isMetered();
            switch (networkInfo->transportMedium()) {
            case QNetworkInformation::TransportMedium::WiFi:
                nextType = QStringLiteral("wifi");
                nextClass = storage_policy::NetworkClass::Wifi;
                break;
            case QNetworkInformation::TransportMedium::Ethernet:
                nextType = QStringLiteral("ethernet");
                nextClass = storage_policy::NetworkClass::Wifi;
                break;
            case QNetworkInformation::TransportMedium::Cellular:
                nextType = QStringLiteral("cellular");
                nextClass = storage_policy::NetworkClass::Cellular;
                break;
            case QNetworkInformation::TransportMedium::Bluetooth:
            case QNetworkInformation::TransportMedium::Unknown:
                break;
            }
        }

        state->setNetworkClass(nextClass);
        if (nextType == networkTypeValue && nextMetered == networkMeteredValue) {
            return;
        }

        networkTypeValue = std::move(nextType);
        networkMeteredValue = nextMetered;
        emit owner.networkChanged();
        emit owner.policyChanged();
    }

    void scheduleMaintenance()
    {
        maintenanceDebounce.start();
    }

    void startMaintenance(QString clearCategoryId, const bool clearAll)
    {
        if (busy) {
            pendingRefresh = true;
            if (clearAll) {
                pendingClearAll = true;
                pendingClears.clear();
            } else if (!clearCategoryId.isEmpty() && !pendingClearAll
                       && !pendingClears.contains(clearCategoryId)) {
                pendingClears.push_back(std::move(clearCategoryId));
            }
            return;
        }

        busy = true;
        emit owner.storageChanged();

        const auto policySnapshot = state->snapshot();
        const auto token = lifetimeToken;
        auto future = QtConcurrent::run(
            [policySnapshot, clearCategoryId = std::move(clearCategoryId), clearAll] {
                return scanAndMaintain(policySnapshot, clearCategoryId, clearAll);
            });
        future.then(&owner, [this, token](StorageSnapshot snapshot) {
            if (!token->load(std::memory_order_acquire)) {
                return;
            }

            cacheBytes = snapshot.totalBytes;
            freeBytes = snapshot.freeBytes;
            breakdown.clear();
            breakdown.reserve(static_cast<qsizetype>(snapshot.rows.size()));
            for (const auto& rowSnapshot : snapshot.rows) {
                QVariantMap row;
                row.insert(QStringLiteral("id"), rowSnapshot.id);
                row.insert(QStringLiteral("bytes"), rowSnapshot.bytes);
                row.insert(QStringLiteral("files"), rowSnapshot.files);
                breakdown.push_back(std::move(row));
            }

            busy = false;
            emit owner.storageChanged();

            if (pendingClearAll) {
                pendingClearAll = false;
                pendingRefresh = false;
                startMaintenance({}, true);
                return;
            }
            if (!pendingClears.isEmpty()) {
                const QString categoryId = pendingClears.takeFirst();
                pendingRefresh = false;
                startMaintenance(categoryId, false);
                return;
            }
            if (pendingRefresh) {
                pendingRefresh = false;
                startMaintenance({}, false);
            }
        });
    }

    DataStorageController& owner;
    QSettings& settings;
    std::shared_ptr<storage_policy::RuntimeState> state;
    std::shared_ptr<std::atomic_bool> lifetimeToken{std::make_shared<std::atomic_bool>(true)};
    QTimer usageTimer{};
    QTimer maintenanceTimer{};
    QTimer maintenanceDebounce{};
    QNetworkInformation* networkInfo{};
    QString networkTypeValue{QStringLiteral("unknown")};
    bool networkMeteredValue{};
    qint64 cacheBytes{};
    qint64 freeBytes{};
    QVariantList breakdown{};
    bool busy{};
    bool pendingRefresh{};
    bool pendingClearAll{};
    QStringList pendingClears{};
    QDateTime usageSinceValue{};
    std::array<std::array<std::uint64_t, categoryCount>, networkCount> usage{};
};

DataStorageController::DataStorageController(QSettings& settings, QObject* parent)
    : QObject(parent),
      m_impl(std::make_unique<Impl>(*this, settings))
{
}

DataStorageController::~DataStorageController() = default;

std::shared_ptr<void> DataStorageController::runtimeStateHandle() const
{
    return m_impl->state;
}

bool DataStorageController::diskCacheEnabled() const noexcept
{
    return m_impl->state->snapshot().diskCacheEnabled;
}

int DataStorageController::cacheLimitMiB() const noexcept
{
    return static_cast<int>(m_impl->state->snapshot().cacheBudgetBytes / mib);
}

qint64 DataStorageController::cacheUsageBytes() const noexcept
{
    return m_impl->cacheBytes;
}

qint64 DataStorageController::freeDiskBytes() const noexcept
{
    return m_impl->freeBytes;
}

QVariantList DataStorageController::cacheBreakdown() const
{
    return m_impl->breakdown;
}

QString DataStorageController::networkType() const
{
    return m_impl->networkTypeValue;
}

bool DataStorageController::networkMetered() const noexcept
{
    return m_impl->networkMeteredValue;
}

qint64 DataStorageController::totalUsageBytes() const noexcept
{
    qint64 total = 0;
    for (const auto& network : m_impl->usage) {
        for (const auto bytes : network) {
            total += static_cast<qint64>(bytes);
        }
    }
    return total;
}

qint64 DataStorageController::wifiUsageBytes() const noexcept
{
    qint64 total = 0;
    for (const auto bytes : m_impl->usage[networkIndex(storage_policy::NetworkClass::Wifi)]) {
        total += static_cast<qint64>(bytes);
    }
    return total;
}

qint64 DataStorageController::cellularUsageBytes() const noexcept
{
    qint64 total = 0;
    for (const auto bytes : m_impl->usage[networkIndex(storage_policy::NetworkClass::Cellular)]) {
        total += static_cast<qint64>(bytes);
    }
    return total;
}

QVariantList DataStorageController::usageBreakdown() const
{
    return usageBreakdownForNetwork(QStringLiteral("all"));
}

QVariantList DataStorageController::usageBreakdownForNetwork(const QString& network) const
{
    const QString normalized = network.trimmed().toLower();
    const bool allNetworks = normalized.isEmpty() || normalized == QStringLiteral("all");
    const auto selectedNetwork = networkFromQString(normalized);

    QVariantList result{};
    result.reserve(static_cast<qsizetype>(categoryCount));
    for (std::size_t categoryIndexValue = 0; categoryIndexValue < categoryCount;
         ++categoryIndexValue) {
        qulonglong total = 0;
        if (allNetworks) {
            for (std::size_t networkIndexValue = 0; networkIndexValue < networkCount;
                 ++networkIndexValue) {
                total += m_impl->usage[networkIndexValue][categoryIndexValue];
            }
        } else {
            total = m_impl->usage[networkIndex(selectedNetwork)][categoryIndexValue];
        }

        QVariantMap row;
        row.insert(
            QStringLiteral("id"),
            qtMediaCategoryId(static_cast<storage_policy::MediaCategory>(categoryIndexValue)));
        row.insert(QStringLiteral("bytes"), total);
        result.push_back(std::move(row));
    }
    return result;
}

qint64 DataStorageController::usageBytesForNetwork(const QString& network) const
{
    const QString normalized = network.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QStringLiteral("all")) {
        return totalUsageBytes();
    }

    qint64 total = 0;
    for (const auto bytes : m_impl->usage[networkIndex(networkFromQString(normalized))]) {
        total += static_cast<qint64>(bytes);
    }
    return total;
}

QDateTime DataStorageController::usageSince() const
{
    return m_impl->usageSinceValue;
}

qint64 DataStorageController::usageSinceMs() const noexcept
{
    return m_impl->usageSinceValue.toMSecsSinceEpoch();
}

bool DataStorageController::maintenanceBusy() const noexcept
{
    return m_impl->busy;
}

bool DataStorageController::autoDownloadAllowed(const QString& category) const
{
    const auto policy = m_impl->state->snapshot();
    return storage_policy::downloadRule(
        policy, categoryFromQString(category), policy.networkClass).enabled;
}

bool DataStorageController::autoDownloadEnabled(
    const QString& category, const QString& network) const
{
    const auto policy = m_impl->state->snapshot();
    return storage_policy::downloadRule(
        policy, categoryFromQString(category), networkFromQString(network)).enabled;
}

int DataStorageController::autoDownloadLimitMiB(
    const QString& category, const QString& network) const
{
    const auto policy = m_impl->state->snapshot();
    return static_cast<int>(storage_policy::downloadRule(
        policy, categoryFromQString(category), networkFromQString(network)).maximumBytes / mib);
}

int DataStorageController::retentionDays(const QString& category) const
{
    return storage_policy::retentionDays(
        m_impl->state->snapshot(), categoryFromQString(category));
}

QString DataStorageController::remoteImageSource(
    const QString& source, const QString& category, const bool manual) const
{
    const QString prefix = QStringLiteral("image://remote/");
    if (!source.startsWith(prefix, Qt::CaseInsensitive)) {
        return source;
    }

    QString encodedUrl = source.mid(prefix.size());
    if (encodedUrl.startsWith(QStringLiteral("v22/"))) {
        const auto parts = encodedUrl.split(QLatin1Char('/'));
        if (parts.size() >= 4) {
            encodedUrl = parts.mid(3).join(QLatin1Char('/'));
        }
    }

    return prefix + QStringLiteral("v22/") + qtMediaCategoryId(categoryFromQString(category))
        + QLatin1Char('/') + (manual ? QStringLiteral("manual/") : QStringLiteral("auto/"))
        + encodedUrl;
}

void DataStorageController::setAutoDownloadEnabled(
    const QString& category, const QString& network, const bool enabled)
{
    auto policy = m_impl->state->snapshot();
    policy.downloadRules[networkIndex(networkFromQString(network))]
                        [categoryIndex(categoryFromQString(category))]
        .enabled = enabled;
    m_impl->state->apply(std::move(policy));
    m_impl->persistPolicy();
    emit policyChanged();
}

void DataStorageController::setAutoDownloadLimitMiB(
    const QString& category, const QString& network, const int value)
{
    auto policy = m_impl->state->snapshot();
    policy.downloadRules[networkIndex(networkFromQString(network))]
                        [categoryIndex(categoryFromQString(category))]
        .maximumBytes = static_cast<std::uint64_t>(std::clamp(
                            value, minimumDownloadMiB, maximumDownloadMiB))
        * mib;
    m_impl->state->apply(std::move(policy));
    m_impl->persistPolicy();
    emit policyChanged();
}

void DataStorageController::setRetentionDays(const QString& category, const int days)
{
    if (!isSupportedRetention(days)) {
        return;
    }

    auto policy = m_impl->state->snapshot();
    policy.retentionDays[categoryIndex(categoryFromQString(category))] = days;
    m_impl->state->apply(std::move(policy));
    m_impl->persistPolicy();
    emit policyChanged();
    m_impl->scheduleMaintenance();
}

void DataStorageController::setDiskCacheEnabled(const bool enabled)
{
    auto policy = m_impl->state->snapshot();
    if (policy.diskCacheEnabled == enabled) {
        return;
    }

    policy.diskCacheEnabled = enabled;
    m_impl->state->apply(std::move(policy));
    m_impl->persistPolicy();
    emit policyChanged();
    if (enabled) {
        m_impl->scheduleMaintenance();
    }
}

void DataStorageController::setCacheLimitMiB(const int value)
{
    const int bounded = std::clamp(value, minimumCacheMiB, maximumCacheMiB);
    auto policy = m_impl->state->snapshot();
    const std::uint64_t nextBytes = static_cast<std::uint64_t>(bounded) * mib;
    if (policy.cacheBudgetBytes == nextBytes) {
        return;
    }

    policy.cacheBudgetBytes = nextBytes;
    m_impl->state->apply(std::move(policy));
    m_impl->persistPolicy();
    emit policyChanged();
    m_impl->scheduleMaintenance();
}

void DataStorageController::resetAutoDownloadSettings()
{
    const auto defaultPolicy = storage_policy::RuntimeState{}.snapshot();
    auto policy = m_impl->state->snapshot();
    policy.downloadRules = defaultPolicy.downloadRules;
    m_impl->state->apply(std::move(policy));
    m_impl->persistPolicy();
    emit policyChanged();
}

void DataStorageController::resetDataUsage()
{
    static_cast<void>(m_impl->state->takeUsageDeltas());
    for (auto& network : m_impl->usage) {
        network.fill(0U);
    }
    m_impl->usageSinceValue = QDateTime::currentDateTimeUtc();

    m_impl->settings.beginGroup(QStringLiteral("dataStorage/usage"));
    m_impl->settings.remove(QString{});
    m_impl->settings.endGroup();
    m_impl->settings.setValue(
        QStringLiteral("dataStorage/usageSince"), m_impl->usageSinceValue);
    m_impl->settings.sync();
    emit usageChanged();
}

void DataStorageController::clearCache(const QString& category)
{
    m_impl->startMaintenance(qtMediaCategoryId(categoryFromQString(category)), false);
}

void DataStorageController::clearEntireCache()
{
    m_impl->startMaintenance({}, true);
}

void DataStorageController::refreshStorageUsage()
{
    m_impl->startMaintenance({}, false);
}
