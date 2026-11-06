#include "update_controller.hpp"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVersionNumber>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

import genycaster.ui.update.install;

namespace {
const QUrl kReleaseApi{QStringLiteral("https://api.github.com/repos/genyleap/genycaster/releases?per_page=20")};
const QUrl kReleasePage{QStringLiteral("https://github.com/genyleap/genycaster/releases")};
#if defined(__EMSCRIPTEN__)
// At most one browser refresh is pending; do not retain a dangling QObject.
QPointer<UpdateController> pendingBrowserRefresh{};
#endif

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

// A release candidate is newer than an earlier RC but older than the stable
// release with the same three numeric components. QVersionNumber alone ignores
// prerelease suffixes and would otherwise treat a candidate as its stable release.
struct ReleaseVersion {
    QVersionNumber core{};
    int channelRank{3}; // alpha=0, beta=1, rc=2, stable=3
    int channelNumber{0};
    bool valid{false};
};

ReleaseVersion parseReleaseVersion(const QString& value)
{
    static const QRegularExpression pattern{
        QStringLiteral(R"(^(\d+)\.(\d+)\.(\d+)(?:-(alpha|beta|rc)([1-9]\d*))?$)")};
    const auto match = pattern.match(normalizeVersion(value));
    if (!match.hasMatch()) return {};
    bool majorOk = false, minorOk = false, patchOk = false, indexOk = true;
    const int major = match.captured(1).toInt(&majorOk);
    const int minor = match.captured(2).toInt(&minorOk);
    const int patch = match.captured(3).toInt(&patchOk);
    const QString channel = match.captured(4);
    const int channelRank = channel == QStringLiteral("alpha") ? 0
                          : channel == QStringLiteral("beta") ? 1
                          : channel == QStringLiteral("rc") ? 2 : 3;
    const int index = match.captured(5).isEmpty()
        ? 0 : match.captured(5).toInt(&indexOk);
    if (!majorOk || !minorOk || !patchOk || !indexOk) return {};
    return {QVersionNumber{major, minor, patch}, channelRank, index, true};
}

int compareReleaseVersions(const ReleaseVersion& left, const ReleaseVersion& right)
{
    const int core = QVersionNumber::compare(left.core, right.core);
    if (core != 0) return core;
    if (left.channelRank != right.channelRank)
        return left.channelRank > right.channelRank ? 1 : -1;
    if (left.channelNumber == right.channelNumber) return 0;
    return left.channelNumber > right.channelNumber ? 1 : -1;
}

// Parse the canonical release JSON embedded by CMake for offline native and WASM use.
QJsonObject loadBundledReleaseNotes(const QString& version)
{
    static const QRegularExpression validVersion{
        QStringLiteral(R"(^[0-9]+\.[0-9]+\.[0-9]+(?:-(?:alpha|beta|rc)[1-9][0-9]*)?$)")};
    if (!validVersion.match(version).hasMatch()) return {};
    QFile resource{QStringLiteral(":/genycaster-release-notes/v%1.json").arg(version)};
    if (!resource.open(QIODevice::ReadOnly)) return {};
    const auto doc = QJsonDocument::fromJson(resource.readAll());
    const auto object = doc.object();
    if (!doc.isObject() || object.value(QStringLiteral("schemaVersion")).toInt() != 1)
        return {};
    return object;
}

// GitHub Release markdown is deterministically generated from the same JSON.
// Locale markers keep future native update notices localized without requiring
// a mutable web endpoint or a separately maintained changelog.
QJsonObject parsePublishedNotes(const QString& body)
{
    if (body.size() > 100'000 || !body.contains(QStringLiteral("<!-- genycaster-locale:")))
        return {};
    QJsonObject translations;
    QJsonArray changes;
    QString releaseType;
    for (const QString& raw : body.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.startsWith(QStringLiteral("<!-- genycaster-release-type:"))
            && line.endsWith(QStringLiteral(" -->"))) {
            const QString kind = line.mid(29, line.size() - 33);
            if (kind == QStringLiteral("security") || kind == QStringLiteral("feature")
                || kind == QStringLiteral("fix") || kind == QStringLiteral("improvement")
                || kind == QStringLiteral("maintenance"))
                releaseType = kind;
            break;
        }
    }
    QString locale;
    QString title;
    QString summary;
    QJsonObject items;
    int index = 0;
    const auto store = [&] {
        if (!locale.isEmpty() && !title.isEmpty() && !items.isEmpty()) {
            translations.insert(locale, QJsonObject{
                {QStringLiteral("title"), title},
                {QStringLiteral("summary"), summary},
                {QStringLiteral("items"), items}});
        }
    };
    for (const QString& raw : body.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.startsWith(QStringLiteral("<!-- genycaster-locale:"))
            && line.endsWith(QStringLiteral(" -->"))) {
            store();
            locale = line.mid(23, line.size() - 27); // marker prefix + closing " -->"
            if (!QRegularExpression{QStringLiteral(R"(^[a-z]{2}-[A-Z]{2}$)")}
                     .match(locale).hasMatch())
                locale.clear();
            title.clear();
            summary.clear();
            items = {};
            index = 0;
        } else if (!locale.isEmpty() && line.startsWith(QStringLiteral("## "))) {
            title = line.mid(3).left(120);
        } else if (!locale.isEmpty() && line.startsWith(QStringLiteral("- ")) && index < 12) {
            const QString key = QStringLiteral("change-%1").arg(index++);
            items.insert(key, line.mid(2).left(400));
            if (changes.size() < index) changes.push_back(
                QJsonObject{{QStringLiteral("id"), key}});
        } else if (!locale.isEmpty() && !title.isEmpty() && summary.isEmpty()
                   && !line.isEmpty() && !line.startsWith(QLatin1Char('<'))) {
            summary = line.left(220);
        }
    }
    store();
    return translations.isEmpty() ? QJsonObject{} : QJsonObject{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("type"), releaseType},
        {QStringLiteral("changes"), changes},
        {QStringLiteral("translations"), translations}};
}

