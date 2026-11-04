#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QPointer>

#include <memory>

class QFile;
class QNetworkReply;
class QSettings;
class QTimer;

/**
 * Asynchronous, opt-out GitHub Releases update discovery.
 *
 * Fail-closed update discovery. Never silently executes code:
 * downloads require a platform-matching asset and a verified SHA-256 digest;
 * installation is delegated to the OS after an explicit user click.
 */
class UpdateController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString productName READ productName CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString versionNumber READ versionNumber CONSTANT)
    Q_PROPERTY(QString releaseChannel READ releaseChannel CONSTANT)
    Q_PROPERTY(int releaseChannelNumber READ releaseChannelNumber CONSTANT)
    Q_PROPERTY(QString releaseTag READ releaseTag CONSTANT)
    Q_PROPERTY(QString releaseSummary READ releaseSummary NOTIFY changed)
    Q_PROPERTY(QString webLatestBuild READ webLatestBuild NOTIFY changed)
    Q_PROPERTY(bool webUpdateChecked READ webUpdateChecked NOTIFY changed)
    Q_PROPERTY(bool autoCheckEnabled READ autoCheckEnabled NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(bool downloading READ downloading NOTIFY changed)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY changed)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString releaseTitle READ releaseTitle NOTIFY changed)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY changed)
    Q_PROPERTY(QString releaseType READ releaseType NOTIFY changed)
    Q_PROPERTY(QString webCurrentBuild READ webCurrentBuild CONSTANT)
    Q_PROPERTY(QString releasePublishedAt READ releasePublishedAt NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(double downloadProgress READ downloadProgress NOTIFY changed)
    Q_PROPERTY(QString downloadedFilePath READ downloadedFilePath NOTIFY changed)
    Q_PROPERTY(QString releaseUrl READ releaseUrl CONSTANT)
    Q_PROPERTY(bool downloadSupported READ downloadSupported CONSTANT)
    Q_PROPERTY(bool canDownload READ canDownload NOTIFY changed)
    Q_PROPERTY(bool webRefreshing READ webRefreshing NOTIFY changed)

public:
    explicit UpdateController(QSettings& settings, bool startupCheck = true, QObject* parent = nullptr);
    ~UpdateController() override;
    [[nodiscard]] QString productName() const;
    [[nodiscard]] QString appVersion() const;
    [[nodiscard]] QString versionNumber() const;
    [[nodiscard]] QString releaseChannel() const;
    [[nodiscard]] int releaseChannelNumber() const;
    [[nodiscard]] QString releaseTag() const;
    [[nodiscard]] QString releaseSummary() const;
    [[nodiscard]] QString webLatestBuild() const;
    [[nodiscard]] bool webUpdateChecked() const;
    [[nodiscard]] bool autoCheckEnabled() const;
    [[nodiscard]] bool checking() const;
    [[nodiscard]] bool downloading() const;
    [[nodiscard]] bool updateAvailable() const;
    [[nodiscard]] QString latestVersion() const;
    [[nodiscard]] QString releaseTitle() const;
    [[nodiscard]] QString releaseNotes() const;
    [[nodiscard]] QString releaseType() const;
    [[nodiscard]] QString webCurrentBuild() const;
    [[nodiscard]] QString releasePublishedAt() const;
    [[nodiscard]] QString status() const;
    [[nodiscard]] QString error() const;
    [[nodiscard]] double downloadProgress() const;
    [[nodiscard]] QString downloadedFilePath() const;
    [[nodiscard]] QString releaseUrl() const;
    [[nodiscard]] bool downloadSupported() const;
    [[nodiscard]] bool canDownload() const;
    [[nodiscard]] bool webRefreshing() const;

    void setLanguageTag(QString tag);
    Q_INVOKABLE void setAutoCheckEnabled(bool enabled);
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void refreshWebApp();
    /** @brief Cancels a browser refresh if its durable local-state flush fails. */
    void browserRefreshPersistenceFailed();
    Q_INVOKABLE void downloadUpdate();
    Q_INVOKABLE bool openDownloadedUpdate();
    Q_INVOKABLE bool openReleasePage();

signals:
    void changed();

private:
    void check(bool manual);
    void checkWebManifest(bool manual);
    void handleRelease(QNetworkReply* reply, bool manual);
    void finishDownload(QNetworkReply* reply);
    void fail(QString message);
    void applyReleaseTranslation();
    QSettings& m_settings;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_checkReply;
    QPointer<QNetworkReply> m_downloadReply;
    std::unique_ptr<QFile> m_file;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    QString m_appVersion;
    QString m_latestVersion;
    QJsonObject m_releaseData;
    QString m_languageTag{QStringLiteral("en-US")};
    QString m_releaseSummary;
    QString m_webLatestBuild;
    QString m_releaseTitle;
    QString m_releaseNotes;
    QString m_releaseType;
    QString m_webCurrentBuild;
    QString m_releasePublishedAt;
    QString m_status;
    QString m_error;
    QString m_assetUrl;
    QString m_assetName;
    QString m_assetDigest;
    QString m_partialPath;
    QString m_downloadedFilePath;
    bool m_autoCheckEnabled{true};
    bool m_checking{false};
    bool m_downloading{false};
    bool m_updateAvailable{false};
    bool m_webRefreshing{false};
    bool m_webUpdateChecked{false};
    qint64 m_received{0};
    qint64 m_total{0};
    QTimer* m_periodic{nullptr};
};
