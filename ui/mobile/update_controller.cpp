#include "update_controller.hpp"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>

namespace {
const QUrl kReleaseApi{QStringLiteral("https://api.github.com/repos/genyleap/genycaster/releases/latest")};
const QUrl kReleasePage{QStringLiteral("https://github.com/genyleap/genycaster/releases")};

QString normalizeVersion(QString v)
{
    v = v.trimmed();
    if (v.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) v.remove(0, 1);
    return v;
}

bool safeDigest(const QString& digest)
{
    static const QRegularExpression pattern{QStringLiteral("^[0-9a-f]{64}$")};
    return pattern.match(digest.toLower()).hasMatch();
}

bool matchingAsset(const QString& name)
{
    const QString n = name.toLower();
    const QString arch = QSysInfo::currentCpuArchitecture().toLower();
    const bool arm = arch.contains(QStringLiteral("arm")) || arch.contains(QStringLiteral("aarch64"));
    const bool x64 = arch.contains(QStringLiteral("x86_64")) || arch.contains(QStringLiteral("amd64"));
    // Refuse wrong-architecture packages; prefer explicit architecture labels.
    const bool wrongArch = (arm && (n.contains(QStringLiteral("x86_64")) ||
                                    n.contains(QStringLiteral("x64")))) ||
                           (x64 && (n.contains(QStringLiteral("arm64")) ||
                                    n.contains(QStringLiteral("aarch64"))));
    if (wrongArch) return false;
#if defined(Q_OS_MACOS)
    return (n.endsWith(QStringLiteral(".dmg")) || n.endsWith(QStringLiteral(".pkg")))
        && n.contains(QStringLiteral("mac"));
#elif defined(Q_OS_WIN)
    return n.endsWith(QStringLiteral(".msi")) || n.endsWith(QStringLiteral(".exe"));
#elif defined(Q_OS_LINUX) && !defined(Q_OS_WASM)
    return n.endsWith(QStringLiteral(".appimage")) || n.endsWith(QStringLiteral(".deb"))
        || n.endsWith(QStringLiteral(".rpm"));
#elif defined(Q_OS_ANDROID)
    return n.endsWith(QStringLiteral(".apk"));
#else
    return false;
#endif
}
bool isOfficialAssetUrl(const QUrl& u)
{
    return u.scheme() == QStringLiteral("https") &&
        u.host().compare(QStringLiteral("github.com"), Qt::CaseInsensitive) == 0 &&
        u.path().startsWith(QStringLiteral("/genyleap/genycaster/releases/download/"));
}
} // namespace

UpdateController::UpdateController(QSettings& settings, const bool startupCheck, QObject* parent)
    : QObject{parent}, m_settings{settings},
      m_appVersion{QCoreApplication::applicationVersion()},
      m_status{QStringLiteral("No update check yet")}
{
    m_autoCheckEnabled = m_settings.value(QStringLiteral("updates/autoCheck"), true).toBool();
    m_periodic = new QTimer(this);
    m_periodic->setInterval(24 * 60 * 60 * 1000);
    connect(m_periodic, &QTimer::timeout, this, [this] {
        if (m_autoCheckEnabled) check(false);
    });
    m_periodic->start();
    if (startupCheck) {
        QTimer::singleShot(10'000, this, [this] {
            if (m_autoCheckEnabled) check(false);
        });
    }
}

UpdateController::~UpdateController()
{
    if (m_checkReply) {
        disconnect(m_checkReply, nullptr, this, nullptr);
        m_checkReply->abort();
    }
    if (m_downloadReply) {
        disconnect(m_downloadReply, nullptr, this, nullptr);
        m_downloadReply->abort();
    }
    if (m_file) m_file->close();
    if (!m_partialPath.isEmpty()) QFile::remove(m_partialPath);
}