QString releaseArchitecture()
{
#if defined(Q_OS_ANDROID)
    // Process architecture is the APK ABI, not an optional host-emulated ABI.
#  if defined(__aarch64__)
    return QStringLiteral("arm64-v8a");
#  elif defined(__arm__)
    return QStringLiteral("armeabi-v7a");
#  else
    return {};
#  endif
#else
    const QString cpu = QSysInfo::currentCpuArchitecture().toLower();
    if (cpu == QStringLiteral("arm64") || cpu == QStringLiteral("aarch64"))
        return QStringLiteral("arm64");
    if (cpu == QStringLiteral("x86_64") || cpu == QStringLiteral("amd64"))
        return QStringLiteral("x86_64");
    return {}; // Release matrix does not ship other desktop architectures.
#endif
}

bool matchingAsset(const QString& name)
{
    const QString n = name.toLower();
    const QString arch = releaseArchitecture();
    // Match exact OS, ABI, extension and official package naming. Never
    // mistake a development helper, runtime snapshot or adjacent platform
    // file for an installable update package.
    if (arch.isEmpty() || !n.startsWith(QStringLiteral("genycaster-v")) ||
        n.contains(QStringLiteral("-selfupdate-")) ||
        n.contains(QStringLiteral("-ci-test")) ||
        n.endsWith(QStringLiteral(".zsync"))) return false;
#if defined(Q_OS_MACOS)
    return n.endsWith(QStringLiteral("-macos-") + arch + QStringLiteral(".dmg"));
#elif defined(Q_OS_WIN)
    return n.endsWith(QStringLiteral("-windows-") + arch + QStringLiteral(".exe"));
#elif defined(Q_OS_LINUX) && !defined(Q_OS_WASM)
    return n.endsWith(QStringLiteral("-linux-") + arch + QStringLiteral(".appimage"))
        || n.endsWith(QStringLiteral("-linux-") + arch + QStringLiteral(".deb"));
#elif defined(Q_OS_ANDROID)
    return n.endsWith(QStringLiteral("-android-") + arch + QStringLiteral(".apk"));
#else
    return false;
#endif
}

