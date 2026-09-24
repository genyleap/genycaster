// Textual Qt boundary for QObject/Q_PROPERTY exposure to QML.
#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>

class QSettings;

/**
 * @brief Owns Genycaster's adaptive media download, cache, retention and usage settings.
 *
 * The controller is a Qt presentation boundary shared by desktop and mobile QML. It persists
 * device-local policy in the supplied QSettings instance, exposes immutable snapshots to the
 * remote-media provider, and schedules cache enumeration/maintenance away from the GUI thread.
 * The supplied QSettings object must outlive this controller.
 */
class DataStorageController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool diskCacheEnabled READ diskCacheEnabled WRITE setDiskCacheEnabled NOTIFY policyChanged)
    Q_PROPERTY(int cacheLimitMiB READ cacheLimitMiB WRITE setCacheLimitMiB NOTIFY policyChanged)
    Q_PROPERTY(qint64 cacheUsageBytes READ cacheUsageBytes NOTIFY storageChanged)
    Q_PROPERTY(qint64 freeDiskBytes READ freeDiskBytes NOTIFY storageChanged)
    Q_PROPERTY(QVariantList cacheBreakdown READ cacheBreakdown NOTIFY storageChanged)
    Q_PROPERTY(QString networkType READ networkType NOTIFY networkChanged)
    Q_PROPERTY(bool networkMetered READ networkMetered NOTIFY networkChanged)
    Q_PROPERTY(qint64 totalUsageBytes READ totalUsageBytes NOTIFY usageChanged)
    Q_PROPERTY(qint64 wifiUsageBytes READ wifiUsageBytes NOTIFY usageChanged)
    Q_PROPERTY(qint64 cellularUsageBytes READ cellularUsageBytes NOTIFY usageChanged)
    Q_PROPERTY(QVariantList usageBreakdown READ usageBreakdown NOTIFY usageChanged)
    Q_PROPERTY(QDateTime usageSince READ usageSince NOTIFY usageChanged)
    Q_PROPERTY(qint64 usageSinceMs READ usageSinceMs NOTIFY usageChanged)
    Q_PROPERTY(bool maintenanceBusy READ maintenanceBusy NOTIFY storageChanged)