QString UpdateController::appVersion() const { return m_appVersion; }
bool UpdateController::autoCheckEnabled() const { return m_autoCheckEnabled; }
bool UpdateController::checking() const { return m_checking; }
bool UpdateController::downloading() const { return m_downloading; }
bool UpdateController::updateAvailable() const { return m_updateAvailable; }
QString UpdateController::latestVersion() const { return m_latestVersion; }
QString UpdateController::status() const { return m_status; }
QString UpdateController::error() const { return m_error; }
QString UpdateController::downloadedFilePath() const { return m_downloadedFilePath; }
QString UpdateController::releaseUrl() const { return kReleasePage.toString(); }
bool UpdateController::canDownload() const {
    return downloadSupported() && m_updateAvailable && !m_assetUrl.isEmpty();
}
#if defined(Q_OS_IOS) || defined(Q_OS_WASM)
bool UpdateController::downloadSupported() const { return false; }
#else
bool UpdateController::downloadSupported() const { return true; }
#endif

double UpdateController::downloadProgress() const
{
    if (!m_downloading || m_total <= 0) return 0;
    return qBound(0.0, static_cast<double>(m_received) / static_cast<double>(m_total), 1.0);
}

void UpdateController::setAutoCheckEnabled(const bool enabled)
{
    if (m_autoCheckEnabled == enabled) return;
    m_autoCheckEnabled = enabled;
    m_settings.setValue(QStringLiteral("updates/autoCheck"), enabled);
    m_settings.sync();
    emit changed();
}

void UpdateController::checkForUpdates() { check(true); }

void UpdateController::check(const bool manual)
{
    if (m_checking || m_downloading) return;
    m_checking = true;
    m_error.clear();
    m_status = QStringLiteral("Checking for updates…");
    emit changed();
    QNetworkRequest request{kReleaseApi};
    request.setRawHeader("User-Agent", "Genycaster-Updater/1.0");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(12'000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = m_network.get(request);
    m_checkReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual] {
        handleRelease(reply, manual);
    });
}

void UpdateController::handleRelease(QNetworkReply* reply, const bool manual)
{
    if (m_checkReply != reply) return;
    m_checkReply = nullptr;
    m_checking = false;
    const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const auto netError = reply->error();
    const QString netErrorText = reply->errorString();
    reply->deleteLater();
    if (code == 404) {
        m_updateAvailable = false;
        m_status = QStringLiteral("No published release yet");
        m_error.clear();
        emit changed();
        return;
    }
    if (netError != QNetworkReply::NoError || code != 200 || body.size() > 2'000'000) {
        m_updateAvailable = false;
        if (manual) fail(QStringLiteral("Could not check releases: %1").arg(netErrorText));
        else { m_status = QStringLiteral("Update check unavailable"); emit changed(); }
        return;
    }
    const auto doc = QJsonDocument::fromJson(body);
    const auto obj = doc.object();
    if (!doc.isObject() || obj.value(QStringLiteral("draft")).toBool() ||
        obj.value(QStringLiteral("prerelease")).toBool()) {
        fail(QStringLiteral("Invalid release metadata"));
        return;
    }
    const QString version = normalizeVersion(obj.value(QStringLiteral("tag_name")).toString());
    const auto parsed = QVersionNumber::fromString(version);
    const auto current = QVersionNumber::fromString(normalizeVersion(m_appVersion));
    if (parsed.isNull() || current.isNull()) {
        fail(QStringLiteral("Unrecognized release version"));
        return;
    }
    m_latestVersion = version;
    m_updateAvailable = QVersionNumber::compare(parsed, current) > 0;
    m_assetUrl.clear();
    m_assetName.clear();
    m_assetDigest.clear();
    m_downloadedFilePath.clear();
    if (m_updateAvailable) {
        const auto assets = obj.value(QStringLiteral("assets")).toArray();
        for (const auto& value : assets) {
            const auto asset = value.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            const QUrl url{asset.value(QStringLiteral("browser_download_url")).toString()};
            if (!matchingAsset(name) || !isOfficialAssetUrl(url)) continue;
            QString digest = asset.value(QStringLiteral("digest")).toString().toLower();
            if (digest.startsWith(QStringLiteral("sha256:"))) digest.remove(0, 7);
            if (!safeDigest(digest)) continue; // fail closed: no unverified installer
            m_assetName = name;
            m_assetUrl = url.toString();
            m_assetDigest = digest;
            break;
        }
        m_status = m_assetUrl.isEmpty()
            ? QStringLiteral("Update available; use release page (no verified compatible package)")
            : QStringLiteral("Verified update available");
    } else {
        m_status = QStringLiteral("Up to date");
    }
    m_error.clear();
    emit changed();
}

