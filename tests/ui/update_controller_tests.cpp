#include "update_controller.hpp"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

#include <cassert>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QStringLiteral("0.4.0"));
    QTemporaryDir directory;
    assert(directory.isValid());
    QSettings settings{directory.filePath(QStringLiteral("updates.ini")), QSettings::IniFormat};
    {
        UpdateController updates{settings, false};
        assert(updates.appVersion() == QStringLiteral("0.4.0"));
        assert(updates.autoCheckEnabled());
        assert(!updates.checking());
        assert(!updates.updateAvailable());
        assert(!updates.canDownload());
        assert(!updates.openDownloadedUpdate());
        assert(updates.releaseUrl().startsWith(
            QStringLiteral("https://github.com/genyleap/genycaster/releases")));
        updates.setAutoCheckEnabled(false);
        assert(!updates.autoCheckEnabled());
    }
    assert(!settings.value(QStringLiteral("updates/autoCheck"), true).toBool());
    {
        UpdateController updates{settings, false};
        assert(!updates.autoCheckEnabled());
        updates.setAutoCheckEnabled(true);
        assert(updates.autoCheckEnabled());
    }
    return 0;
}