int assetPreference(const QString& name)
{
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID) && !defined(__EMSCRIPTEN__)
    const bool runningAppImage = !qEnvironmentVariable("APPIMAGE").isEmpty();
    const QString distro = QSysInfo::productType().toLower();
    const bool debian = distro == QStringLiteral("debian")
        || distro == QStringLiteral("ubuntu")
        || distro == QStringLiteral("linuxmint");
    const bool appImage = name.endsWith(QStringLiteral(".AppImage"), Qt::CaseInsensitive);
    // Use the same install format to retain reliable migration behavior.
    return appImage == (runningAppImage || !debian) ? 20 : 10;
#else
    Q_UNUSED(name);
    return 10;
#endif
}

bool isOfficialAssetUrl(const QUrl& u)
{
    return u.scheme() == QStringLiteral("https") &&
        u.host().compare(QStringLiteral("github.com"), Qt::CaseInsensitive) == 0 &&
        u.path().startsWith(QStringLiteral("/genyleap/genycaster/releases/download/"));
}
} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE void genycasterWebRefreshSaveFailure()
{
    if (pendingBrowserRefresh)
        pendingBrowserRefresh->browserRefreshPersistenceFailed();
}
#endif

void UpdateController::browserRefreshPersistenceFailed()
{
    if (!m_webRefreshing) return;
    m_webRefreshing = false;
    fail(QStringLiteral("Could not save browser data. Refresh cancelled; please retry."));
}

UpdateController::UpdateController(QSettings& settings, const bool startupCheck, QObject* parent)
    : QObject{parent}, m_settings{settings},
      m_appVersion{QCoreApplication::applicationVersion()},
      m_status{QStringLiteral("No update check yet")}
{
    // Show the installed release's bundled notes offline, on both desktop
    // and mobile. Future update notes still come from their published release.
    m_releaseData = loadBundledReleaseNotes(m_appVersion);
    applyReleaseTranslation();
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    // The supervised post-exit helper cannot report to a dead QObject.
    // Read its bounded, untrusted status text once on the next app launch.
    const QString lastStatus = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/updates/genycaster-native-update.status");
    QFile priorInstall{lastStatus};
    if (priorInstall.open(QIODevice::ReadOnly)) {
        const QByteArray result = priorInstall.read(80).trimmed();
        priorInstall.close();
        QFile::remove(lastStatus);
        if (result.startsWith("failed:") && result.size() <= 16) {
            bool validCode = false;
            const int code = result.mid(7).toInt(&validCode);
            if (validCode && code > 0) {
                m_lastInstallError = QStringLiteral(
                    "The previous update could not be applied (step %1). "
                    "The previous installation is still available.").arg(code);
                m_error = m_lastInstallError;
                m_status = QStringLiteral("Previous update failed; retry available");
            }
        } else if (result == "success") {
            m_status = QStringLiteral("Update installed successfully");
        }
    }
#endif
    m_autoCheckEnabled = m_settings.value(QStringLiteral("updates/autoCheck"), true).toBool();
    m_periodic = new QTimer(this);
    m_periodic->setInterval(24 * 60 * 60 * 1000);
    connect(m_periodic, &QTimer::timeout, this, [this] {
        if (m_autoCheckEnabled) check(false);
    });
#if defined(__EMSCRIPTEN__)
    // Browser updates are discovered from the same-origin release manifest,
    // never from desktop GitHub installer releases.
    m_webCurrentBuild = QString::fromUtf8(emscripten_run_script_string(
        "(window.__genycasterAssets && window.__genycasterAssets.version) || ''"));
    if (startupCheck && m_autoCheckEnabled) {
        QTimer::singleShot(12'000, this, [this] { check(false); });
    }
#else
    m_periodic->start();
    if (startupCheck) {
        QTimer::singleShot(10'000, this, [this] {
            if (m_autoCheckEnabled) check(false);
        });
    }
#endif
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
    if (m_deltaProcess) {
        disconnect(m_deltaProcess, nullptr, this, nullptr);
        m_deltaProcess->kill();
        m_deltaProcess->waitForFinished(1000);
    }
#if defined(__EMSCRIPTEN__)
    if (pendingBrowserRefresh == this) pendingBrowserRefresh = nullptr;
#endif
    if (m_file) m_file->close();
    if (!m_partialPath.isEmpty()) QFile::remove(m_partialPath);
}

