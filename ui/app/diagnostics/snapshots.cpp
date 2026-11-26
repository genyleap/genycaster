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
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

module genycaster.ui.diagnostics.snapshots;

import genycaster.ui.bootstrap;

namespace genycaster::ui::diagnostics {
using genycaster::ui::bootstrap::optionValue;

namespace {

int exerciseSetupLayout(QGuiApplication& application, QQmlApplicationEngine& engine,
                        QQuickWindow& window)
{
    auto* const setup = window.findChild<QQuickItem*>(QStringLiteral("initialSetup"));
    auto* const brand = window.findChild<QQuickItem*>(QStringLiteral("setupBrandPane"));
    auto* const stage = window.findChild<QQuickItem*>(QStringLiteral("setupStagePanel"));
    auto* const action = window.findChild<QQuickItem*>(QStringLiteral("setupStageAction"));
    auto* const localization = engine.rootContext()->contextProperty(
        QStringLiteral("localizationController")).value<QObject*>();
    if (!setup || !setup->isVisible() || !brand || !stage || !action || !localization) {
        return 3;
    }
    // Exercise the same Setup item at explicit viewport sizes independent
    // of the headless platform's reported screen geometry.
    if (!QQmlProperty::write(setup, QStringLiteral("anchors.fill"),
                             QVariant::fromValue(static_cast<QQuickItem*>(nullptr)))) {
        return 3;
    }
    setup->setWidth(1440);
    setup->setHeight(900);

    struct Step {
        int page;
        bool rtl;
        bool hasBrand;
        const char* button = nullptr;
        const char* language = nullptr;
        int nextWidth = 0;
    };
    static constexpr std::array steps{
        Step{0, true, true, "setupStageAction"},
        Step{1, true, true, nullptr, "en"},
        Step{1, false, true, nullptr, "fa"},
        Step{1, true, true, nullptr, nullptr, 900},
        Step{1, true, false, nullptr, nullptr, 430},
        Step{1, true, false, "setupBack"},
        Step{0, true, false, nullptr, nullptr, 1440},
        Step{0, true, true}
    };
    auto* const timer = new QTimer{&application};
    timer->setInterval(450);
    QObject::connect(timer, &QTimer::timeout, &application,
        [timer, setup, brand, stage, action, localization, index = std::size_t{0}]() mutable {
            const Step& step = steps.at(index);
            const qreal viewport = setup->width();
            const qreal brandWidth = step.hasBrand ? brand->width() : 0;
            const qreal formStart = step.rtl ? 0 : brandWidth;
            const qreal formEnd = step.rtl ? viewport - brandWidth : viewport;
            constexpr qreal tolerance = 2;
            const bool brandFits = !step.hasBrand || (brandWidth > 0 && brandWidth < viewport / 2
                && std::abs(brand->x() - (step.rtl ? viewport - brandWidth : 0)) < tolerance);
            const bool formFits = stage->width() > 0 && stage->x() >= formStart - tolerance
                && stage->x() + stage->width() <= formEnd + tolerance
                && action->width() > 0 && action->x() >= formStart - tolerance
                && action->x() + action->width() <= formEnd + tolerance;
            if (setup->property("currentStep").toInt() != step.page
                || brand->isVisible() != step.hasBrand || !brandFits || !formFits) {
                qCritical() << "Setup layout failed" << index << viewport
                            << brand->x() << brandWidth << stage->x() << stage->width();
                timer->stop();
                QCoreApplication::exit(15);
                return;
            }
            if (step.button) {
                auto* const button = setup->findChild<QObject*>(QString::fromLatin1(step.button));
                if (!button || !QMetaObject::invokeMethod(button, "clicked", Qt::DirectConnection)) {
                    qCritical() << "Setup action unavailable" << step.button;
                    timer->stop();
                    QCoreApplication::exit(16);
                    return;
                }
            }
            if (step.language) {
                bool applied = false;
                if (!QMetaObject::invokeMethod(localization, "setLanguageTag", Qt::DirectConnection,
                                               Q_RETURN_ARG(bool, applied),
                                               Q_ARG(QString, QString::fromLatin1(step.language)))
                    || !applied) {
                    qCritical() << "Setup language unavailable" << step.language;
                    timer->stop();
                    QCoreApplication::exit(17);
                    return;
                }
            }
            if (step.nextWidth > 0) {
                setup->setWidth(step.nextWidth);
            }
            if (++index == steps.size()) {
                timer->stop();
                QCoreApplication::exit(0);
            }
        });
    timer->start();
    return 0;
}

bool openWalletBrowserPreview(QQmlApplicationEngine& engine, QQuickWindow& window)
{
    auto* const picker = window.findChild<QObject*>(QStringLiteral("walletBrowserPicker"));
    if (!picker) {
        return false;
    }
    QQmlComponent fixture{&engine};
    fixture.setData(R"QML(
        import QtQuick

        QtObject {
            property bool walletBrowserPickerActive: true
            property string walletBrowserAppName: "Sample Mini App"
            property string walletBrowserOrigin: "https://miniapp.example"
            property string walletBrowserError: ""
            property var walletBrowserOptions: [
                { id: "brave", label: "Brave", lastUsed: true },
                { id: "chrome", label: "Chrome", lastUsed: false },
                { id: "system", lastUsed: false }
            ]
            signal walletBrowserPickerChanged()
            onWalletBrowserPickerActiveChanged: walletBrowserPickerChanged()
            function cancelWalletBrowser() { walletBrowserPickerActive = false }
            function chooseWalletBrowser(browserId) { cancelWalletBrowser() }
        }
    )QML", QUrl{});
    auto* const host = fixture.create();
    if (!host) {
        qWarning().noquote() << fixture.errorString();
        return false;
    }
    host->setParent(picker);
    return picker->setProperty("walletHost", QVariant::fromValue(host))
        && QMetaObject::invokeMethod(picker, "sync", Qt::DirectConnection);
}

} // namespace

int scheduleSnapshots(QGuiApplication& application,
                      QQmlApplicationEngine& engine,
                      PresentationBridge& presentationBridge,
                      RichComposerController& richComposerController,
                      const QStringList& arguments,
                      bool isVisualTest,
                      bool openExploreForTest,
                      const QString& snapshotPath)
{
    if (isVisualTest && arguments.contains(QStringLiteral("--exercise-setup-layout"))) {
        auto* const window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
        return window ? exerciseSetupLayout(application, engine, *window) : 3;
    }
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
        if (arguments.contains(QStringLiteral("--open-wallet-browser-picker"))
            && !openWalletBrowserPreview(engine, *window)) {
            return 3;
        }
        if (arguments.contains(QStringLiteral("--open-settings"))) {
            bool hasSection = false;
            const int section = optionValue(
                arguments, QStringLiteral("--settings-section=")).toInt(&hasSection);
            if (hasSection && section == 5) {
                // The normal snapshot fixture is signed out. Seed only the
                // presentation state for the account settings preview.
                presentationBridge.applyAuthenticationState(true, QStringLiteral("authenticated"),
                    6'841, QStringLiteral("protocolnotes"), QStringLiteral("Protocol Field Notes"),
                    {}, {}, {}, {});
                presentationBridge.applySignerState(QStringLiteral("ready"), false, true, {}, {});
            }
            auto* const shell = window->findChild<QObject*>(QStringLiteral("appShell"));
            if (shell == nullptr || (hasSection && (section < 0 || section > 6))
                || !QMetaObject::invokeMethod(shell, "openLocaleSettings", Qt::DirectConnection,
                                             Q_ARG(QVariant, QVariant{hasSection ? section : -1}))) {
                return 3;
            }
        }
        if (arguments.contains(QStringLiteral("--open-signer-recovery"))) {
            presentationBridge.applyAuthenticationState(true, QStringLiteral("authenticated"),
                6'841, QStringLiteral("protocolnotes"), QStringLiteral("Protocol Field Notes"),
                {}, QStringLiteral("farcaster-snapchain"), {}, {});
            presentationBridge.applySignerState(QStringLiteral("missing"), false, false, {}, {});
            auto* const recovery = window->findChild<QObject*>(
                QStringLiteral("interactionAuthorizationExplanation"));
            if (!recovery || !recovery->setProperty("enteringNativeWords", true)
                || !QMetaObject::invokeMethod(recovery, "open", Qt::DirectConnection)) {
                return 3;
            }
        }
        if (arguments.contains(QStringLiteral("--open-initial-setup"))) {
            bool hasStep = false;
            const int step = optionValue(
                arguments, QStringLiteral("--setup-step=")).toInt(&hasStep);
            if (hasStep) {
                auto* const setup = window->findChild<QObject*>(QStringLiteral("initialSetup"));
                if (setup == nullptr || step < 0 || step > 7
                    || !setup->setProperty("currentStep", step)) {
                    return 3;
                }
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
