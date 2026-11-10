// Diagnostic-only snapshot behavior, kept out of application bootstrap.
module;

#include "presentation_bridge.hpp"
#include "rich_composer_controller.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QMetaObject>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>

module genycaster.ui.diagnostics.snapshots;

import genycaster.ui.bootstrap;

namespace genycaster::ui::diagnostics {
using genycaster::ui::bootstrap::optionValue;

int scheduleSnapshots(QGuiApplication& application,
                      QQmlApplicationEngine& engine,
                      PresentationBridge& presentationBridge,
                      RichComposerController& richComposerController,
                      const QStringList& arguments,
                      bool isVisualTest,
                      bool openExploreForTest,
                      const QString& snapshotPath)
{
    // Exercise the real shared Qt Drawer and Pro QML with a running event
    // loop, rather than merely looking for component names in source text.
    // Run only in the local --visual-test fixture; never modify user state.
    if (isVisualTest && arguments.contains(QStringLiteral("--exercise-pro-drawer"))) {
        auto* const window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        auto* const drawer = window ? window->findChild<QObject*>(
            QStringLiteral("genyProDetailsDrawer")) : nullptr;
        if (!window || !drawer) return 11;
        auto* const cycle = new QTimer{&application};
        cycle->setInterval(550); // Allow 275ms entrance/exit animations to finish.
        QObject::connect(cycle, &QTimer::timeout, &application,
            [cycle, window, drawer, phase = 0]() mutable {
                if (phase > 0 && drawer->property("opened").toBool()
                    != ((phase - 1) % 2 == 0)) {
                    qWarning() << "Pro drawer did not finish transition at phase" << phase;
                    cycle->stop();
                    QCoreApplication::exit(12);
                    return;
                }
                if (phase == 12) {
                    qInfo() << "Pro drawer: six open/close cycles passed";
                    cycle->stop();
                    QCoreApplication::exit(0);
                    return;
                }
                if (phase % 2 == 0) {
                    if (drawer->property("height").toReal() < window->height() - 2) {
                        qWarning() << "Pro drawer does not span desktop height";
                        cycle->stop();
                        QCoreApplication::exit(13);
                        return;
                    }
                }
                const char* action = phase % 2 == 0 ? "open" : "close";
                if (!QMetaObject::invokeMethod(drawer, action, Qt::DirectConnection)) {
                    qWarning() << "Pro drawer cannot invoke" << action;
                    cycle->stop();
                    QCoreApplication::exit(14);
                    return;
                }
                ++phase;
            });
        QTimer::singleShot(600, cycle, [cycle] { cycle->start(); });
    }
    if (isVisualTest && !snapshotPath.isEmpty()) {
        auto* const window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        if (window == nullptr) { return 3; }
        for (const QString& argument : arguments) {
            if (argument.startsWith(QStringLiteral("--width="))) {
                window->setWidth(argument.sliced(8).toInt());
            } else if (argument.startsWith(QStringLiteral("--height="))) {
                window->setHeight(argument.sliced(9).toInt());
            }
        }
        if (arguments.contains(QStringLiteral("--open-settings"))) {
            if (auto* const popup = window->findChild<QObject*>(QStringLiteral("settingsPopup"))) {
                QMetaObject::invokeMethod(popup, "open", Qt::DirectConnection);
            }
        }
        if (openExploreForTest) {
            presentationBridge.selectFeed(QStringLiteral("recent"));
        }
        if (arguments.contains(QStringLiteral("--open-composer"))) {
            if (auto* const popup = window->findChild<QObject*>(QStringLiteral("composerDrawer"))) {
                QMetaObject::invokeMethod(popup, "open", Qt::DirectConnection);
            }
        }
        if (arguments.contains(QStringLiteral("--open-reply-composer"))) {
            if (auto* const popup = window->findChild<QObject*>(QStringLiteral("composerDrawer"))) {
                popup->setProperty("parentCast", QVariantMap{
                    {QStringLiteral("castId"), QStringLiteral("visual-reply-target")},
                    {QStringLiteral("author"), QStringLiteral("kmacb.eth")},
                    {QStringLiteral("displayName"), QStringLiteral("KMac")},
                    {QStringLiteral("avatarUrl"), QStringLiteral(
                        "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/farcaster.svg")},
                    {QStringLiteral("text"), QStringLiteral(
                        "Fair question — here is the original cast with enough context to reply confidently.")},
                });
                QMetaObject::invokeMethod(popup, "open", Qt::DirectConnection);
            }
        }
        if (arguments.contains(QStringLiteral("--open-create"))) {
            if (auto* const popup = window->findChild<QObject*>(QStringLiteral("mobileCreateMenu"))) {
                QMetaObject::invokeMethod(popup, "open", Qt::DirectConnection);
            }
        }
        if (arguments.contains(QStringLiteral("--open-gallery"))) {
            if (auto* const gallery = window->findChild<QObject*>(QStringLiteral("mediaGallery"))) {
                gallery->setProperty("items", QVariantList{
                    QVariantMap{{QStringLiteral("imageSource"), QStringLiteral(
                        "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/genyleap.svg")}},
                    QVariantMap{{QStringLiteral("imageSource"), QStringLiteral(
                        "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/farcaster.svg")}},
                    QVariantMap{{QStringLiteral("imageSource"), QStringLiteral(
                        "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/quilibrium/quilibrium.svg")}},
                });
                gallery->setProperty("requestedIndex", 0);
                QMetaObject::invokeMethod(gallery, "open", Qt::DirectConnection);
            }
        }
        if (arguments.contains(QStringLiteral("--exercise-code-block-lifetime"))) {
            QTimer::singleShot(180, &application, [&richComposerController] {
                richComposerController.insertBlock(QStringLiteral("code"));
            });
            QTimer::singleShot(320, &application, [&richComposerController] {
                richComposerController.setCodeLanguage(1, QStringLiteral("cpp"));
            });
            QTimer::singleShot(460, &application, [&richComposerController] {
                richComposerController.removeBlock(1);
            });
        }
        bool snapshotDelayValid = false;
        const int requestedSnapshotDelay = optionValue(
            arguments, QStringLiteral("--snapshot-delay-ms=")).toInt(&snapshotDelayValid);
        const int snapshotDelay = snapshotDelayValid
            ? std::clamp(requestedSnapshotDelay, 100, 30'000) : 600;
        QTimer::singleShot(snapshotDelay, &application, [window, snapshotPath] {
            const bool saved = window->grabWindow().save(snapshotPath);
            QCoreApplication::exit(saved ? 0 : 4);
        });
    }
    const QString snapshotLivePath = optionValue(arguments, QStringLiteral("--snapshot-live="));
    if (!snapshotLivePath.isEmpty()) {
        // Grab a screenshot of the real running app (live data, not the fixture)
        // once the demo feed has rendered. Used to visually verify real content.
        if (auto* const window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst())) {
            window->resize(1280, 900);
        }
        bool liveSnapshotDelayValid = false;
        const int requestedLiveSnapshotDelay = optionValue(
            arguments, QStringLiteral("--snapshot-delay-ms=")).toInt(&liveSnapshotDelayValid);
        const int liveSnapshotDelay = liveSnapshotDelayValid
            ? std::clamp(requestedLiveSnapshotDelay, 100, 30'000) : 5'000;
        QTimer::singleShot(liveSnapshotDelay, &application, [&engine, snapshotLivePath] {
            auto* const window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
            if (window == nullptr) {
                QCoreApplication::exit(3);
                return;
            }
            const bool saved = window->grabWindow().save(snapshotLivePath);
            QCoreApplication::exit(saved ? 0 : 4);
        });
    }
    return 0;
}

} // namespace genycaster::ui::diagnostics