QString UpdateController::productName() const { return QString::fromLatin1(GENYCASTER_PRODUCT_NAME); }
QString UpdateController::appVersion() const { return m_appVersion; }
QString UpdateController::versionNumber() const { return QString::fromLatin1(GENYCASTER_VERSION_NUMBER); }
QString UpdateController::releaseChannel() const { return QString::fromLatin1(GENYCASTER_RELEASE_CHANNEL); }
int UpdateController::releaseChannelNumber() const { return QString::fromLatin1(GENYCASTER_RELEASE_CHANNEL_NUMBER).toInt(); }
QString UpdateController::releaseTag() const { return QStringLiteral("v") + m_appVersion; }
QString UpdateController::releaseSummary() const { return m_releaseSummary; }
QString UpdateController::webLatestBuild() const { return m_webLatestBuild; }
bool UpdateController::webUpdateChecked() const { return m_webUpdateChecked; }

void UpdateController::setLanguageTag(QString tag)
{
    tag = tag.trimmed().replace(QLatin1Char('_'), QLatin1Char('-'));
    if (tag.compare(m_languageTag, Qt::CaseInsensitive) == 0) return;
    m_languageTag = std::move(tag);
    applyReleaseTranslation();
}

void UpdateController::applyReleaseTranslation()
{
    if (m_releaseData.isEmpty()) return;
    const QJsonObject locales = m_releaseData.value(QStringLiteral("translations")).toObject();
    QJsonObject localized;
    for (auto it = locales.constBegin(); it != locales.constEnd(); ++it) {
        if (it.key().compare(m_languageTag, Qt::CaseInsensitive) == 0) {
            localized = it.value().toObject();
            break;
        }
    }
    if (localized.isEmpty()) {
        const QString language = m_languageTag.section(QLatin1Char('-'), 0, 0);
        for (auto it = locales.constBegin(); it != locales.constEnd(); ++it) {
            if (it.key().startsWith(language + QLatin1Char('-'), Qt::CaseInsensitive)) {
                localized = it.value().toObject();
                break;
            }
        }
    }
    if (localized.isEmpty()) localized = locales.value(QStringLiteral("en-US")).toObject();
    if (localized.isEmpty()) return;
    m_releaseTitle = localized.value(QStringLiteral("title")).toString().left(120);
    m_releaseSummary = localized.value(QStringLiteral("summary")).toString().left(220);
    const auto items = localized.value(QStringLiteral("items")).toObject();
    QStringList bullets;
    for (const auto& change : m_releaseData.value(QStringLiteral("changes")).toArray()) {
        const QString id = change.toObject().value(QStringLiteral("id")).toString();
        const QString item = items.value(id).toString();
        if (!item.isEmpty() && bullets.size() < 12)
            bullets.append(QStringLiteral("• ") + item.left(400));
    }
    m_releaseNotes = bullets.join(QLatin1Char('\n'));
    m_releaseType = m_releaseData.value(QStringLiteral("type")).toString();
    emit changed();
}
bool UpdateController::autoCheckEnabled() const { return m_autoCheckEnabled; }
bool UpdateController::checking() const { return m_checking; }
bool UpdateController::downloading() const { return m_downloading; }
bool UpdateController::updateAvailable() const { return m_updateAvailable; }
QString UpdateController::latestVersion() const { return m_latestVersion; }
QString UpdateController::releaseTitle() const { return m_releaseTitle; }
QString UpdateController::releaseNotes() const { return m_releaseNotes; }
QString UpdateController::releaseType() const { return m_releaseType; }
QString UpdateController::webCurrentBuild() const { return m_webCurrentBuild.left(8); }
QString UpdateController::releasePublishedAt() const { return m_releasePublishedAt; }
QString UpdateController::status() const { return m_status; }
QString UpdateController::error() const { return m_error; }
QString UpdateController::lastInstallError() const { return m_lastInstallError; }
QString UpdateController::downloadedFilePath() const { return m_downloadedFilePath; }
QString UpdateController::releaseUrl() const { return kReleasePage.toString(); }
bool UpdateController::webRefreshing() const { return m_webRefreshing; }
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