public:
    /**
     * @brief Creates the device-local Data & Storage presentation controller.
     * @param settings Persistent settings store. It must outlive the controller.
     * @param parent Optional QObject owner.
     */
    explicit DataStorageController(QSettings& settings, QObject* parent = nullptr);

    /** @brief Stops timers/work publication and flushes pending device-local settings. */
    ~DataStorageController() override;

    /**
     * @return Opaque shared handle to the module-owned thread-safe media policy state.
     *
     * The concrete RuntimeState stays inside C++ module implementation units. Keeping it out of
     * this Qt/MOC textual header avoids cross-module ownership/ABI mismatches on Clang while
     * preserving shared lifetime between the controller and asynchronous media workers.
     */
    [[nodiscard]] std::shared_ptr<void> runtimeStateHandle() const;

    /** @return Whether newly downloaded remote media may be persisted on disk. */
    [[nodiscard]] bool diskCacheEnabled() const noexcept;

    /** @return Global media-cache budget in mebibytes. */
    [[nodiscard]] int cacheLimitMiB() const noexcept;

    /** @return Bytes currently occupied by managed and migratable legacy media cache entries. */
    [[nodiscard]] qint64 cacheUsageBytes() const noexcept;

    /** @return Free bytes on the filesystem containing the media cache. */
    [[nodiscard]] qint64 freeDiskBytes() const noexcept;

    /**
     * @return QML rows containing category id, byte count and file count for cached media.
     * @note The list reflects the most recently completed asynchronous storage scan.
     */
    [[nodiscard]] QVariantList cacheBreakdown() const;

    /** @return Presentation network id: wifi, ethernet, cellular, or unknown. */
    [[nodiscard]] QString networkType() const;

    /** @return Whether Qt reports the current network transport as metered. */
    [[nodiscard]] bool networkMetered() const noexcept;

    /** @return Total successfully downloaded remote-media bytes recorded since the last reset. */
    [[nodiscard]] qint64 totalUsageBytes() const noexcept;

    /** @return Successfully downloaded remote-media bytes attributed to Wi-Fi/Ethernet. */
    [[nodiscard]] qint64 wifiUsageBytes() const noexcept;

    /** @return Successfully downloaded remote-media bytes attributed to cellular transport. */
    [[nodiscard]] qint64 cellularUsageBytes() const noexcept;

    /** @return QML rows containing media-category ids and recorded downloaded-byte totals. */
    [[nodiscard]] QVariantList usageBreakdown() const;

    /**
     * @brief Returns media usage rows filtered to one network scope.
     * @param network `all`, `wifi`/`ethernet`, `cellular`, or `other`.
     * @return QML rows containing category id and downloaded-byte total for that scope.
     */
    Q_INVOKABLE QVariantList usageBreakdownForNetwork(const QString& network) const;

    /**
     * @brief Returns total downloaded media bytes for one network scope.
     * @param network `all`, `wifi`/`ethernet`, `cellular`, or `other`.
     */
    Q_INVOKABLE qint64 usageBytesForNetwork(const QString& network) const;

    /** @return UTC timestamp from which the current media usage statistics are accumulated. */
    [[nodiscard]] QDateTime usageSince() const;

    /** @return usageSince() as Unix milliseconds for deterministic QML formatting. */
    [[nodiscard]] qint64 usageSinceMs() const noexcept;

    /** @return Whether asynchronous cache scan/eviction/deletion work is currently active. */
    [[nodiscard]] bool maintenanceBusy() const noexcept;

    /**
     * @brief Tests the automatic-download rule for a category on the current network.
     * @param category Stable media category id.
     * @return true when automatic requests for that category are permitted.
     */
    Q_INVOKABLE bool autoDownloadAllowed(const QString& category) const;

    /**
     * @brief Reads whether automatic download is enabled for a category/network pair.
     * @param category Stable media category id.
     * @param network Stable network id (wifi, cellular, or unknown).
     */
    Q_INVOKABLE bool autoDownloadEnabled(const QString& category, const QString& network) const;

    /**
     * @brief Reads the configured maximum automatic download size.
     * @return Limit in mebibytes for the category/network pair.
     */
    Q_INVOKABLE int autoDownloadLimitMiB(const QString& category, const QString& network) const;

    /**
     * @brief Reads the retention window for one cache category.
     * @return Days to retain recently used entries; zero means Never auto-remove.
     */
    Q_INVOKABLE int retentionDays(const QString& category) const;

    /**
     * @brief Encodes a remote URL as the category-aware image-provider source used by QML.
     * @param source Original remote or already-provider-backed URL.
     * @param category Stable media category id.
     * @param manual true for an explicit user download that may bypass auto-download enablement.
     * @return Provider URL suitable for a QML Image source, or the original local/non-remote source.
     */
    Q_INVOKABLE QString remoteImageSource(const QString& source, const QString& category, bool manual) const;

    /** @brief Enables or disables automatic download for a category/network pair. */
    Q_INVOKABLE void setAutoDownloadEnabled(const QString& category, const QString& network, bool enabled);

    /** @brief Sets a validated per-file automatic download limit in mebibytes. */
    Q_INVOKABLE void setAutoDownloadLimitMiB(const QString& category, const QString& network, int value);

    /** @brief Sets a supported retention duration in days; zero means Never. */
    Q_INVOKABLE void setRetentionDays(const QString& category, int days);

    /** @brief Enables or disables persistence of newly downloaded media on disk. */
    Q_INVOKABLE void setDiskCacheEnabled(bool enabled);

    /** @brief Sets the global cache budget, clamped to the supported 64–8192 MiB range. */
    Q_INVOKABLE void setCacheLimitMiB(int value);

    /** @brief Restores default per-network auto-download rules and file-size limits. */
    Q_INVOKABLE void resetAutoDownloadSettings();

    /** @brief Clears only locally recorded remote-media usage counters and restarts their epoch. */
    Q_INVOKABLE void resetDataUsage();

    /** @brief Asynchronously removes cached files in the requested media category. */
    Q_INVOKABLE void clearCache(const QString& category);

    /** @brief Asynchronously removes all managed media cache entries, including legacy entries. */
    Q_INVOKABLE void clearEntireCache();

    /** @brief Schedules an asynchronous cache scan and policy maintenance pass. */
    Q_INVOKABLE void refreshStorageUsage();

signals:
    /** @brief Emitted after persisted download/cache policy changes. */
    void policyChanged();

    /** @brief Emitted after storage metrics or maintenance state changes. */
    void storageChanged();

    /** @brief Emitted after media-network accounting or its epoch changes. */
    void usageChanged();

    /** @brief Emitted after current network transport or metering state changes. */
    void networkChanged();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
