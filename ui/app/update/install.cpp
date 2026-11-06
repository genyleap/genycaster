module;

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

module genycaster.ui.update.install;

namespace genycaster::ui::update {
namespace {

[[nodiscard]] bool launchSystemInstaller(const QString& package)
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(package));
}

#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
[[nodiscard]] bool launchSupervisedScript(
    const QString& resource, const QString& package, const QString& target,
    const QString& digest)
{
    const QString updates = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation) + QStringLiteral("/updates");
    if (!QDir{}.mkpath(updates)) return false;

    QFile bundled{resource};
    if (!bundled.open(QIODevice::ReadOnly)) return false;
    const QByteArray script = bundled.readAll();
    if (script.isEmpty() || script.size() > 16 * 1024) return false;

    // Script contents come only from the compiled Qt resource, never from
    // GitHub metadata or a release archive. QSaveFile avoids partial writes.
    const QString helper = QDir{updates}.filePath(
        QStringLiteral("genycaster-install-helper.sh"));
    QSaveFile output{helper};
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || output.write(script) != script.size() || !output.commit()) {
        return false;
    }
    QFile::setPermissions(helper, QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    // The helper is an independent /bin/sh process. It waits for GenyCaster
    // to exit before replacing a program, and verifies the digest again.
    // Pass all paths as argv (never via shell command string interpolation).
    return QProcess::startDetached(QStringLiteral("/bin/sh"),
        {helper,
         QString::number(QCoreApplication::applicationPid()),
         package, target, digest},
        updates);
}
#endif

} // namespace

InstallMode launchVerifiedUpdate(
    const QString& verifiedPackage, const QString& expectedSha256,
    QString* errorDetail)
{
    const auto fail = [errorDetail](QString error) {
        if (errorDetail) *errorDetail = std::move(error);
        return InstallMode::Unsupported;
    };
    const QFileInfo package{verifiedPackage};
    if (!package.isAbsolute() || !package.isFile() || package.isSymLink()) {
        return fail(QStringLiteral("Invalid update package path"));
    }

#if defined(Q_OS_WIN)
    if (!package.fileName().endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        return fail(QStringLiteral("Windows requires the NSIS installer"));
    }
    return launchSystemInstaller(verifiedPackage)
        ? InstallMode::SystemInstaller
        : fail(QStringLiteral("Could not launch Windows installer")};
#elif defined(Q_OS_MACOS)
    if (!package.fileName().endsWith(QStringLiteral(".dmg"), Qt::CaseInsensitive)) {
        return fail(QStringLiteral("macOS requires a DMG package"));
    }
    const QString target = QDir::cleanPath(
        QCoreApplication::applicationDirPath() + QStringLiteral("/../.."));
    const QFileInfo current{target};
    const QFileInfo folder{current.absolutePath()};
    // A signed app in a user-writable installation can be safely replaced
    // after quit. /Applications usually requires authorization: let Finder
    // handle the update instead of silently weakening filesystem controls.
    const bool replaceable = current.fileName() == QStringLiteral("Genycaster.app")
        && current.isDir() && !current.isSymLink() && folder.isWritable()
        && QProcess::execute(QStringLiteral("/usr/bin/codesign"),
            {QStringLiteral("--verify"), QStringLiteral("--deep"),
             QStringLiteral("--strict"), target}) == 0;
    if (replaceable && launchSupervisedScript(
        QStringLiteral(":/genycaster/update/macos.sh"),
        verifiedPackage, target, expectedSha256)) {
        return InstallMode::Supervised;
    }
    return launchSystemInstaller(verifiedPackage)
        ? InstallMode::SystemInstaller
        : fail(QStringLiteral("Could not open macOS DMG")};
#elif defined(Q_OS_LINUX) && !defined(__EMSCRIPTEN__)
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    const QFileInfo current{appImage};
    const QFileInfo folder{current.absolutePath()};
    if (package.fileName().endsWith(QStringLiteral(".AppImage"), Qt::CaseSensitive)
        && current.isAbsolute() && current.isFile() && !current.isSymLink()
        && current.isWritable() && folder.isWritable()
        && launchSupervisedScript(
            QStringLiteral(":/genycaster/update/appimage.sh"),
            verifiedPackage, current.absoluteFilePath(), expectedSha256)) {
        return InstallMode::Supervised;
    }
    // DEB/NixOS/system-managed applications must be updated by their package
    // manager. Never overwrite files under /usr or mutate the Nix store.
    return launchSystemInstaller(verifiedPackage)
        ? InstallMode::SystemInstaller
        : fail(QStringLiteral("No compatible Linux package installer")};
#elif defined(Q_OS_ANDROID)
    // Qt 6.12 uses Android FileProvider for local file URLs. Android's
    // Package Installer enforces certificate matching and user consent.
    if (!package.fileName().endsWith(QStringLiteral(".apk"), Qt::CaseInsensitive)) {
        return fail(QStringLiteral("Android requires an APK"));
    }
    return launchSystemInstaller(verifiedPackage)
        ? InstallMode::SystemInstaller
        : fail(QStringLiteral("Could not invoke Android Package Installer")};
#else
    Q_UNUSED(expectedSha256);
    return fail(QStringLiteral("Use the system's approved update channel"));
#endif
}
} // namespace genycaster::ui::update