void UpdateController::refreshWebApp()
{
#if defined(__EMSCRIPTEN__)
    if (m_webRefreshing) return;
    m_webRefreshing = true;
    m_error.clear();
    m_status = QStringLiteral("Updating the WebAssembly application…");
    emit changed();

    // Do not direct WASM users to desktop installers or GitHub Releases.
    // Verify that the same-origin public release manifest is reachable
    // before requesting a service-worker update and a full navigation.
    const auto location = QString::fromUtf8(emscripten_run_script_string(
        "new URL('./genycaster-assets.json', window.location.href).href"));
    QUrl url{location};
    if (url.scheme() != QStringLiteral("https") || url.host().isEmpty()) {
        m_webRefreshing = false;
        fail(QStringLiteral("Web update is only supported on a secure browser origin"));
        return;
    }
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("refresh"), QString::number(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);
    QNetworkRequest request{url};
    request.setRawHeader("Cache-Control", "no-cache, no-store");
    request.setTransferTimeout(12'000);
    auto* reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->isReadable() ? reply->readAll() : QByteArray{};
        const bool networkOkay = reply->error() == QNetworkReply::NoError && status == 200;
        reply->deleteLater();
        const QJsonDocument manifest = QJsonDocument::fromJson(bytes);
        const QJsonObject metadata = manifest.object();
        if (!networkOkay || !manifest.isObject()
            || metadata.value(QStringLiteral("version")).toString().isEmpty()
            || !metadata.value(QStringLiteral("files")).isArray()) {
            m_webRefreshing = false;
            fail(QStringLiteral("Could not verify the online WebAssembly release. Please retry."));
            return;
        }
        // QSettings::sync updates the virtual INI file, not IndexedDB.
        // A user may press Refresh immediately after Use: wait for the
        // serialized durable flush before any service-worker/navigation work.
        m_settings.sync();
        if (m_settings.status() != QSettings::NoError) {
            browserRefreshPersistenceFailed();
            return;
        }
        pendingBrowserRefresh = this;
        // Only public browser assets are refreshed. Account credentials,
        // draft content and local signer state are not erased or re-keyed.
        EM_ASM({
            Module.genycasterSyncBrowserData().then(async function() {
                try {
                    const registration = await navigator.serviceWorker?.getRegistration('./');
                    if (registration) {
                        await Promise.race([
                            registration.update(),
                            new Promise(resolve => setTimeout(resolve, 2500))
                        ]);
                    }
                } catch (error) {
                    console.warn('[Genycaster] Service worker update unavailable', error);
                }
                const next = new URL(window.location.href);
                next.searchParams.set('genycaster_refresh', Date.now().toString());
                window.location.replace(next.href);
            }).catch(function(error) {
                console.error('[Genycaster] Browser data persistence prevented refresh', error);
                Module._genycasterWebRefreshSaveFailure();
            });
        });
    });
#else
    // Native platforms continue using verified installer updates.
#endif
}

