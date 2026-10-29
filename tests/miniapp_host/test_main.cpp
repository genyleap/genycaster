#include "../../ui/mobile/miniapp_host_controller.hpp"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QStringList>

#include <cassert>

int main(int argc, char* argv[])
{
    QCoreApplication application{argc, argv};
    QTemporaryDir temporary{};
    if (!temporary.isValid()) return 1;
    QSettings settings{temporary.filePath(QStringLiteral("miniapps.ini")), QSettings::IniFormat};
    MiniAppHostController controller{false, settings};
    int internalLaunches{};
    int externalLaunches{};
    QObject::connect(&controller, &MiniAppHostController::launchRequested,
                     &application, [&internalLaunches] { ++internalLaunches; });
    QObject::connect(&controller, &MiniAppHostController::externalLaunchSucceeded,
                     &application, [&externalLaunches] { ++externalLaunches; });

    const QStringList rejected{
        QStringLiteral("http://miniapp.example.com"),
        QStringLiteral("file:///etc/passwd"),
        QStringLiteral("content://com.example/secret"),
        QStringLiteral("javascript:alert(1)"),
        QStringLiteral("https://localhost/"),
        QStringLiteral("https://account.localhost/"),
        QStringLiteral("https://admin.internal/"),
        QStringLiteral("https://node.local/"),
        QStringLiteral("https://dev.test/"),
        QStringLiteral("https://0.0.0.0/"),
        QStringLiteral("https://127.0.0.1/"),
        QStringLiteral("https://[::1]/"),
        QStringLiteral("https://user:pass@app.example/"),
        QStringLiteral("https://app.example:8443/"),
        QStringLiteral("https://app.example./"),
    };
    for (const QString& url : rejected) {
        controller.openWithMetadata(url, QStringLiteral("Untrusted"), QString{});
        if (controller.active() || controller.errorText().isEmpty()
            || !controller.launchUrl().isEmpty()
            || internalLaunches != 0 || externalLaunches != 0) {
            return 2;
        }
    }
    // A valid external HTTPS origin is still not a sign-in credential.
    // Without an authenticated FID, the controller must not open a browser.
    controller.openWithMetadata(
        QStringLiteral("https://app.example.com/path"),
        QStringLiteral("Public"), QString{});
    if (controller.active() || controller.errorText().isEmpty()
        || internalLaunches != 0 || externalLaunches != 0) {
        return 3;
    }
    return 0;
}
