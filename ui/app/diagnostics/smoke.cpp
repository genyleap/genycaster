// Internal diagnostics run on the GUI thread, after QML root creation.
module;

#include "presentation_bridge.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QMetaObject>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QString>
#include <QStringList>
#include <QTimer>

module genycaster.ui.diagnostics.smoke;

namespace genycaster::ui::diagnostics {
int configureSmokeTests(QGuiApplication& application,
                        QQmlApplicationEngine& engine,
                        PresentationBridge& presentationBridge,
                        bool isSmokeTest,
                        bool isRuntimeSmokeTest)
{
    if (isSmokeTest) {
        QObject* const rootObject = engine.rootObjects().constFirst();
        bool ready = rootObject != nullptr
            && rootObject->objectName() == QStringLiteral("rootWindow")
            && rootObject->property("isReady").toBool()
            && rootObject->findChild<QObject*>(QStringLiteral("appShell")) != nullptr
            // A constructed shell can still display nothing if account
            // restoration has never reached its settled/observed state.
            && rootObject->findChild<QObject*>(QStringLiteral("shellContent")) != nullptr
            && rootObject->findChild<QObject*>(QStringLiteral("shellContent"))
                   ->property("visible").toBool()
            // Home is the initial surface; the costly feed ListView is
            // deliberately Loader-backed and may not exist until Explorer
            // navigation. Treat either an instantiated feed or Home as a
            // valid startup surface, not an eager-all-views contract.
            && (rootObject->findChild<QObject*>(QStringLiteral("readList")) != nullptr
                || rootObject->findChild<QObject*>(QStringLiteral("homeDashboard")) != nullptr);
        // Keep the smoke contract aligned with the actual SideNavigation
        // destinations. Legacy Store/Business/Messages are consolidated into
        // Apps; testing removed buttons masked real startup regressions.
        const QStringList sidebarActions{
            QStringLiteral("trending"),
            QStringLiteral("following"),
            QStringLiteral("apps"),
            QStringLiteral("channels"),
            QStringLiteral("notifications"),
            QStringLiteral("bookmarks"),
            QStringLiteral("profile"),
            QStringLiteral("settings"),
        };
        if (!ready) {
            qInfo().noquote() << "Smoke: root shell or feed readiness failed:"
                              << (rootObject == nullptr ? QStringLiteral("<null>")
                                  : rootObject->objectName());
        }
        for (const QString& action : sidebarActions) {
            QObject* const button = rootObject == nullptr ? nullptr
                : rootObject->findChild<QObject*>(QStringLiteral("sidebarAction-") + action);
            const bool actionReady = button != nullptr && button->property("enabled").toBool()
                && QMetaObject::invokeMethod(button, "clicked", Qt::DirectConnection);
            if (!actionReady) {
                qInfo().noquote() << "Smoke: sidebar action unavailable:" << action;
            }
            ready = actionReady && ready;
        }
        QObject* const emojiPicker = rootObject == nullptr ? nullptr
            : rootObject->findChild<QObject*>(QStringLiteral("emojiPicker"));
        if (!emojiPicker) {
            qInfo() << "Smoke: emoji picker unavailable";
        }
        ready = ready && emojiPicker != nullptr;
        if (!ready) { return 2; }
        QMetaObject::invokeMethod(&application, [] { QCoreApplication::exit(0); }, Qt::QueuedConnection);
    }
    if (isRuntimeSmokeTest) {
        auto* const poller = new QTimer(&application);
        poller->setInterval(25);
        QObject::connect(poller, &QTimer::timeout, &application,
            [&presentationBridge] {
                if (!presentationBridge.runtimeReady()) { return; }
                if (!presentationBridge.errorText().isEmpty()) {
                    qWarning().noquote() << "Read diagnostic:" << presentationBridge.diagnosticText();
                    QCoreApplication::exit(5);
                } else if (!presentationBridge.rows().isEmpty() && !presentationBridge.isBusy()) {
                    QCoreApplication::exit(0);
                }
            });
        poller->start();
        QTimer::singleShot(10'000, &application, [] { QCoreApplication::exit(6); });
    }
    return 0;
}
} // namespace genycaster::ui::diagnostics