void UpdateController::checkWebManifest(const bool manual)
{
#if defined(__EMSCRIPTEN__)
    if (m_checking || m_downloading || m_webRefreshing) return;
    m_checking = true;
    m_error.clear();
    m_status = QStringLiteral("Checking the WebAssembly release…");
    emit changed();

    const QString location = QString::fromUtf8(emscripten_run_script_string(
        "new URL('./genycaster-assets.json', window.location.href).href"));
    QUrl url{location};
    // Reject cross-origin manifests, even after redirects.
    const QUrl page{QString::fromUtf8(emscripten_run_script_string(
        "window.location.origin"))};
    if (url.scheme() != QStringLiteral("https") || url.scheme() != page.scheme()
        || url.host() != page.host() || url.port(443) != page.port(443)) {
        m_checking = false;
        fail(QStringLiteral("Invalid WebAssembly update origin"));
        return;
    }
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("check"), QString::number(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);
    QNetworkRequest request{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Cache-Control", "no-cache, no-store");
    request.setTransferTimeout(12'000);
    auto* reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual] {
        m_checking = false;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->isReadable() ? reply->readAll() : QByteArray{};
        const auto error = reply->error();
        reply->deleteLater();
        const QJsonDocument manifest = QJsonDocument::fromJson(data);
        const QJsonObject root = manifest.object();
        const QString fingerprint = root.value(QStringLiteral("version")).toString();
        const QString build = root.value(QStringLiteral("buildId")).toString();
        if (error != QNetworkReply::NoError || status != 200
            || data.size() > 100'000 || !manifest.isObject()
            || fingerprint.size() != 20 || build != fingerprint.left(8)
            || !root.value(QStringLiteral("files")).isArray()
            || !QRegularExpression{QStringLiteral(R"(^\d+\.\d+\.\d+(?:-(?:alpha|beta|rc)[1-9][0-9]*)?$)")}
                    .match(root.value(QStringLiteral("appVersion")).toString()).hasMatch()) {
            if (manual) fail(QStringLiteral("Could not verify WebAssembly release metadata"));
            else { m_status = QStringLiteral("Web update check unavailable"); emit changed(); }
            return;
        }
        const QJsonObject notes = root.value(QStringLiteral("releaseNotes")).toObject();
        m_releaseData = notes;
        m_releaseTitle.clear();
        m_releaseSummary.clear();
        m_releaseNotes.clear();
        m_releaseType.clear();
        applyReleaseTranslation();
        m_releasePublishedAt.clear(); // A build hash does not prove a publication date.
        m_webUpdateChecked = true;
        m_latestVersion = root.value(QStringLiteral("appVersion")).toString();
        m_webLatestBuild = build;
        m_updateAvailable = !m_webCurrentBuild.isEmpty() && fingerprint != m_webCurrentBuild;
        m_status = m_updateAvailable
            ? QStringLiteral("New WebAssembly build available")
            : QStringLiteral("WebAssembly build is up to date");
        m_error.clear();
        emit changed();
    });
#else
    Q_UNUSED(manual);
#endif
}