void UpdateController::fail(QString message)
{
    m_error = std::move(message);
    m_status = QStringLiteral("Update operation failed");
    emit changed();
}

void UpdateController::downloadUpdate()
{
    if (!downloadSupported() || m_downloading || m_checking ||
        !m_updateAvailable || m_assetUrl.isEmpty() || !safeDigest(m_assetDigest)) return;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                        + QStringLiteral("/updates");
    if (!QDir{}.mkpath(dir)) {
        fail(QStringLiteral("Could not create update directory"));
        return;
    }
    m_partialPath = dir + QLatin1Char('/') + QFileInfo(m_assetName).fileName() +
                    QStringLiteral(".partial");
    m_file = std::make_unique<QFile>(m_partialPath);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_file.reset();
        fail(QStringLiteral("Could not open update destination"));
        return;
    }
    m_hash.reset();
    m_received = 0;
    m_total = 0;
    m_downloading = true;
    m_error.clear();
    m_status = QStringLiteral("Downloading verified package…");
    emit changed();
    QNetworkRequest req{QUrl{m_assetUrl}};
    req.setRawHeader("User-Agent", "Genycaster-Updater/1.0");
    req.setTransferTimeout(90'000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = m_network.get(req);
    m_downloadReply = reply;
    connect(reply, &QIODevice::readyRead, this, [this, reply] {
        if (!m_file || !m_file->isOpen()) return;
        const auto bytes = reply->readAll();
        if (bytes.size() > 0 && m_file->write(bytes) != bytes.size()) {
            reply->abort();
            return;
        }
        m_hash.addData(bytes);
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        m_received = received;
        m_total = total;
        emit changed();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] { finishDownload(reply); });
}

void UpdateController::finishDownload(QNetworkReply* reply)
{
    if (m_downloadReply != reply) return;
    m_downloadReply = nullptr;
    if (m_file && m_file->isOpen()) {
        const auto rest = reply->readAll();
        if (m_file->write(rest) != rest.size()) {
            m_file->close();
            reply->deleteLater();
            m_downloading = false;
            m_file.reset();
            QFile::remove(m_partialPath);
            m_partialPath.clear();
            fail(QStringLiteral("Update file write failed"));
            return;
        }
        m_hash.addData(rest);
        m_file->flush();
        m_file->close();
    }
    const bool ok = reply->error() == QNetworkReply::NoError &&
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200 &&
        QString::fromLatin1(m_hash.result().toHex()) == m_assetDigest;
    reply->deleteLater();
    m_file.reset();
    m_downloading = false;
    if (!ok) {
        QFile::remove(m_partialPath);
        m_partialPath.clear();
        fail(QStringLiteral("Download failed or SHA-256 verification failed"));
        return;
    }
    const QString finalPath = m_partialPath.left(m_partialPath.size() - 8);
    if (QFile::exists(finalPath) && !QFile::remove(finalPath)) {
        fail(QStringLiteral("Cannot replace previous update package"));
        return;
    }
    if (!QFile::rename(m_partialPath, finalPath)) {
        fail(QStringLiteral("Cannot finalize verified update"));
        return;
    }
    m_partialPath.clear();
    m_downloadedFilePath = finalPath;
    m_status = QStringLiteral("Package verified; open installer to update");
    emit changed();
}

bool UpdateController::openDownloadedUpdate()
{
    if (!downloadSupported() || m_downloadedFilePath.isEmpty() ||
        !QFileInfo::exists(m_downloadedFilePath)) return false;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadedFilePath));
}

bool UpdateController::openReleasePage()
{
    return QDesktopServices::openUrl(kReleasePage);
}
