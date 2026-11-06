#include "../../ui/app/miniapp_host_controller.hpp"

#include <QCoreApplication>
#include <QByteArray>
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
    // Pro allows up to four isolated native documents; invalid or oversized
    // grants must not bypass the bounded resource ceiling.
    assert(controller.sessionLimit() == 1);
    controller.setSessionLimit(8);
    assert(controller.sessionLimit() == 4);
    controller.setSessionLimit(0);
    assert(controller.sessionLimit() == 1);

    // A minimized Mini App has active() == false, but its browser/wallet
    // document is still alive. Switching FIDs must revoke that session.
    int closeAllRequests{};
    QObject::connect(&controller, &MiniAppHostController::closeAllRequested,
                     &application, [&closeAllRequests] { ++closeAllRequests; });
    controller.setUser({{QStringLiteral("fid"), QVariant::fromValue<qulonglong>(42)}});
    closeAllRequests = 0;
    controller.setState(false);
    controller.setUser({{QStringLiteral("fid"), QVariant::fromValue<qulonglong>(43)}});
    assert(closeAllRequests == 1);
    controller.setUser({{QStringLiteral("fid"), QVariant::fromValue<qulonglong>(43)}});
    assert(closeAllRequests == 1);
    controller.setUser({});
    assert(closeAllRequests == 2);

    // The wallet picker owns only the external approval prompt. Canceling
    // must reject that wallet RPC, not close the native Mini App.
    int walletCancels{};
    int miniAppCloseRequests{};
    QObject::connect(&controller, &MiniAppHostController::walletBrowserCancelRequested,
                     &application, [&walletCancels] { ++walletCancels; });
    QObject::connect(&controller, &MiniAppHostController::closeRequested,
                     &application, [&miniAppCloseRequests] { ++miniAppCloseRequests; });
    const QString localSession = QStringLiteral("http://127.0.0.1:64141/session/")
        + QString(64, QLatin1Char('a'));
    controller.requestWalletBrowser(localSession);
    assert(!controller.walletBrowserPickerActive()); // No current trusted app context.
    controller.setWalletBrowserContext(
        QStringLiteral("Example Mini App"), QStringLiteral("https://example.com"));
    controller.requestWalletBrowser(QStringLiteral("https://evil.example/session/")
                                    + QString(64, QLatin1Char('a')));
    assert(!controller.walletBrowserPickerActive());
    controller.requestWalletBrowser(localSession);
    assert(controller.walletBrowserPickerActive());
    assert(!controller.walletBrowserWaiting());
    assert(controller.walletBrowserAppName() == QStringLiteral("Example Mini App"));
    assert(controller.walletBrowserOrigin() == QStringLiteral("https://example.com"));
    assert(!controller.walletBrowserOptions().isEmpty());
    controller.cancelWalletBrowser();
    assert(!controller.walletBrowserPickerActive());
    assert(walletCancels == 1);
    assert(miniAppCloseRequests == 0);
    controller.requestWalletBrowser(localSession);
    assert(controller.walletBrowserPickerActive());
    controller.walletBrowserSessionEnded();
    assert(!controller.walletBrowserPickerActive());
    assert(!controller.walletBrowserWaiting());
    controller.requestWalletBrowser(localSession);
    assert(!controller.walletBrowserPickerActive()); // Closed session cannot reopen picker.
    assert(miniAppCloseRequests == 0);

    // The Farcaster SDK requires an absolute public HTTPS pfpUrl. Qt's
    // image://remote alias is valid in GenyCaster's scene graph but not in
    // untrusted WKWebView HTML; decode it before constructing SDK context.
    const QString publicAvatar = QStringLiteral("https://images.example.com/pfp/me.png");
    const QString qtAvatar = QStringLiteral("image://remote/")
        + QString::fromLatin1(publicAvatar.toUtf8().toBase64(
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    controller.setUser({
        {QStringLiteral("fid"), QVariant::fromValue<qulonglong>(44)},
        {QStringLiteral("username"), QStringLiteral("example.eth")},
        {QStringLiteral("pfpUrl"), qtAvatar}
    });
    assert(controller.user().value(QStringLiteral("pfpUrl")).toString() == publicAvatar);
    controller.setUser({
        {QStringLiteral("fid"), QVariant::fromValue<qulonglong>(44)},
        {QStringLiteral("username"), QStringLiteral("example.eth")},
        {QStringLiteral("pfpUrl"), QStringLiteral("image://remote/v22/profile/auto/")
            + QString::fromLatin1(publicAvatar.toUtf8().toBase64(
                QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals))}
    });
    assert(controller.user().value(QStringLiteral("pfpUrl")).toString() == publicAvatar);
    const int userSwitchClosures = closeAllRequests;
    controller.setUser({
        {QStringLiteral("fid"), QVariant::fromValue<qulonglong>(44)},
        {QStringLiteral("username"), QStringLiteral("example.eth")},
        {QStringLiteral("pfpUrl"), QStringLiteral("https://images.example.com/new.png")}
    });
    assert(closeAllRequests == userSwitchClosures);
    assert(controller.user().value(QStringLiteral("pfpUrl")).toString()
           == QStringLiteral("https://images.example.com/new.png"));

    // No untrusted Mini App receives a local, credentialed, or network-internal
    // image location even if a source profile contains a malformed URL.
    const QStringList unsafeAvatars{
        QStringLiteral("file:///tmp/private.png"),
        QStringLiteral("data:image/png;base64,AAAA"),
        QStringLiteral("http://images.example.com/pfp.png"),
        QStringLiteral("https://127.0.0.1/private.png"),
        QStringLiteral("https://admin.internal/secret.png"),
        QStringLiteral("https://user:pass@images.example.com/private.png"),
        QStringLiteral("https://images.example.com:8443/private.png")
    };
    for (const QString& candidate : unsafeAvatars) {
        controller.setUser({
            {QStringLiteral("fid"), QVariant::fromValue<qulonglong>(44)},
            {QStringLiteral("pfpUrl"), candidate}
        });
        assert(!controller.user().contains(QStringLiteral("pfpUrl")));
        assert(closeAllRequests == userSwitchClosures);
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