void UpdateController::check(const bool manual)
{
#if defined(__EMSCRIPTEN__)
    checkWebManifest(manual);
    return;
#endif
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
    const QByteArray body = reply->isReadable() ? reply->readAll() : QByteArray{};
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
    // The /releases/latest endpoint intentionally excludes release
    // candidates. Select the highest actual, published semantic release from
    // a bounded GitHub release list, including rcN while it is current.
    const auto doc = QJsonDocument::fromJson(body);
    if (!doc.isArray()) {
        fail(QStringLiteral("Invalid GitHub release list"));
        return;
    }
    const ReleaseVersion current = parseReleaseVersion(m_appVersion);
    if (!current.valid) {
        fail(QStringLiteral("Unrecognized installed release version"));
        return;
    }
    QJsonObject obj;
    ReleaseVersion parsed;
    QString version;
    for (const QJsonValue& item : doc.array()) {
        if (!item.isObject()) continue;
        const QJsonObject candidate = item.toObject();
        if (candidate.value(QStringLiteral("draft")).toBool()) continue;
        const QString name = normalizeVersion(
            candidate.value(QStringLiteral("tag_name")).toString());
        const ReleaseVersion candidateVersion = parseReleaseVersion(name);
        if (!candidateVersion.valid) continue;
        if (!parsed.valid || compareReleaseVersions(candidateVersion, parsed) > 0) {
            obj = candidate;
            parsed = candidateVersion;
            version = name;
        }
    }
    if (!parsed.valid) {
        m_updateAvailable = false;
        m_status = QStringLiteral("No published release yet");
        m_error.clear();
        emit changed();
        return;
    }
    m_latestVersion = version;
    // Display only the selected GitHub release's actual title, date and
    // body. Do not invent a changelog based on a version-number comparison.
    m_releaseData = parsePublishedNotes(obj.value(QStringLiteral("body")).toString());
    if (m_releaseData.isEmpty()) {
        // Older releases without locale metadata remain readable in English.
        m_releaseTitle = obj.value(QStringLiteral("name")).toString().trimmed().left(120);
        m_releaseNotes = obj.value(QStringLiteral("body")).toString().trimmed().left(1600);
        m_releaseSummary.clear();
        m_releaseType.clear();
    } else {
        applyReleaseTranslation();
    }
    m_releasePublishedAt = obj.value(QStringLiteral("published_at")).toString().left(10);
    m_updateAvailable = compareReleaseVersions(parsed, current) > 0;
    m_assetUrl.clear();
    m_assetName.clear();
    m_assetDigest.clear();
    m_downloadedFilePath.clear();
    if (m_updateAvailable) {
        const auto assets = obj.value(QStringLiteral("assets")).toArray();
        int preferredRank = -1;
        for (const auto& value : assets) {
            const auto asset = value.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            const QUrl url{asset.value(QStringLiteral("browser_download_url")).toString()};
            if (!matchingAsset(name) || !isOfficialAssetUrl(url)) continue;
            QString digest = asset.value(QStringLiteral("digest")).toString().toLower();
            if (digest.startsWith(QStringLiteral("sha256:"))) digest.remove(0, 7);
            if (!safeDigest(digest)) continue; // fail closed: no unverified installer
            const int rank = assetPreference(name);
            if (rank <= preferredRank) continue;
            preferredRank = rank;
            m_assetName = name;
            m_assetUrl = url.toString();
            m_assetDigest = digest;
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
    m_received = 0;
    m_total = 0;
    m_downloading = true;
    m_error.clear();
    m_status = QStringLiteral("Downloading verified package…");
    emit changed();

#if defined(Q_OS_LINUX) && !defined(__EMSCRIPTEN__)
    // A current AppImage is a valid block seed for zsync. Bundle the native
    // architecture-matching zsync executable with the AppImage, and never
    // execute a random PATH-resolved helper from untrusted directories.
    const QString currentImage = qEnvironmentVariable("APPIMAGE");
    const QFileInfo original{currentImage};
    const QString tool = QDir{QCoreApplication::applicationDirPath()}.filePath(
        QStringLiteral("zsync"));
    if (m_assetName.endsWith(QStringLiteral(".AppImage"), Qt::CaseSensitive)
        && original.isAbsolute() && original.isFile() && !original.isSymLink()
        && QFileInfo{tool}.isExecutable()) {
        auto* const delta = new QProcess(this);
        m_deltaProcess = delta;
        delta->setWorkingDirectory(dir);
        delta->setProgram(tool);
        delta->setArguments({
            QStringLiteral("-i"), original.absoluteFilePath(),
            QStringLiteral("-o"), m_partialPath,
            m_assetUrl + QStringLiteral(".zsync")});
        delta->setProcessChannelMode(QProcess::MergedChannels);
        m_status = QStringLiteral("Downloading changed AppImage blocks…");
        emit changed();

        const auto fallBack = [this, delta] {
            if (m_deltaProcess != delta) return;
            m_deltaProcess = nullptr;
            delta->deleteLater();
            QFile::remove(m_partialPath);
            QFile::remove(m_partialPath + QStringLiteral(".part"));
            m_received = 0;
            m_total = 0;
            m_status = QStringLiteral("Delta unavailable; downloading full update…");
            emit changed();
            startFullDownload();
        };
        connect(delta, &QProcess::errorOccurred, this,
                [fallBack](const QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) fallBack();
        });
        connect(delta, &QProcess::readyReadStandardOutput, this, [this, delta] {
            const QByteArray output = delta->readAllStandardOutput().right(4096);
            const QRegularExpression pattern{QStringLiteral(R"((\d{1,3})%)")};
            auto matches = pattern.globalMatch(QString::fromUtf8(output));
            while (matches.hasNext()) {
                const auto match = matches.next();
                const int percent = match.captured(1).toInt();
                if (percent >= 0 && percent <= 100) {
                    m_received = percent;
                    m_total = 100;
                }
            }
            emit changed();
        });
        connect(delta, &QProcess::finished, this,
                [this, delta, fallBack](int code, QProcess::ExitStatus status) {
            if (m_deltaProcess != delta) return;
            if (code != 0 || status != QProcess::NormalExit) {
                fallBack();
                return;
            }
            QFile output{m_partialPath};
            QCryptographicHash digest{QCryptographicHash::Sha256};
            const bool verified = output.open(QIODevice::ReadOnly)
                && digest.addData(&output)
                && QString::fromLatin1(digest.result().toHex()) == m_assetDigest;
            output.close();
            if (!verified) {
                fallBack(); // never trust unverified differential output
                return;
            }
            m_deltaProcess = nullptr;
            delta->deleteLater();
            m_downloading = false;
            finalizeVerifiedPackage();
        });
        delta->start();
        return;
    }
#endif
    startFullDownload();
}

void UpdateController::startFullDownload()
{
    m_file = std::make_unique<QFile>(m_partialPath);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_file.reset();
        m_downloading = false;
        fail(QStringLiteral("Could not open update destination"));
        return;
    }
    m_hash.reset();
    m_received = 0;
    m_total = 0;
    QNetworkRequest req{QUrl{m_assetUrl}};
    req.setRawHeader("User-Agent", "Genycaster-Updater/1.0");
    req.setTransferTimeout(90'000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = m_network.get(req);
    m_downloadReply = reply;
    connect(reply, &QIODevice::readyRead, this, [this, reply] {
        if (!m_file || !m_file->isOpen()) return;
        const auto bytes = reply->isReadable() ? reply->readAll() : QByteArray{};
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
        const auto rest = reply->isReadable() ? reply->readAll() : QByteArray{};
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
    finalizeVerifiedPackage();
}

bool UpdateController::finalizeVerifiedPackage()
{
    const QString finalPath = m_partialPath.left(m_partialPath.size() - 8);
    if (QFile::exists(finalPath) && !QFile::remove(finalPath)) {
        QFile::remove(m_partialPath);
        m_partialPath.clear();
        fail(QStringLiteral("Cannot replace previous update package"));
        return false;
    }
    if (!QFile::rename(m_partialPath, finalPath)) {
        QFile::remove(m_partialPath);
        m_partialPath.clear();
        fail(QStringLiteral("Cannot finalize verified update"));
        return false;
    }
    m_partialPath.clear();
    m_downloadedFilePath = finalPath;
    m_status = QStringLiteral("Package verified; ready to install update");
    emit changed();
    return true;
}

bool UpdateController::openDownloadedUpdate()
{
    return installDownloadedUpdate();
}

bool UpdateController::installDownloadedUpdate()
{
    // Keep release selection and installation independent of the current
    // filesystem cache. A local package can be altered after download.
    const QFileInfo info{m_downloadedFilePath};
    if (!downloadSupported() || !m_updateAvailable ||
        m_downloadedFilePath.isEmpty() || !info.isFile() || info.isSymLink() ||
        info.fileName() != m_assetName || !safeDigest(m_assetDigest) ||
        !isOfficialAssetUrl(QUrl{m_assetUrl})) {
        fail(QStringLiteral("No verified installer is ready"));
        return false;
    }
    QFile installer{m_downloadedFilePath};
    QCryptographicHash checksum{QCryptographicHash::Sha256};
    if (!installer.open(QIODevice::ReadOnly) || !checksum.addData(&installer) ||
        QString::fromLatin1(checksum.result().toHex()) != m_assetDigest) {
        m_downloadedFilePath.clear();
        fail(QStringLiteral("Cached update failed SHA-256 verification; download again"));
        return false;
    }
    installer.close();

    // A tiny first-party post-exit helper handles writable macOS app bundles
    // and portable Linux AppImages. Other platforms hand off to the system
    // installer, which owns the user consent / elevation boundary.
    const auto launched = genycaster::ui::update::launchVerifiedUpdate(
        m_downloadedFilePath, m_assetDigest);
    if (launched.mode == genycaster::ui::update::InstallMode::Unsupported) {
        fail(launched.detail.isEmpty() ? QStringLiteral("Update installer unavailable")
                                      : launched.detail);
        return false;
    }

    const bool quitForUpdate =
        launched.mode == genycaster::ui::update::InstallMode::Supervised
#if defined(Q_OS_WIN)
        || launched.mode == genycaster::ui::update::InstallMode::SystemInstaller
#endif
        ;
    m_status = quitForUpdate
        ? QStringLiteral("Closing GenyCaster to complete the verified update…")
        : QStringLiteral("Update transferred to the system installer");
    m_error.clear();
    emit changed();
    if (quitForUpdate) {
        // Allow vault/session controllers to shut down on the existing
        // application exit path. Never delete files from a running process.
        QTimer::singleShot(150, QCoreApplication::instance(), [] {
            QCoreApplication::quit();
        });
    }
    return true;
}

bool UpdateController::openReleasePage()
{
    return QDesktopServices::openUrl(kReleasePage);
}
