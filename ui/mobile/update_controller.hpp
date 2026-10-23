#pragma once

#include <QObject>
#include <QString>
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
 * Inspired by GenyConnect's update workflow. Never silently executes code:
 * downloads require a platform-matching asset and a verified SHA-256 digest;
 * installation is delegated to the OS after an explicit user click.
 */
class UpdateController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(bool autoCheckEnabled READ autoCheckEnabled NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(bool downloading READ downloading NOTIFY changed)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY changed)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString releaseTitle READ releaseTitle NOTIFY changed)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY changed)
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
    [[nodiscard]] QString appVersion() const;
    [[nodiscard]] bool autoCheckEnabled() const;
    [[nodiscard]] bool checking() const;
    [[nodiscard]] bool downloading() const;
    [[nodiscard]] bool updateAvailable() const;
    [[nodiscard]] QString latestVersion() const;
    [[nodiscard]] QString releaseTitle() const;
    [[nodiscard]] QString releaseNotes() const;
    [[nodiscard]] QString releasePublishedAt() const;
    [[nodiscard]] QString status() const;
    [[nodiscard]] QString error() const;
    [[nodiscard]] double downloadProgress() const;
    [[nodiscard]] QString downloadedFilePath() const;
    [[nodiscard]] QString releaseUrl() const;
    [[nodiscard]] bool downloadSupported() const;
    [[nodiscard]] bool canDownload() const;
    [[nodiscard]] bool webRefreshing() const;

    Q_INVOKABLE void setAutoCheckEnabled(bool enabled);
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void refreshWebApp();
    Q_INVOKABLE void downloadUpdate();
    Q_INVOKABLE bool openDownloadedUpdate();
    Q_INVOKABLE bool openReleasePage();

signals:
    void changed();

private:
    void check(bool manual);
    void handleRelease(QNetworkReply* reply, bool manual);
    void finishDownload(QNetworkReply* reply);
    void fail(QString message);
    QSettings& m_settings;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_checkReply;
    QPointer<QNetworkReply> m_downloadReply;
    std::unique_ptr<QFile> m_file;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    QString m_appVersion;
    QString m_latestVersion;
    QString m_releaseTitle;
    QString m_releaseNotes;
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
    qint64 m_received{0};
    qint64 m_total{0};
    QTimer* m_periodic{nullptr};
};
