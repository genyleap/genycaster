#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDebug>
#include <QFont>
#include <QGuiApplication>
#include <QFontDatabase>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkInformation>
#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#if defined(Q_OS_ANDROID)
#include "android_tinted_icon_provider.hpp"
#include <QtWebView/QtWebView>
#endif
#include <QWheelEvent>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QTimeZone>
#include <QThread>
#include <QUrl>
#include <QUrlQuery>
#include <QtTranslation>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <expected>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "presentation_bridge.hpp"
#include "rendering_controller.hpp"
#include "emoji_controller.hpp"
#include "rich_inline_text_item.hpp"
#include "rich_composer_controller.hpp"
#include "channel_picker_controller.hpp"
#include "text_selection_settings.hpp"
#include "appearance_controller.hpp"
#include "localization_controller.hpp"
#include "media_upload_controller.hpp"
#include "media_classifier_controller.hpp"
#include "media_playback_coordinator.hpp"
#include "geo_location_resolver.hpp"
#include "link_preview_resolver.hpp"
#include "cast_view_controller.hpp"
#include "browser_reaction_controller.hpp"
#include "browser_link_metadata_controller.hpp"
#include "browser_token_preview_controller.hpp"
#include "miniapp_host_controller.hpp"
#include "media_web_host_controller.hpp"
#include "miniapp_catalog_controller.hpp"
#include "miniapp_task_manager_controller.hpp"
#include "live_insights_controller.hpp"
#include "node_selection_controller.hpp"
#include "provider_profiles_controller.hpp"
#include "update_controller.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include "remote_image_provider.hpp"
#include "browser_openproof_auth.hpp"
#include "openproof_connections_controller.hpp"
#include "genypulse_activity_controller.hpp"
#if defined(__EMSCRIPTEN__)
#include "browser_wheel_fallback.hpp"
#include "browser_signer_bridge.hpp"
#include "browser_cast_payload.hpp"
#include "browser_emoji_recovery.hpp"
#endif
#include "data_storage_controller.hpp"

// Entry-point only: provider/storage modules are owned by the separate
// runtime UI binding implementation, not imported into the Qt bootstrap.
import farcaster.runtime.emoji;
import genycaster.ui.runtime;
#if GENYCASTER_HAS_MINIAPP_HOST
import farcaster.runtime.miniapp;
#endif
import farcaster.application.configuration;
import farcaster.presentation.media;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.transport.qtnetwork;
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
import farcaster.provider.hypersnap.config;
#endif
#if FARCASTER_HAS_MINIAPP_EVM_WALLET
import farcaster.capability.wallet;
import farcaster.provider.compatibility.browser_wallet.bridge;
#endif

#include "runtime_ui_bindings.hpp"

#if defined(__EMSCRIPTEN__)
namespace {
QPointer<QQuickWindow> browserWheelWindow{};
}

// Emscripten routes DOM wheel input directly into C++. Do not depend on
// Module exports, which are not guaranteed with Qt 6.12's JS loader.
namespace {
double normalizedCanvasCoordinate(int clientX, int clientY, int axis)
{
    return EM_ASM_DOUBLE({
        const x = $0;
        const y = $1;
        // The capture-phase DOM observer sees the real composed event path
        // even when Qt's screen canvas lives inside a shadow root or has
        // pointer-events retargeted to a wrapper. No Module export required.
        const hit = window.__genycasterWheelHit;
        if (hit && hit.clientX === x && hit.clientY === y
            && hit.x >= 0 && hit.y >= 0)
            return $2 === 0 ? hit.x : hit.y;
        let element = document.elementFromPoint(x, y);
        while (element && element.shadowRoot) {
            const child = element.shadowRoot.elementFromPoint(x, y);
            if (!child || child === element) break;
            element = child;
        }
        if (!(element instanceof HTMLCanvasElement)) return -1;
        const rect = element.getBoundingClientRect();
        if (rect.width <= 0 || rect.height <= 0) return -1;
        return $2 === 0 ? (x - rect.left) / rect.width
            : (y - rect.top) / rect.height;
    }, clientX, clientY, axis);
}

EM_BOOL browserWheelCallback(int type, const EmscriptenWheelEvent* event, void*)
{
    QQuickWindow* window = browserWheelWindow.data();
    if (type != EMSCRIPTEN_EVENT_WHEEL || !event || !window ||
        !window->isVisible() || event->mouse.ctrlKey) return EM_FALSE;
    EM_ASM({
        if (window.__genycasterWheelDiagnostics)
            window.__genycasterWheelDiagnostics.received += 1;
    });
    const double x = normalizedCanvasCoordinate(event->mouse.clientX, event->mouse.clientY, 0);
    const double y = normalizedCanvasCoordinate(event->mouse.clientX, event->mouse.clientY, 1);
    if (x < 0 || x > 1 || y < 0 || y > 1 ||
        !std::isfinite(event->deltaX) || !std::isfinite(event->deltaY)) return EM_FALSE;
    EM_ASM({
        if (window.__genycasterWheelDiagnostics)
            window.__genycasterWheelDiagnostics.onCanvas += 1;
    });
    const double scale = event->deltaMode == DOM_DELTA_LINE ? 16.0
        : (event->deltaMode == DOM_DELTA_PAGE ? window->height() : 1.0);
    const auto bounded = [](double v) {
        return static_cast<int>(std::lround(std::clamp(v, -2000.0, 2000.0)));
    };
    // Chromium touchpads can emit sub-pixel deltas in rapid succession.
    // Rounding each event independently to zero makes scrolling feel as if
    // some wheel ticks never arrive. Carry fractional pixels to the next tick.
    static QPointF wheelRemainder{};
    const auto pixelsForAxis = [](double value, double& remainder) {
        const double accumulated = std::clamp(value, -2000.0, 2000.0) + remainder;
        const int whole = static_cast<int>(std::trunc(accumulated));
        remainder = accumulated - whole;
        return whole;
    };
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    if (event->mouse.shiftKey) modifiers |= Qt::ShiftModifier;
    if (event->mouse.altKey) modifiers |= Qt::AltModifier;
    if (event->mouse.metaKey) modifiers |= Qt::MetaModifier;
    const QPointF local{x * window->width(), y * window->height()};
    const QPoint pixels{
        pixelsForAxis(-event->deltaX * scale, wheelRemainder.rx()),
        pixelsForAxis(-event->deltaY * scale, wheelRemainder.ry())};
    const QPoint angles{bounded(-event->deltaX * scale * 1.2),
                        bounded(-event->deltaY * scale * 1.2)};
    // WebAssembly Qt's synthesized wheel events are not reliable for
    // Flickables: they can be discarded by the screen/pointer handler even
    // while dragging scrolls normally. Move the nearest interactive
    // Flickable's viewport directly, including ComboBox popup ListViews.
    if (genycaster::ui::browser_wheel::scroll(*window, local, pixels)) {
        EM_ASM({
            // The listener on window is registered in capture phase, before
            // Qt's own canvas listener. Chromium may otherwise deliver the
            // same wheel tick twice (native viewport + Qt synthetic event),
            // sometimes scrolling back to the previous location.
            const e = window.__genycasterWheelEvent;
            if (e && e.clientX === $0 && e.clientY === $1) {
                e.stopImmediatePropagation();
                if (e.cancelable) e.preventDefault();
                window.__genycasterWheelEvent = null;
            }
            if (window.__genycasterWheelDiagnostics) {
                window.__genycasterWheelDiagnostics.forwarded += 1;
                window.__genycasterWheelDiagnostics.directScrolled += 1;
            }
        }, event->mouse.clientX, event->mouse.clientY);
        return EM_TRUE;
    }
    // Retain Qt wheel delivery for non-Flickable controls, e.g. an editable
    // spin box; don't consume events that Qt itself declines.
    QWheelEvent wheel{local, window->mapToGlobal(local.toPoint()), pixels, angles,
                      Qt::NoButton, modifiers, Qt::NoScrollPhase, false};
    wheel.setAccepted(false);
    QCoreApplication::sendEvent(window, &wheel);
    // Do not swallow wheel events Qt Quick did not handle. In particular,
    // popup/list bounds and early screen initialization may leave events
    // unaccepted; allow Qt's own web event routing to handle those.
    if (!wheel.isAccepted()) return EM_FALSE;
    EM_ASM({
        const e = window.__genycasterWheelEvent;
        if (e && e.clientX === $0 && e.clientY === $1) {
            e.stopImmediatePropagation();
            if (e.cancelable) e.preventDefault();
            window.__genycasterWheelEvent = null;
        }
        if (window.__genycasterWheelDiagnostics)
            window.__genycasterWheelDiagnostics.forwarded += 1;
    }, event->mouse.clientX, event->mouse.clientY);
    return EM_TRUE;
}

void installBrowserWheelBridge(QQuickWindow* window)
{
    browserWheelWindow = window;
    EM_ASM({
        if (!window.__genycasterWheelPathInstalled) {
            window.__genycasterWheelPathInstalled = true;
            window.addEventListener('wheel', function(event) {
                const path = event.composedPath ? event.composedPath() : [event.target];
                const canvas = path.find(node => node instanceof HTMLCanvasElement);
                const hit = Object.create(null);
                hit.clientX = event.clientX;
                hit.clientY = event.clientY;
                hit.x = -1;
                hit.y = -1;
                if (canvas) {
                    const rect = canvas.getBoundingClientRect();
                    if (rect.width > 0 && rect.height > 0) {
                        hit.x = (event.clientX - rect.left) / rect.width;
                        hit.y = (event.clientY - rect.top) / rect.height;
                    }
                }
                window.__genycasterWheelHit = hit;
                window.__genycasterWheelEvent = event;
            }, {capture: true, passive: true});
        }
    });
    EM_ASM({
        if (!window.__genycasterWheelDiagnostics) {
            window.__genycasterWheelDiagnostics = Object.create(null);
            window.__genycasterWheelDiagnostics.received = 0;
            window.__genycasterWheelDiagnostics.onCanvas = 0;
            window.__genycasterWheelDiagnostics.forwarded = 0;
            window.__genycasterWheelDiagnostics.directScrolled = 0;
        }
    });
    const EMSCRIPTEN_RESULT result = emscripten_set_wheel_callback(
        EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, browserWheelCallback);
    if (result != EMSCRIPTEN_RESULT_SUCCESS)
        qWarning() << "WASM wheel callback registration failed:" << result;
}
}
#endif

namespace {

namespace application = farcaster::application;
namespace foundation = farcaster::foundation;
using genycaster::ui::runtime::Runtime;
using genycaster::ui::runtime::makeRuntime;
using genycaster::ui::bindings::connectRuntime;
using genycaster::ui::bindings::connectMediaPlayback;
using genycaster::ui::bindings::applyProfileVerificationFixture;
#if FARCASTER_HAS_MEDIA_UPLOAD
using genycaster::ui::bindings::connectMediaUpload;
#endif
#if defined(__EMSCRIPTEN__)
using genycaster::ui::bindings::connectBrowserMediaUpload;
#endif

[[nodiscard]] QString optionValue(const QStringList& arguments, const QString& prefix)
{
    for (const QString& argument : arguments) {
        if (argument.startsWith(prefix)) {
            return argument.sliced(prefix.size()).trimmed();
        }
    }
    return {};
}


[[nodiscard]] QString startupRenderingProfile(const int argc, char* argv[])
{
    QString explicitProfile{};
    for (int index = 1; index < argc; ++index) {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        constexpr auto prefix = "--render-profile=";
        if (argument.startsWith(QString::fromLatin1(prefix))) {
            explicitProfile = argument.mid(static_cast<qsizetype>(std::char_traits<char>::length(prefix)));
            break;
        }
    }
    if (explicitProfile.isEmpty()) {
        explicitProfile = qEnvironmentVariable("GENYCASTER_RENDER_PROFILE").trimmed();
    }
    if (!explicitProfile.isEmpty()) {
        const QString canonical = RenderingController::canonicalProfile(explicitProfile);
        return canonical.isEmpty() ? QStringLiteral("adaptive") : canonical;
    }

    QSettings startupSettings{
        QSettings::NativeFormat,
        QSettings::UserScope,
        QStringLiteral("Genycaster"),
        QStringLiteral("Genycaster")};
    return RenderingController::canonicalProfile(
        startupSettings.value(QStringLiteral("rendering/profile"), QStringLiteral("adaptive")).toString());
}

[[nodiscard]] QString normalizeConfigurationToken(QString value)
{
    value = value.trimmed();
    for (int pass = 0; pass < 2 && value.size() >= 2; ++pass) {
        const QChar first = value.front();
        const QChar last = value.back();
        if ((first == QLatin1Char('\"') && last == QLatin1Char('\"')) ||
            (first == QLatin1Char('\'') && last == QLatin1Char('\''))) {
            value = value.sliced(1, value.size() - 2).trimmed();
            continue;
        }
        if (value.startsWith(QStringLiteral("\\\"")) &&
            value.endsWith(QStringLiteral("\\\"")) && value.size() >= 4) {
            value = value.sliced(2, value.size() - 4).trimmed();
            continue;
        }
        break;
    }
    return value;
}

[[nodiscard]] bool hasUnsafeConfigurationCharacters(const QStringView value)
{
    for (const QChar ch : value) {
        if (ch.isSpace() || ch == QLatin1Char('\\') || ch == QLatin1Char('\"') ||
            ch == QLatin1Char('\'')) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool isValidHttpServiceUrl(
    const QString& value,
    const bool allowQuery = false)
{
    if (value.isEmpty() || hasUnsafeConfigurationCharacters(value)) {
        return false;
    }

    const QUrl url{value, QUrl::StrictMode};
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() ||
        (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) ||
        url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasFragment()) {
        return false;
    }
    return allowQuery || !url.hasQuery();
}

[[nodiscard]] bool isValidSiwfUri(const QString& value)
{
    return isValidHttpServiceUrl(value, true);
}

[[nodiscard]] bool isValidSiwfDomain(const QString& value)
{
    if (value.isEmpty() || hasUnsafeConfigurationCharacters(value) ||
        value.contains(QLatin1Char('/')) || value.contains(QLatin1Char('?')) ||
        value.contains(QLatin1Char('#'))) {
        return false;
    }
    const QUrl authority{QStringLiteral("http://") + value, QUrl::StrictMode};
    return authority.isValid() && !authority.host().isEmpty() && authority.userInfo().isEmpty();
}

[[nodiscard]] bool siwfDomainMatchesUri(const QString& domain, const QString& uri)
{
    const QUrl authority{QStringLiteral("http://") + domain, QUrl::StrictMode};
    const QUrl relyingParty{uri, QUrl::StrictMode};
    if (!authority.isValid() || !relyingParty.isValid()) {
        return false;
    }
    if (authority.host().compare(relyingParty.host(), Qt::CaseInsensitive) != 0) {
        return false;
    }
    return authority.port(-1) == relyingParty.port(-1);
}

[[nodiscard]] std::function<foundation::result::Status(const std::string&)>
makeWalletApprovalLauncher(QString launchTemplate, const bool preferCustodyWalletBrowser = false)
{
    return [launchTemplate = std::move(launchTemplate), preferCustodyWalletBrowser](const std::string& approvalUrl)
        -> foundation::result::Status {
        const QString rawUrl = QString::fromStdString(approvalUrl);
        QString target = rawUrl;
        if (!launchTemplate.isEmpty()) {
            if (!launchTemplate.contains(QStringLiteral("{url}")) &&
                !launchTemplate.contains(QStringLiteral("{rawUrl}"))) {
                return std::unexpected(foundation::error::Error::make(
                    foundation::error::ErrorCode::InvalidArgument,
                    "Wallet launch URL template must contain {url} or {rawUrl}."));
            }
            target = launchTemplate;
            target.replace(
                QStringLiteral("{url}"),
                QString::fromLatin1(QUrl::toPercentEncoding(rawUrl)));
            target.replace(QStringLiteral("{rawUrl}"), rawUrl);
        }

        const QUrl parsed{target};
        const QString scheme = parsed.scheme().toLower();
        if (!parsed.isValid() || scheme.isEmpty() ||
            scheme == QStringLiteral("javascript") ||
            scheme == QStringLiteral("data") ||
            scheme == QStringLiteral("file")) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Wallet approval launcher produced an invalid or unsafe URL."));
        }

        bool opened{false};
        const bool preferInstalledWalletBrowser = preferCustodyWalletBrowser && launchTemplate.isEmpty()
            && parsed.scheme() == QStringLiteral("http")
            && parsed.host() == QStringLiteral("127.0.0.1");
        const auto open = [&opened, parsed, preferInstalledWalletBrowser] {
#if defined(Q_OS_MACOS)
            if (preferInstalledWalletBrowser) {
                // Safari has no injected EIP-1193 wallet by default. An
                // installed wallet-capable browser avoids forcing users to
                // hand-copy a time-limited loopback URL on every attempt.
                for (const QString& appName : {
                         QStringLiteral("Brave Browser"),
                         QStringLiteral("Google Chrome")}) {
                    const QString bundle = appName + QStringLiteral(".app");
                    if (QFileInfo::exists(QStringLiteral("/Applications/") + bundle)
                        || QFileInfo::exists(QDir::homePath()
                                            + QStringLiteral("/Applications/") + bundle)) {
                        opened = QProcess::startDetached(
                            QStringLiteral("/usr/bin/open"),
                            {QStringLiteral("-a"), appName, parsed.toString()});
                        if (opened) break;
                    }
                }
            }
#else
            Q_UNUSED(preferInstalledWalletBrowser);
#endif
            if (!opened) opened = QDesktopServices::openUrl(parsed);
        };
        if (auto* app = QCoreApplication::instance(); app != nullptr &&
            QThread::currentThread() != app->thread()) {
            if (!QMetaObject::invokeMethod(app, open, Qt::BlockingQueuedConnection)) {
                return std::unexpected(foundation::error::Error::make(
                    foundation::error::ErrorCode::Unavailable,
                    "Could not dispatch wallet approval URL opening to the Qt application thread."));
            }
        } else {
            open();
        }
        if (!opened) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "The platform could not open a browser or wallet for Mini App approval."));
        }
        return {};
    };
}

#if FARCASTER_HAS_MINIAPP_EVM_WALLET
[[nodiscard]] std::vector<std::string> miniAppEthereumChains()
{
    // Mirrors the EVM CAIP-2 chain set accepted by the current Farcaster Mini App manifest schema.
    const std::array<std::string_view, 16> allowed{
        "eip155:1", "eip155:8453", "eip155:42161", "eip155:421614",
        "eip155:84532", "eip155:666666666", "eip155:100", "eip155:10",
        "eip155:11155420", "eip155:137", "eip155:11155111", "eip155:7777777",
        "eip155:130", "eip155:10143", "eip155:42220", "eip155:999",
    };
    const QString configured = qEnvironmentVariable("GENYCASTER_MINIAPP_EVM_CHAINS").trimmed();
    if (configured.isEmpty()) {
        return {allowed.begin(), allowed.end()};
    }
    std::vector<std::string> selected{};
    for (const QString& token : configured.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const std::string chain = token.trimmed().toStdString();
        if (std::ranges::find(allowed, std::string_view{chain}) == allowed.end()) {
            qWarning().noquote() << "Ignoring unsupported GENYCASTER_MINIAPP_EVM_CHAINS entry:"
                                 << QString::fromStdString(chain);
            continue;
        }
        if (std::ranges::find(selected, chain) == selected.end()) selected.push_back(chain);
    }
    return selected;
}
#endif

}

int main(int argc, char* argv[])
{
#if defined(Q_OS_ANDROID)
    // Match the bundled OpenSSL 3 sonames before Qt initializes TLS/networking.
    qputenv("ANDROID_OPENSSL_SUFFIX", "_3");
    // Native Android WebView must be initialized before the GUI application.
    QtWebView::initialize();
#endif
    // Adaptive is the product default. Qt Quick's standard scene graph keeps
    // its default vertex-AA path, while Genycaster's curved design primitives
    // use Shape.CurveRenderer for native high-quality curve AA without MSAA.
    // Fidelity remains an explicit 4x choice because window-wide multisampling
    // has hardware-dependent GPU and memory cost.
    const QString startupRenderProfile = startupRenderingProfile(argc, argv);
    QSurfaceFormat surfaceFormat = QSurfaceFormat::defaultFormat();
    surfaceFormat.setSamples(RenderingController::sampleCountForProfile(startupRenderProfile));
    QSurfaceFormat::setDefaultFormat(surfaceFormat);

    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Genycaster"));
    QCoreApplication::setApplicationName(QStringLiteral("Genycaster"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(GENYCASTER_APP_VERSION));
    QQuickStyle::setStyle(QStringLiteral(FARCASTER_QT_QUICK_CONTROLS_STYLE));
    QString bundledPersianFontFamily{};
    QString bundledDevanagariFontFamily{};
    QString bundledJapaneseFontFamily{};
    QString bundledKoreanFontFamily{};
    QString bundledChineseFontFamily{};
    QString bundledIconFontFamily{};
    QString bundledIconRegularFontFamily{};

    // The minimal QPA backend used by headless structure tests intentionally has
    // no application-font backend. Shipping platforms load every bundled family;
    // a missing resource is a packaging defect rather than a system-font fallback.
    if (QGuiApplication::platformName() != QStringLiteral("minimal")) {
        const auto loadApplicationFont = [](const QString& resource) -> QStringList {
            const int fontId = QFontDatabase::addApplicationFont(resource);
            if (fontId < 0) {
                qCritical().noquote() << "Unable to load embedded Genycaster font:" << resource;
                return {};
            }
            return QFontDatabase::applicationFontFamilies(fontId);
        };

        constexpr std::array bundledPersianFontResources{
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-Regular.ttf",
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-Medium.ttf",
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-SemiBold.ttf",
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-Bold.ttf",
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-Black.ttf",
        };
        for (const char* const resource : bundledPersianFontResources) {
            const QStringList families = loadApplicationFont(QString::fromLatin1(resource));
            if (families.isEmpty()) {
                return 2;
            }
            if (bundledPersianFontFamily.isEmpty()) {
                bundledPersianFontFamily = families.front();
            }
        }

#if defined(__EMSCRIPTEN__)
        // Qt 6.12 supports CBDT/CBLC and emoji sequence segmentation.
        // System/browser fonts are not available in the WASM sandbox.
        const QStringList colorEmojiFamilies = loadApplicationFont(QStringLiteral(
            ":/qt/qml/Farcaster/Client/assets/fonts/GenycasterColorEmoji.ttf"));
        const QStringList monoEmojiFamilies = loadApplicationFont(QStringLiteral(
            ":/qt/qml/Farcaster/Client/assets/fonts/GenycasterEmojiFallback.ttf"));
        if (monoEmojiFamilies.isEmpty()) {
            return 2;
        }
        if (!colorEmojiFamilies.isEmpty()) {
            QFontDatabase::setApplicationEmojiFontFamilies(
                {colorEmojiFamilies.front(), monoEmojiFamilies.front()});
        } else {
            qWarning() << "Bundled color emoji unavailable; using monochrome fallback";
            QFontDatabase::addApplicationEmojiFontFamily(monoEmojiFamilies.front());
        }
#endif

        // Devanagari/CJK resources are optional in source/developer builds.
        // Production packaging can require them through
        // GENYCASTER_REQUIRE_SCRIPT_FONTS; when absent, the platform fallback
        // families selected below provide shaping without making configure depend
        // on network access.
        const QString devanagariResource =
            QStringLiteral(":/qt/qml/Farcaster/Client/assets/fonts/NotoSansDevanagari.ttf");
        if (QFile::exists(devanagariResource)) {
            const QStringList devanagariFamilies = loadApplicationFont(devanagariResource);
            if (!devanagariFamilies.isEmpty()) {
                bundledDevanagariFontFamily = devanagariFamilies.front();
            }
        }

        const QString cjkResource =
            QStringLiteral(":/qt/qml/Farcaster/Client/assets/fonts/NotoSansCJK-Regular.ttc");
        if (QFile::exists(cjkResource)) {
            const QStringList cjkFamilies = loadApplicationFont(cjkResource);
            if (!cjkFamilies.isEmpty()) {
                const auto chooseCjkFamily = [&cjkFamilies](const QString& suffix) {
                    const auto exact = std::ranges::find_if(cjkFamilies, [&](const QString& family) {
                        return family.contains(suffix, Qt::CaseInsensitive);
                    });
                    return exact != cjkFamilies.end() ? *exact : cjkFamilies.front();
                };
                bundledJapaneseFontFamily = chooseCjkFamily(QStringLiteral("JP"));
                bundledKoreanFontFamily = chooseCjkFamily(QStringLiteral("KR"));
                bundledChineseFontFamily = chooseCjkFamily(QStringLiteral("SC"));
            }
        }

        const QStringList iconRegularFamilies = loadApplicationFont(
            QStringLiteral(":/qt/qml/Farcaster/Client/assets/icons/fa-regular-400.ttf"));
        const QStringList iconSolidFamilies = loadApplicationFont(
            QStringLiteral(":/qt/qml/Farcaster/Client/assets/icons/fa-solid-900.ttf"));
        if (iconRegularFamilies.isEmpty() || iconSolidFamilies.isEmpty()) {
            return 2;
        }
        bundledIconRegularFontFamily = iconRegularFamilies.front();
        // Keep the legacy property populated for compatibility with older QML;
        // new Icon.qml selects the regular or solid family explicitly.
        bundledIconFontFamily = iconSolidFamilies.front();
    }

    const auto installedFamilies = QFontDatabase::families();
    const auto preferredFamily = [&installedFamilies](const QStringList& candidates, const QString& fallback) {
        for (const auto& candidate : candidates) {
            if (installedFamilies.contains(candidate, Qt::CaseInsensitive)) {
                return candidate;
            }
        }
        return fallback;
    };
    const QString systemFamily = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
#if defined(Q_OS_MACOS)
    const QString platformTextFontFamily = preferredFamily(
        {QStringLiteral("SF Pro Text"), QStringLiteral("Helvetica Neue")}, systemFamily);
    const QString platformRtlFontFamily = preferredFamily(
        {QStringLiteral("Geeza Pro"), QStringLiteral("Arial Unicode MS"), QStringLiteral("Arial")}, systemFamily);
    const QString platformDevanagariFontFamily = preferredFamily(
        {QStringLiteral("Kohinoor Devanagari"), QStringLiteral("Devanagari Sangam MN")}, systemFamily);
    const QString platformJapaneseFontFamily = preferredFamily(
        {QStringLiteral("Hiragino Sans"), QStringLiteral("YuGothic")}, systemFamily);
    const QString platformKoreanFontFamily = preferredFamily(
        {QStringLiteral("Apple SD Gothic Neo"), QStringLiteral("Arial Unicode MS")}, systemFamily);
    const QString platformChineseFontFamily = preferredFamily(
        {QStringLiteral("PingFang SC"), QStringLiteral("Hiragino Sans GB")}, systemFamily);
#elif defined(Q_OS_WIN)
    const QString platformTextFontFamily = preferredFamily(
        {QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI")}, systemFamily);
    const QString platformRtlFontFamily = preferredFamily(
        {QStringLiteral("Segoe UI"), QStringLiteral("Tahoma"), QStringLiteral("Arial")}, systemFamily);
    const QString platformDevanagariFontFamily = preferredFamily(
        {QStringLiteral("Nirmala UI"), QStringLiteral("Mangal")}, systemFamily);
    const QString platformJapaneseFontFamily = preferredFamily(
        {QStringLiteral("Yu Gothic UI"), QStringLiteral("Meiryo UI")}, systemFamily);
    const QString platformKoreanFontFamily = preferredFamily(
        {QStringLiteral("Malgun Gothic")}, systemFamily);
    const QString platformChineseFontFamily = preferredFamily(
        {QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft JhengHei UI")}, systemFamily);
#else
    const QString platformTextFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans")}, systemFamily);
    const QString platformRtlFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans Arabic"), QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans")}, systemFamily);
    const QString platformDevanagariFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans Devanagari"), QStringLiteral("Noto Sans")}, systemFamily);
    const QString platformJapaneseFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans CJK JP"), QStringLiteral("Noto Sans")}, systemFamily);
    const QString platformKoreanFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans CJK KR"), QStringLiteral("Noto Sans")}, systemFamily);
    const QString platformChineseFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans CJK SC"), QStringLiteral("Noto Sans")}, systemFamily);
#endif

    application.setFont(QFont{platformTextFontFamily});

    const QStringList arguments = QCoreApplication::arguments();
    const bool isSmokeTest = arguments.contains(QStringLiteral("--smoke-test"));
    const bool isVisualTest = arguments.contains(QStringLiteral("--visual-test"));
    const bool isRuntimeSmokeTest = arguments.contains(QStringLiteral("--runtime-smoke-test"));
    const bool openExploreForTest = arguments.contains(QStringLiteral("--open-explore"));
    const qsizetype snapshotIndex = arguments.indexOf(QStringLiteral("--snapshot"));
    const QString snapshotPath = snapshotIndex >= 0 && snapshotIndex + 1 < arguments.size()
        ? arguments[snapshotIndex + 1] : QString{};

    const QString settingsPath = optionValue(arguments, QStringLiteral("--settings-path="));
#if defined(__EMSCRIPTEN__)
    // QSettings NativeFormat writes under ~/.config, which is not mounted
    // by IDBFS. Keep only non-secret preferences/consent in the durable
    // virtual user-data directory. The preRun hook restores it before main().
    auto settingsStorage = std::make_unique<QSettings>(
        settingsPath.isEmpty()
            ? QStringLiteral("/home/web_user/.local/share/genycaster-settings.ini")
            : settingsPath,
        QSettings::IniFormat);
#else
    auto settingsStorage = settingsPath.isEmpty()
        ? std::make_unique<QSettings>()
        : std::make_unique<QSettings>(settingsPath, QSettings::IniFormat);
#endif
    QSettings& settings = *settingsStorage;

    const auto deploymentEndpoints =
        application::configuration::configuredDeploymentEndpoints();
    const auto deploymentIdentity =
        application::configuration::configuredDeploymentIdentity();

    QString networkProxyText = optionValue(arguments, QStringLiteral("--network-proxy="));
    if (networkProxyText.isEmpty()) {
        networkProxyText = qEnvironmentVariable("GENYCASTER_NETWORK_PROXY").trimmed();
    }
    if (networkProxyText.isEmpty()) {
        networkProxyText = settings.value(
            QStringLiteral("network/proxyMode"), QStringLiteral("system")).toString();
    }
    networkProxyText = networkProxyText.toLower();
    farcaster::transport::qtnetwork::ProxyPolicy networkProxyPolicy{};
    if (networkProxyText == QStringLiteral("direct")) {
        networkProxyPolicy = farcaster::transport::qtnetwork::ProxyPolicy::Direct;
    } else if (networkProxyText == QStringLiteral("system")) {
        networkProxyPolicy = farcaster::transport::qtnetwork::ProxyPolicy::System;
    } else {
        qCritical().noquote()
            << "GENYCASTER_NETWORK_PROXY/--network-proxy must be 'system' or 'direct'.";
        return 2;
    }

    QString providerBaseUrlText = optionValue(
        arguments, QStringLiteral("--hypersnap-base-url="));
    if (providerBaseUrlText.isEmpty()) {
        providerBaseUrlText = qEnvironmentVariable("GENYCASTER_HYPERSNAP_BASE_URL").trimmed();
    }
    if (providerBaseUrlText.isEmpty()) {
        providerBaseUrlText = settings.value(
            QStringLiteral("network/hypersnapBaseUrl")).toString().trimmed();
    }
    providerBaseUrlText = normalizeConfigurationToken(std::move(providerBaseUrlText));
    if (!providerBaseUrlText.isEmpty() && !isValidHttpServiceUrl(providerBaseUrlText)) {
        qWarning().noquote()
            << "Ignoring invalid Hypersnap base URL override; using the built-in public read source.";
        providerBaseUrlText.clear();
    }
    const std::optional<std::string> providerBaseUrl = providerBaseUrlText.isEmpty()
        ? std::nullopt : std::optional<std::string>{providerBaseUrlText.toStdString()};

    // An independent hosted Farcaster v2-compatible indexed API is optional.
    // Never hardcode/distribute a Neynar key; a deployment supplies it in the
    // process environment or proxies requests from its own authenticated host.
    QString farcasterReadBaseUrlText = optionValue(
        arguments, QStringLiteral("--farcaster-read-base-url="));
    if (farcasterReadBaseUrlText.isEmpty()) {
        farcasterReadBaseUrlText = qEnvironmentVariable(
            "GENYCASTER_FARCASTER_READ_BASE_URL").trimmed();
    }
    QString farcasterReadApiKeyText = qEnvironmentVariable(
        "GENYCASTER_FARCASTER_READ_API_KEY").trimmed();
    if (!farcasterReadApiKeyText.isEmpty() && farcasterReadBaseUrlText.isEmpty()) {
        farcasterReadBaseUrlText = QStringLiteral("https://api.neynar.com");
    }
    farcasterReadBaseUrlText = normalizeConfigurationToken(std::move(farcasterReadBaseUrlText));
    if (!farcasterReadBaseUrlText.isEmpty() && !isValidHttpServiceUrl(farcasterReadBaseUrlText)) {
        qWarning().noquote() << "Farcaster read endpoint is invalid; hosted provider will not be registered.";
        farcasterReadBaseUrlText.clear();
    }
    if (farcasterReadApiKeyText.size() > 256
        || farcasterReadApiKeyText.contains(QLatin1Char('\r'))
        || farcasterReadApiKeyText.contains(QLatin1Char('\n'))) {
        qCritical().noquote() << "Invalid Farcaster read credential format.";
        return 2;
    }
    // Never disclose a Neynar API key to a custom host or redirect target.
    if (!farcasterReadApiKeyText.isEmpty()
        && QUrl(farcasterReadBaseUrlText).host().compare(
            QStringLiteral("api.neynar.com"), Qt::CaseInsensitive) != 0) {
        qCritical().noquote()
            << "Neynar API key may only be sent to the canonical api.neynar.com host.";
        return 2;
    }
    const std::optional<std::string> farcasterReadBaseUrl = farcasterReadBaseUrlText.isEmpty()
        ? std::nullopt : std::optional<std::string>{farcasterReadBaseUrlText.toStdString()};
    const std::optional<std::string> farcasterReadApiKey = farcasterReadApiKeyText.isEmpty()
        ? std::nullopt : std::optional<std::string>{farcasterReadApiKeyText.toStdString()};

    // Profiles are adapter-instance registrations, not a global provider URL.
    // A profile declares the protocol contract explicitly; unknown contracts
    // fail closed rather than being bound to whichever API happens to respond.
    QString providerProfilesJson = optionValue(
        arguments, QStringLiteral("--read-provider-profiles-json="));
    if (providerProfilesJson.isEmpty()) {
        providerProfilesJson = qEnvironmentVariable(
            "GENYCASTER_READ_PROVIDER_PROFILES_JSON").trimmed();
    }
    if (providerProfilesJson.isEmpty()) {
        providerProfilesJson = settings.value(
            QStringLiteral("network/readProviderProfilesJson")).toString().trimmed();
    }
    const auto validProviderId = [](const QString& id) {
        if (id.isEmpty() || id.size() > 64) return false;
        for (const QChar ch : id) {
            const ushort code = ch.unicode();
            if (!((code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z')
                  || (code >= '0' && code <= '9') || code == '-' || code == '_'
                  || code == '.')) return false;
        }
        return true;
    };
    std::vector<genycaster::ui::runtime::ReadProviderProfile> additionalReadProviders{};
    if (!providerProfilesJson.isEmpty()) {
        QJsonParseError parseError{};
        const auto document = QJsonDocument::fromJson(providerProfilesJson.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isArray()
            || document.array().size() > 16) {
            qCritical().noquote() << "Read provider profiles must be a JSON array of at most 16 entries.";
            return 2;
        }
        std::unordered_set<std::string> seenIds{"hypersnap-v2", "farcaster-snapchain"};
        for (const QJsonValue& entry : document.array()) {
            if (!entry.isObject()) {
                qCritical().noquote() << "Every read provider profile must be a JSON object.";
                return 2;
            }
            const QJsonObject profile = entry.toObject();
            const QString id = profile.value(QStringLiteral("id")).toString().trimmed();
            const QString contract = profile.value(QStringLiteral("contract")).toString().trimmed();
            const QString endpoint = profile.value(QStringLiteral("endpoint")).toString().trimmed();
            const QString label = profile.value(QStringLiteral("label")).toString().trimmed();
            const auto hostClass = profile.value(QStringLiteral("selfHosted"));
            if (!validProviderId(id) || !isValidHttpServiceUrl(endpoint)
                || contract.isEmpty() || !hostClass.isBool()
                || !seenIds.insert(id.toStdString()).second) {
                qCritical().noquote() << "Invalid, duplicate, or credential-bearing read provider profile.";
                return 2;
            }
            additionalReadProviders.push_back({
                .id = id.toStdString(),
                .displayName = label.isEmpty() ? id.toStdString() : label.toStdString(),
                .endpoint = endpoint.toStdString(),
                .adapterContract = contract.toStdString(),
                .selfHosted = hostClass.toBool(),
            });
        }
    }

    // Provider API keys are ephemeral, per-instance operator inputs.
    // Never read secrets from saved provider profiles or QSettings, and never
    // log the JSON or its values. An unknown ID or insecure endpoint fails
    // closed rather than silently targeting another adapter.
    std::unordered_map<std::string, std::string> readProviderApiKeys{};
    QByteArray providerSecretsJson =
        qgetenv("GENYCASTER_READ_PROVIDER_API_KEYS_JSON");
#ifdef __EMSCRIPTEN__
    if (providerSecretsJson.isEmpty()) {
        // Explicitly supplied per-tab browser credentials. Never persist
        // these keys in QSettings/IDBFS or bundle them into WASM assets.
        const char* sessionJson = reinterpret_cast<const char*>(EM_ASM_PTR({
            try {
                const value = sessionStorage.getItem("genycaster:read-session-keys") || "{}";
                const bytes = lengthBytesUTF8(value) + 1;
                const ptr = _malloc(bytes);
                stringToUTF8(value, ptr, bytes);
                return ptr;
            } catch (_) { return 0; }
        }));
        if (sessionJson) {
            providerSecretsJson = QByteArray{sessionJson};
            std::free(const_cast<char*>(sessionJson));
        }
    }
#endif
    if (!providerSecretsJson.isEmpty()) {
        QJsonParseError secretParseError{};
        const auto secrets = QJsonDocument::fromJson(
            providerSecretsJson, &secretParseError);
        if (secretParseError.error != QJsonParseError::NoError
            || !secrets.isObject() || secrets.object().size() > 16) {
            qCritical().noquote()
                << "Indexed read credentials must be a JSON object with at most 16 provider IDs.";
            return 2;
        }
        const auto entries = secrets.object();
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            const auto candidate = std::find_if(
                additionalReadProviders.cbegin(), additionalReadProviders.cend(),
                [&it](const auto& profile) {
                    return profile.id == it.key().toStdString()
                        && (profile.adapterContract == "hypersnap-v2"
                            || profile.adapterContract == "snapchain-v1")
                        && QUrl(QString::fromStdString(profile.endpoint))
                            .scheme().compare(QStringLiteral("https"),
                                              Qt::CaseInsensitive) == 0;
                });
            if (candidate == additionalReadProviders.cend()
                || !it.value().isString()) {
                qCritical().noquote()
                    << "Read credential ID must select a configured HTTPS provider.";
                return 2;
            }
            const QByteArray value = it.value().toString().toUtf8();
            if (value.isEmpty() || value.size() > 256
                || std::any_of(value.cbegin(), value.cend(), [](const char character) {
                    const auto ascii = static_cast<unsigned char>(character);
                    return ascii < 33U || ascii > 126U;
                })) {
                qCritical().noquote() << "Invalid indexed read credential format.";
                return 2;
            }
            readProviderApiKeys.emplace(it.key().toStdString(), value.toStdString());
        }
    }

    // Non-strict routing may contact a different indexed provider. Require
    // explicit user/operator consent; absence of a setting fails closed.
    const bool allowCompatibleReadFallback =
        arguments.contains(QStringLiteral("--allow-compatible-read-fallback"))
        || (qEnvironmentVariableIsSet("GENYCASTER_ALLOW_COMPATIBLE_READ_FALLBACK")
                ? qEnvironmentVariable("GENYCASTER_ALLOW_COMPATIBLE_READ_FALLBACK")
                      .trimmed() == QStringLiteral("1")
                : settings.value(QStringLiteral("network/allowCompatibleReadFallback"),
                                 false).toBool());
    QString preferredReadProviderText = optionValue(
        arguments, QStringLiteral("--preferred-read-provider-id="));
    if (preferredReadProviderText.isEmpty()) {
        preferredReadProviderText = qEnvironmentVariable(
            "GENYCASTER_PREFERRED_READ_PROVIDER_ID").trimmed();
    }
    if (preferredReadProviderText.isEmpty()) {
        preferredReadProviderText = settings.value(
            QStringLiteral("network/preferredReadProviderId")).toString().trimmed();
    }
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    // HyperSnap is the default *read* experience, never a forced signer or
    // write adapter. Preserve explicit user choices and self-hosted policy.
    if (preferredReadProviderText.isEmpty()
        && farcaster::application::configuration::configuredDeploymentProfile()
            != farcaster::application::configuration::DeploymentProfile::StrictSelfHosted) {
        preferredReadProviderText = QStringLiteral("hypersnap-v2");
        // Keep the settings UI in agreement with the actual applied runtime.
        // In a read-only settings backend, the runtime preference still works.
        settings.setValue(QStringLiteral("network/preferredReadProviderId"),
                          preferredReadProviderText);
        settings.sync();
    }
#endif
    if (!preferredReadProviderText.isEmpty() && !validProviderId(preferredReadProviderText)) {
        qCritical().noquote() << "Preferred read provider id is invalid.";
        return 2;
    }
    const std::optional<std::string> preferredReadProviderId =
        preferredReadProviderText.isEmpty() ? std::nullopt
            : std::optional<std::string>{preferredReadProviderText.toStdString()};

    QString writeProfilesJson = optionValue(
        arguments, QStringLiteral("--write-provider-profiles-json="));
    if (writeProfilesJson.isEmpty()) {
        writeProfilesJson = qEnvironmentVariable(
            "GENYCASTER_WRITE_PROVIDER_PROFILES_JSON").trimmed();
    }
    if (writeProfilesJson.isEmpty()) {
        writeProfilesJson = settings.value(
            QStringLiteral("network/writeProviderProfilesJson")).toString().trimmed();
    }
    std::vector<genycaster::ui::runtime::WriteProviderProfile> additionalWriteProviders{};
    if (!writeProfilesJson.isEmpty()) {
        QJsonParseError parseError{};
        const auto document = QJsonDocument::fromJson(writeProfilesJson.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isArray()
            || document.array().size() > 16) {
            qCritical().noquote()
                << "Write provider profiles must be a JSON array of at most 16 entries.";
            return 2;
        }
        std::unordered_set<std::string> ids{"primary-protocol-writer",
            "hypersnap-v2", "farcaster-snapchain"};
        for (const auto& read : additionalReadProviders) ids.insert(read.id);
        for (const QJsonValue& entry : document.array()) {
            if (!entry.isObject()) {
                qCritical().noquote() << "Write provider profile must be an object.";
                return 2;
            }
            const auto profile = entry.toObject();
            const QString id = profile.value(QStringLiteral("id")).toString().trimmed();
            const QString contract = profile.value(QStringLiteral("contract")).toString().trimmed();
            const QString endpoint = profile.value(QStringLiteral("endpoint")).toString().trimmed();
            const QString readEndpoint = profile.value(
                QStringLiteral("readEndpoint")).toString().trimmed();
            const QString label = profile.value(QStringLiteral("label")).toString().trimmed();
            const QJsonValue selfHosted = profile.value(QStringLiteral("selfHosted"));
            if (!validProviderId(id) || !isValidHttpServiceUrl(endpoint)
                || (!readEndpoint.isEmpty() && !isValidHttpServiceUrl(readEndpoint))
                || contract.isEmpty() || !selfHosted.isBool()
                || !ids.insert(id.toStdString()).second) {
                qCritical().noquote()
                    << "Invalid, duplicate, or credential-bearing write provider profile.";
                return 2;
            }
            additionalWriteProviders.push_back({
                .id = id.toStdString(),
                .displayName = label.isEmpty() ? id.toStdString() : label.toStdString(),
                .endpoint = endpoint.toStdString(),
                .readEndpoint = readEndpoint.toStdString(),
                .adapterContract = contract.toStdString(),
                .selfHosted = selfHosted.toBool(),
            });
        }
    }
    QString preferredWriteProviderText = optionValue(
        arguments, QStringLiteral("--preferred-write-provider-id="));
    if (preferredWriteProviderText.isEmpty()) {
        preferredWriteProviderText = qEnvironmentVariable(
            "GENYCASTER_PREFERRED_WRITE_PROVIDER_ID").trimmed();
    }
    if (preferredWriteProviderText.isEmpty()) {
        preferredWriteProviderText = settings.value(
            QStringLiteral("network/preferredWriteProviderId")).toString().trimmed();
    }
    if (!preferredWriteProviderText.isEmpty() && !validProviderId(preferredWriteProviderText)) {
        qCritical().noquote() << "Preferred write provider ID is invalid.";
        return 2;
    }
    const std::optional<std::string> preferredWriteProviderId =
        preferredWriteProviderText.isEmpty() ? std::nullopt
            : std::optional<std::string>{preferredWriteProviderText.toStdString()};

    QStringList nodeCandidateEndpoints{};
    const QString nodeCandidatesText =
        qEnvironmentVariable("GENYCASTER_HYPERSNAP_NODE_CANDIDATES").trimmed();
    if (!nodeCandidatesText.isEmpty()) {
        nodeCandidateEndpoints = nodeCandidatesText.split(
            QLatin1Char(','), Qt::SkipEmptyParts);
        for (QString& endpoint : nodeCandidateEndpoints) {
            endpoint = endpoint.trimmed();
        }
    }
    if (!providerBaseUrlText.isEmpty()) {
        nodeCandidateEndpoints.prepend(providerBaseUrlText);
    }
    // Provider-specific defaults belong to src/providers, never the QML/UI
    // composition root. A selected custom node still overrides the default.
    QString appliedNodeEndpoint = providerBaseUrlText;
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    const QString hypersnapDefaultEndpoint = QString::fromStdString(
        farcaster::provider::hypersnap::config::Config{}.baseUrl);
    nodeCandidateEndpoints.append(hypersnapDefaultEndpoint);
    if (appliedNodeEndpoint.isEmpty())
        appliedNodeEndpoint = hypersnapDefaultEndpoint;
#endif

    QString snapchainBaseUrlText = optionValue(arguments, QStringLiteral("--snapchain-base-url="));
    if (snapchainBaseUrlText.isEmpty()) {
        snapchainBaseUrlText = qEnvironmentVariable("GENYCASTER_SNAPCHAIN_BASE_URL").trimmed();
    }
    if (snapchainBaseUrlText.isEmpty()) {
        snapchainBaseUrlText = settings.value(
            QStringLiteral("network/snapchainReadBaseUrl")).toString().trimmed();
    }
    if (snapchainBaseUrlText.isEmpty()) {
        snapchainBaseUrlText = QString::fromStdString(
            std::string{deploymentEndpoints.snapchainBaseUrl});
    }
    snapchainBaseUrlText = normalizeConfigurationToken(std::move(snapchainBaseUrlText));
    if (!isValidHttpServiceUrl(snapchainBaseUrlText)) {
        qCritical().noquote() << "Snapchain read URL must be a credential-free HTTP(S) origin.";
        return 2;
    }
    const std::optional<std::string> snapchainBaseUrl{snapchainBaseUrlText.toStdString()};

    // A selected read-only Snapchain node cannot accept submissions. Configure
    // its validator/write route separately; with no override, preserve the
    // prior one-origin behavior for deployments whose node supports both.
    QString snapchainWriteBaseUrlText = optionValue(
        arguments, QStringLiteral("--snapchain-write-base-url="));
    if (snapchainWriteBaseUrlText.isEmpty()) {
        snapchainWriteBaseUrlText = qEnvironmentVariable(
            "GENYCASTER_SNAPCHAIN_WRITE_BASE_URL").trimmed();
    }
    if (snapchainWriteBaseUrlText.isEmpty()) {
        snapchainWriteBaseUrlText = settings.value(
            QStringLiteral("network/snapchainWriteBaseUrl")).toString().trimmed();
    }
    snapchainWriteBaseUrlText = normalizeConfigurationToken(
        std::move(snapchainWriteBaseUrlText));
    if (!snapchainWriteBaseUrlText.isEmpty()
        && !isValidHttpServiceUrl(snapchainWriteBaseUrlText)) {
        qCritical().noquote() << "Snapchain validator/write URL must be a credential-free HTTP(S) origin.";
        return 2;
    }
    const std::optional<std::string> snapchainWriteBaseUrl =
        snapchainWriteBaseUrlText.isEmpty()
            ? snapchainBaseUrl
            : std::optional<std::string>{snapchainWriteBaseUrlText.toStdString()};

    QString mediaGatewayBaseUrlText = optionValue(
        arguments, QStringLiteral("--media-gateway-base-url="));
    if (mediaGatewayBaseUrlText.isEmpty()) {
        mediaGatewayBaseUrlText =
            qEnvironmentVariable("GENYCASTER_MEDIA_GATEWAY_BASE_URL").trimmed();
    }
    if (mediaGatewayBaseUrlText.isEmpty()) {
        mediaGatewayBaseUrlText = QString::fromStdString(
            std::string{deploymentEndpoints.mediaGatewayBaseUrl});
    }
    mediaGatewayBaseUrlText = normalizeConfigurationToken(std::move(mediaGatewayBaseUrlText));
    if (!isValidHttpServiceUrl(mediaGatewayBaseUrlText)) {
        qCritical().noquote()
            << "GENYCASTER_MEDIA_GATEWAY_BASE_URL/--media-gateway-base-url must be a credential-free HTTP(S) origin.";
        return 2;
    }
    const std::optional<std::string> mediaGatewayBaseUrl{mediaGatewayBaseUrlText.toStdString()};

    QString snapchainAuthorizationHeaderText = optionValue(
        arguments, QStringLiteral("--snapchain-auth-header="));
    if (snapchainAuthorizationHeaderText.isEmpty()) {
        snapchainAuthorizationHeaderText =
            qEnvironmentVariable("GENYCASTER_SNAPCHAIN_AUTH_HEADER").trimmed();
    }
    QString snapchainAuthorizationValueText = optionValue(
        arguments, QStringLiteral("--snapchain-auth-value="));
    if (snapchainAuthorizationValueText.isEmpty()) {
        snapchainAuthorizationValueText =
            qEnvironmentVariable("GENYCASTER_SNAPCHAIN_AUTH_VALUE").trimmed();
    }
    const std::optional<std::string> snapchainAuthorizationHeader =
        snapchainAuthorizationHeaderText.isEmpty()
            ? std::nullopt
            : std::optional<std::string>{snapchainAuthorizationHeaderText.toStdString()};
    const std::optional<std::string> snapchainAuthorizationValue =
        snapchainAuthorizationValueText.isEmpty()
            ? std::nullopt
            : std::optional<std::string>{snapchainAuthorizationValueText.toStdString()};
    if (snapchainAuthorizationHeader.has_value() != snapchainAuthorizationValue.has_value()) {
        qCritical().noquote()
            << "Snapchain advanced authorization requires both --snapchain-auth-header/"
               "GENYCASTER_SNAPCHAIN_AUTH_HEADER and --snapchain-auth-value/"
               "GENYCASTER_SNAPCHAIN_AUTH_VALUE.";
        return 2;
    }

    QString snapchainWriteAuthorizationHeaderText = optionValue(
        arguments, QStringLiteral("--snapchain-write-auth-header="));
    if (snapchainWriteAuthorizationHeaderText.isEmpty()) {
        snapchainWriteAuthorizationHeaderText =
            qEnvironmentVariable("GENYCASTER_SNAPCHAIN_WRITE_AUTH_HEADER").trimmed();
    }
    QString snapchainWriteAuthorizationValueText = optionValue(
        arguments, QStringLiteral("--snapchain-write-auth-value="));
    if (snapchainWriteAuthorizationValueText.isEmpty()) {
        snapchainWriteAuthorizationValueText =
            qEnvironmentVariable("GENYCASTER_SNAPCHAIN_WRITE_AUTH_VALUE").trimmed();
    }
    const std::optional<std::string> snapchainWriteAuthorizationHeader =
        snapchainWriteAuthorizationHeaderText.isEmpty()
            ? std::nullopt
            : std::optional<std::string>{snapchainWriteAuthorizationHeaderText.toStdString()};
    const std::optional<std::string> snapchainWriteAuthorizationValue =
        snapchainWriteAuthorizationValueText.isEmpty()
            ? std::nullopt
            : std::optional<std::string>{snapchainWriteAuthorizationValueText.toStdString()};
    if (snapchainWriteAuthorizationHeader.has_value()
        != snapchainWriteAuthorizationValue.has_value()) {
        qCritical().noquote()
            << "Validator authorization requires both write-auth-header and write-auth-value.";
        return 2;
    }

    QString appFidText = optionValue(arguments, QStringLiteral("--app-fid="));
    if (appFidText.isEmpty()) {
        appFidText = qEnvironmentVariable("GENYCASTER_APP_FID").trimmed();
    }
    std::optional<std::uint64_t> appFid{};
    if (!appFidText.isEmpty()) {
        bool ok{false};
        const qulonglong parsed = appFidText.toULongLong(&ok);
        if (!ok || parsed == 0U) {
            qCritical().noquote() << "GENYCASTER_APP_FID/--app-fid must be a non-zero unsigned integer.";
            return 2;
        }
        appFid = static_cast<std::uint64_t>(parsed);
    }
    if (!appFid && deploymentIdentity.appFid != 0U) {
        appFid = deploymentIdentity.appFid;
    }

    QString signerAttestationUrlText = optionValue(
        arguments, QStringLiteral("--signer-attestation-url="));
    if (signerAttestationUrlText.isEmpty()) {
        signerAttestationUrlText =
            qEnvironmentVariable("GENYCASTER_SIGNER_ATTESTATION_URL").trimmed();
    }
    // Snapchain gateways implement signed-message submission, NOT App-FID
    // SignedKeyRequest attestation. Never infer a signing endpoint from the
    // public read/write gateway: it returns 404 route_not_available and cannot
    // produce a valid App-FID signature. Only an explicitly configured trusted
    // signer-attestation service may authorize a new protocol signer.
    const std::optional<std::string> signerAttestationUrl = signerAttestationUrlText.isEmpty()
        ? std::nullopt : std::optional<std::string>{signerAttestationUrlText.toStdString()};

    QString walletLaunchTemplate = optionValue(
        arguments, QStringLiteral("--wallet-launch-url-template="));
    if (walletLaunchTemplate.isEmpty()) {
        walletLaunchTemplate =
            qEnvironmentVariable("GENYCASTER_WALLET_LAUNCH_URL_TEMPLATE").trimmed();
    }

    const QString compiledSiwfDomain = normalizeConfigurationToken(
        QString::fromStdString(std::string{deploymentEndpoints.siwfDomain}));
    const QString compiledSiwfUri = normalizeConfigurationToken(
        QString::fromStdString(std::string{deploymentEndpoints.siwfUri}));

    QString siwfDomainOverride = optionValue(arguments, QStringLiteral("--siwf-domain="));
    const bool explicitSiwfDomainOverride = !siwfDomainOverride.isEmpty();
    if (siwfDomainOverride.isEmpty()) {
        siwfDomainOverride = qEnvironmentVariable("GENYCASTER_SIWF_DOMAIN").trimmed();
    }
    QString siwfUriOverride = optionValue(arguments, QStringLiteral("--siwf-uri="));
    const bool explicitSiwfUriOverride = !siwfUriOverride.isEmpty();
    if (siwfUriOverride.isEmpty()) {
        siwfUriOverride = qEnvironmentVariable("GENYCASTER_SIWF_URI").trimmed();
    }
    const bool hasRuntimeSiwfDomainOverride = !siwfDomainOverride.isEmpty();
    const bool hasRuntimeSiwfUriOverride = !siwfUriOverride.isEmpty();
    siwfDomainOverride = normalizeConfigurationToken(std::move(siwfDomainOverride));
    siwfUriOverride = normalizeConfigurationToken(std::move(siwfUriOverride));

    QString siwfDomainText = compiledSiwfDomain;
    QString siwfUriText = compiledSiwfUri;
    if (hasRuntimeSiwfDomainOverride && hasRuntimeSiwfUriOverride) {
        if (isValidSiwfDomain(siwfDomainOverride) && isValidSiwfUri(siwfUriOverride) &&
            siwfDomainMatchesUri(siwfDomainOverride, siwfUriOverride)) {
            siwfDomainText = std::move(siwfDomainOverride);
            siwfUriText = std::move(siwfUriOverride);
        } else if (explicitSiwfDomainOverride || explicitSiwfUriOverride) {
            qWarning().noquote()
                << "Invalid explicit SIWF override; using the compiled deployment identity.";
        }
    }

    if (!isValidSiwfDomain(siwfDomainText) || !isValidSiwfUri(siwfUriText) ||
        !siwfDomainMatchesUri(siwfDomainText, siwfUriText)) {
        qWarning().noquote()
            << "Sign in with Farcaster is disabled because the compiled SIWF deployment configuration is invalid.";
        siwfDomainText.clear();
        siwfUriText.clear();
    }
    QString authRelayUrlText = optionValue(arguments, QStringLiteral("--farcaster-auth-relay="));
    const bool explicitAuthRelayOverride = !authRelayUrlText.isEmpty();
    if (authRelayUrlText.isEmpty()) {
        authRelayUrlText = qEnvironmentVariable("GENYCASTER_FARCASTER_AUTH_RELAY").trimmed();
    }
    authRelayUrlText = normalizeConfigurationToken(std::move(authRelayUrlText));
    if (!authRelayUrlText.isEmpty() && !isValidHttpServiceUrl(authRelayUrlText)) {
        if (explicitAuthRelayOverride) {
            qWarning().noquote()
                << "Invalid explicit Farcaster authentication relay override; using the official relay default.";
        }
        authRelayUrlText.clear();
    }

    QString optimismRpcUrlText = optionValue(arguments, QStringLiteral("--optimism-rpc-url="));
    const bool explicitOptimismRpcOverride = !optimismRpcUrlText.isEmpty();
    if (optimismRpcUrlText.isEmpty()) {
        optimismRpcUrlText = qEnvironmentVariable("GENYCASTER_OPTIMISM_RPC_URL").trimmed();
    }
    optimismRpcUrlText = normalizeConfigurationToken(std::move(optimismRpcUrlText));
    if (!optimismRpcUrlText.isEmpty() && !isValidHttpServiceUrl(optimismRpcUrlText, true)) {
        if (explicitOptimismRpcOverride) {
            qWarning().noquote()
                << "Invalid explicit Optimism RPC override; using the built-in public RPC default.";
        }
        optimismRpcUrlText.clear();
    }

    const std::optional<std::string> authRelayUrl = authRelayUrlText.isEmpty()
        ? std::nullopt : std::optional<std::string>{authRelayUrlText.toStdString()};
    const std::optional<std::string> optimismRpcUrl = optimismRpcUrlText.isEmpty()
        ? std::nullopt : std::optional<std::string>{optimismRpcUrlText.toStdString()};
    const std::string siwfDomain = siwfDomainText.toStdString();
    const std::string siwfUri = siwfUriText.toStdString();
#if defined(__EMSCRIPTEN__)
    const bool authenticationAvailable = true; // OpenProof browser device OAuth, read-only session.
#elif GENYCASTER_HAS_OPENPROOF_AUTH
    const bool authenticationAvailable = true;
#elif FARCASTER_HAS_FARCASTER_AUTH
    const bool authenticationAvailable = !siwfDomain.empty() && !siwfUri.empty();
#else
    const bool authenticationAvailable = false;
#endif

    // Optional development/demo identity. Normal guest startup never restores a
    // previously browsed identity as if it were the current account.
    QString startupIdentity = optionValue(arguments, QStringLiteral("--auto-open-identity="));
    if (startupIdentity.isEmpty()) {
        startupIdentity = qEnvironmentVariable("GENYCASTER_DEMO_IDENTITY").trimmed();
    }

    LocalizationController::StartupOverrides localizationOverrides{
        .languageTag = optionValue(arguments, QStringLiteral("--language=")),
        .regionalLocaleTag = optionValue(arguments, QStringLiteral("--regional-locale=")),
        .calendarId = optionValue(arguments, QStringLiteral("--calendar=")),
        .timestampDisplayMode = optionValue(arguments, QStringLiteral("--timestamp-display=")),
        .timeZoneId = optionValue(arguments, QStringLiteral("--time-zone=")),
        .currencyCode = optionValue(arguments, QStringLiteral("--currency=")),
        .additionalCurrencyCodes = {},
    };
    if (localizationOverrides.languageTag.isEmpty()) {
        localizationOverrides.languageTag = qEnvironmentVariable("GENYCASTER_LANGUAGE").trimmed();
    }
    if (localizationOverrides.regionalLocaleTag.isEmpty()) {
        localizationOverrides.regionalLocaleTag = qEnvironmentVariable("GENYCASTER_REGIONAL_LOCALE").trimmed();
    }
    if (localizationOverrides.calendarId.isEmpty()) {
        localizationOverrides.calendarId = qEnvironmentVariable("GENYCASTER_CALENDAR").trimmed();
    }
    if (localizationOverrides.timestampDisplayMode.isEmpty()) {
        localizationOverrides.timestampDisplayMode = qEnvironmentVariable("GENYCASTER_TIMESTAMP_DISPLAY").trimmed();
    }
    if (localizationOverrides.timeZoneId.isEmpty()) {
        localizationOverrides.timeZoneId = qEnvironmentVariable("GENYCASTER_TIME_ZONE").trimmed();
    }
    if (localizationOverrides.currencyCode.isEmpty()) {
        localizationOverrides.currencyCode = qEnvironmentVariable("GENYCASTER_CURRENCY").trimmed();
    }
    QString additionalCurrencies = optionValue(arguments, QStringLiteral("--additional-currencies="));
    if (additionalCurrencies.isEmpty()) {
        additionalCurrencies = qEnvironmentVariable("GENYCASTER_ADDITIONAL_CURRENCIES").trimmed();
    }
    if (!additionalCurrencies.isEmpty()) {
        localizationOverrides.additionalCurrencyCodes = additionalCurrencies.split(
            QLatin1Char(','), Qt::SkipEmptyParts);
    }

    AppearanceController::StartupOverrides appearanceOverrides{
        .themeMode = optionValue(arguments, QStringLiteral("--theme-mode=")),
        .colorThemeId = optionValue(arguments, QStringLiteral("--color-theme=")),
        .reduceMotion = optionValue(arguments, QStringLiteral("--reduce-motion=")),
    };
    if (appearanceOverrides.themeMode.isEmpty()) {
        appearanceOverrides.themeMode = qEnvironmentVariable("GENYCASTER_THEME_MODE").trimmed();
    }
    if (appearanceOverrides.colorThemeId.isEmpty()) {
        appearanceOverrides.colorThemeId = qEnvironmentVariable("GENYCASTER_COLOR_THEME").trimmed();
    }
    if (appearanceOverrides.reduceMotion.isEmpty()) {
        appearanceOverrides.reduceMotion = qEnvironmentVariable("GENYCASTER_REDUCE_MOTION").trimmed();
    }

    // The context objects must outlive QQmlApplicationEngine. During orderly
    // teardown the engine reevaluates/destroys QML bindings; destroying these
    // objects first turns every context-property access into a spurious null
    // TypeError. Declare the owners before the engine so reverse destruction
    // order tears the QML engine down first.
    std::unique_ptr<AppearanceController> appearanceControllerOwner{};
    std::unique_ptr<RenderingController> renderingControllerOwner{};
    std::unique_ptr<TextSelectionSettings> textSelectionSettingsOwner{};
    std::unique_ptr<LocalizationController> localizationControllerOwner{};
    std::unique_ptr<PresentationBridge> presentationBridgeOwner{};
    std::unique_ptr<DataStorageController> dataStorageControllerOwner{};
    std::unique_ptr<RichComposerController> richComposerControllerOwner{};
    std::unique_ptr<EmojiController> emojiControllerOwner{};
    std::unique_ptr<farcaster::runtime::emoji::EmojiRuntime> emojiRuntimeOwner{};
    std::unique_ptr<MediaUploadController> mediaUploadControllerOwner{};
    std::unique_ptr<MediaClassifierController> mediaClassifierControllerOwner{};
    std::unique_ptr<MediaPlaybackCoordinator> mediaPlaybackCoordinatorOwner{};
    std::unique_ptr<LiveInsightsController> liveInsightsControllerOwner{};
    std::unique_ptr<NodeSelectionController> nodeSelectionControllerOwner{};
    std::unique_ptr<ProviderProfilesController> providerProfilesControllerOwner{};
    std::unique_ptr<UpdateController> updateControllerOwner{};
    std::unique_ptr<MiniAppHostController> miniAppHostControllerOwner{};
    std::unique_ptr<MediaWebHostController> mediaWebHostControllerOwner{};
    std::unique_ptr<MiniAppCatalogController> miniAppCatalogControllerOwner{};
    std::unique_ptr<ChannelPickerController> channelPickerControllerOwner{};
    std::unique_ptr<MiniAppTaskManagerController> miniAppTaskManagerControllerOwner{};
#if FARCASTER_HAS_MINIAPP_EVM_WALLET
    std::unique_ptr<farcaster::capability::wallet::IEthereumWalletBridge> miniAppWalletBridgeOwner{};
#endif
#if GENYCASTER_HAS_MINIAPP_HOST
    std::unique_ptr<farcaster::runtime::miniapp::MiniAppRuntime> miniAppRuntimeOwner{};
#endif

    // Qt's generated static module registration for the executable (render
    // C++ types such as SceneGraphSuperellipseItem) must be primed before the
    // engine imports Farcaster.Client. The QQmlModuleRegistration static
    // initializer alone is not enough when the module also ships as a resource
    // qmldir.
    extern void qml_register_types_Farcaster_Client();
    qml_register_types_Farcaster_Client();
    qmlRegisterType<RichInlineTextItem>(
        "Farcaster.Client", 1, 0, "RichInlineTextItem");

    QQmlApplicationEngine engine{};

    appearanceControllerOwner = std::make_unique<AppearanceController>(
        settings, std::move(appearanceOverrides));
    AppearanceController& appearanceController = *appearanceControllerOwner;
#if defined(__EMSCRIPTEN__)
    // Safari/Brave use the HTML document theme for their browser chrome,
    // independent of Qt Quick's canvas. Keep it in sync with the effective
    // app appearance (including manual overrides and system changes).
    const auto updateBrowserChromeTheme = [&appearanceController] {
        EM_ASM({
            if (typeof window.__genycasterSetBrowserChromeTheme === "function") {
                window.__genycasterSetBrowserChromeTheme($0 !== 0);
            }
        }, appearanceController.effectiveDark() ? 1 : 0);
    };
    updateBrowserChromeTheme();
    QObject::connect(&appearanceController, &AppearanceController::appearanceChanged,
                     &application, updateBrowserChromeTheme);
#endif
    renderingControllerOwner = std::make_unique<RenderingController>(settings);
    RenderingController& renderingController = *renderingControllerOwner;
    textSelectionSettingsOwner = std::make_unique<TextSelectionSettings>(settings);
    TextSelectionSettings& textSelectionSettings = *textSelectionSettingsOwner;

    try {
        localizationControllerOwner = std::make_unique<LocalizationController>(
            engine, settings, std::move(localizationOverrides));
    } catch (const std::exception& error) {
        qCritical().noquote() << "Unable to initialize Genycaster localization:" << error.what();
        return 2;
    }
    LocalizationController& localizationController = *localizationControllerOwner;

    nodeSelectionControllerOwner = std::make_unique<NodeSelectionController>(
        settings,
        appliedNodeEndpoint,
        QString::fromStdString(snapchainWriteBaseUrl.value_or(std::string{})),
        nodeCandidateEndpoints,
        networkProxyText,
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest);
    NodeSelectionController& nodeSelectionController = *nodeSelectionControllerOwner;
    providerProfilesControllerOwner = std::make_unique<ProviderProfilesController>(
        settings, providerProfilesJson, writeProfilesJson,
        preferredReadProviderText, preferredWriteProviderText,
        allowCompatibleReadFallback, nullptr, appliedNodeEndpoint,
        farcasterReadBaseUrlText);
    ProviderProfilesController& providerProfilesController = *providerProfilesControllerOwner;
    updateControllerOwner = std::make_unique<UpdateController>(
        settings, !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest, &application);
    UpdateController& updateController = *updateControllerOwner;
    const auto updateReleaseLocale = [&] {
        updateController.setLanguageTag(localizationController.effectiveLanguageTag());
    };
    updateReleaseLocale();
    QObject::connect(&localizationController, &LocalizationController::localeChanged,
                     &application, updateReleaseLocale);

    const auto applicationFontFamilyForLanguage = [&](QString languageTag) -> QString {
        languageTag = languageTag.trimmed().toLower();
        if (languageTag == QStringLiteral("fa") || languageTag == QStringLiteral("ar")) {
            return !bundledPersianFontFamily.isEmpty() ? bundledPersianFontFamily : platformRtlFontFamily;
        }
        if (languageTag == QStringLiteral("hi")) {
            return !bundledDevanagariFontFamily.isEmpty() ? bundledDevanagariFontFamily : platformDevanagariFontFamily;
        }
        if (languageTag == QStringLiteral("ja")) {
            return !bundledJapaneseFontFamily.isEmpty() ? bundledJapaneseFontFamily : platformJapaneseFontFamily;
        }
        if (languageTag == QStringLiteral("ko")) {
            return !bundledKoreanFontFamily.isEmpty() ? bundledKoreanFontFamily : platformKoreanFontFamily;
        }
        if (languageTag == QStringLiteral("zh")) {
            return !bundledChineseFontFamily.isEmpty() ? bundledChineseFontFamily : platformChineseFontFamily;
        }
        return platformTextFontFamily;
    };
    const auto applyApplicationLocaleFont = [&] {
        QFont localeFont = application.font();
        localeFont.setFamily(applicationFontFamilyForLanguage(localizationController.effectiveLanguageTag()));
        application.setFont(localeFont);
    };
    applyApplicationLocaleFont();
    QObject::connect(
        &localizationController, &LocalizationController::localeChanged,
        &application, applyApplicationLocaleFont);
#if defined(__EMSCRIPTEN__)
    // Keep the phrase-import DOM sheet in the same UI language as Qt/QML,
    // including live changes in the Genycaster language selector. This passes
    // only a public locale tag; recovery words never cross this bridge.
    const auto updateBrowserRecoveryLocale = [&localizationController] {
        const QByteArray language = localizationController.effectiveLanguageTag().toUtf8();
        EM_ASM({
            const tag = UTF8ToString($0, $1);
            window.__genycasterPreferredLanguage = tag;
            if (typeof window.__genycasterSetUpdateLanguage === "function")
                window.__genycasterSetUpdateLanguage(tag);
        }, language.constData(), language.size());
    };
    updateBrowserRecoveryLocale();
    QObject::connect(
        &localizationController, &LocalizationController::localeChanged,
        &application, updateBrowserRecoveryLocale);
#endif

    GeoLocationResolver geoLocationResolver{
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct};
    BrowserTokenPreviewController browserTokenPreviewController{
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest};
    BrowserLinkMetadataController browserLinkMetadataController{
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
#if defined(__EMSCRIPTEN__)
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest
#else
        false
#endif
    };
    LinkPreviewResolver linkPreviewResolver{
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct};
    CastViewController castViewController{
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
        GENYCASTER_CAST_VIEWS_ENABLED != 0
            && !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest};
    BrowserReactionController browserReactionController{
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
#if defined(__EMSCRIPTEN__)
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest
#else
        false
#endif
    };
    liveInsightsControllerOwner = std::make_unique<LiveInsightsController>(
        settings,
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest);
    LiveInsightsController& liveInsightsController = *liveInsightsControllerOwner;

    // Construct the bridge only after the translation catalog is active because
    // its initial user-visible status strings are ID-based translations.
    presentationBridgeOwner = std::make_unique<PresentationBridge>(
        nullptr, &settings, QStringLiteral("hypersnap"), QStringLiteral("farcaster"));
    PresentationBridge& presentationBridge = *presentationBridgeOwner;
    presentationBridge.setFarcasterReadConfigured(farcasterReadBaseUrl.has_value());
    presentationBridge.setFarcasterReadEndpoint(farcasterReadBaseUrlText);

    dataStorageControllerOwner = std::make_unique<DataStorageController>(settings);
    DataStorageController& dataStorageController = *dataStorageControllerOwner;

#if defined(__EMSCRIPTEN__) || defined(Q_OS_ANDROID)
    // Browser/WASM and Android host public web content without exposing
    // signer or wallet permissions. macOS separately enables its SDK runtime.
    constexpr bool miniAppLaunchAvailable = true;
#else
    constexpr bool miniAppLaunchAvailable = GENYCASTER_HAS_MINIAPP_HOST != 0;
#endif
    miniAppHostControllerOwner = std::make_unique<MiniAppHostController>(
        miniAppLaunchAvailable, settings);
    MiniAppHostController& miniAppHost = *miniAppHostControllerOwner;
    mediaWebHostControllerOwner = std::make_unique<MediaWebHostController>(
        GENYCASTER_HAS_MINIAPP_HOST != 0, QUrl{siwfUriText});
    MediaWebHostController& mediaWebHost = *mediaWebHostControllerOwner;

    const QByteArray configuredMiniAppDiscoveryApiKey =
        qEnvironmentVariable("GENYCASTER_NEYNAR_API_KEY").trimmed().toUtf8();
    const bool usingNeynarDocsAccess = configuredMiniAppDiscoveryApiKey.isEmpty();
    const QByteArray miniAppDiscoveryApiKey = usingNeynarDocsAccess
        ? QByteArrayLiteral("NEYNAR_API_DOCS")
        : configuredMiniAppDiscoveryApiKey;
    MiniAppCatalogEndpoint primaryMiniAppCatalogue{
        .catalogueUrl = QStringLiteral(
            "https://api.neynar.com/v2/farcaster/frame/catalog/?limit=100&time_window=7d"),
        .searchUrl = QStringLiteral("https://api.neynar.com/v2/farcaster/frame/search/"),
        .relevantUrl = QStringLiteral("https://api.neynar.com/v2/farcaster/frame/relevant/?time_window=7d"),
        .authorizationHeaderName = QByteArrayLiteral("x-api-key"),
        .authorizationHeaderValue = miniAppDiscoveryApiKey,
        .providerLabel = usingNeynarDocsAccess
            ? QStringLiteral("Neynar Mini Apps · docs access")
            : QStringLiteral("Neynar Mini Apps"),
        .format = MiniAppCatalogFormat::Json,
    };
    MiniAppCatalogEndpoint fallbackMiniAppCatalogue{
        .catalogueUrl = QStringLiteral("https://api.farcaster.xyz/v1/developer-rewards-winner-history?limit=100"),
        .providerLabel = QStringLiteral("Farcaster public developer index"),
        .format = MiniAppCatalogFormat::Json,
    };
#if defined(__EMSCRIPTEN__)
    // Keep upstream API credentials out of browser binaries and avoid CORS.
    // The first-party gateway only exposes bounded public catalogue reads.
    primaryMiniAppCatalogue = MiniAppCatalogEndpoint{
        .catalogueUrl = QStringLiteral("https://storage-farcaster.genyleap.com/v1/browser/miniapps?kind=catalog"),
        .searchUrl = QStringLiteral("https://storage-farcaster.genyleap.com/v1/browser/miniapps?kind=search"),
        .relevantUrl = QStringLiteral("https://storage-farcaster.genyleap.com/v1/browser/miniapps?kind=relevant"),
        .providerLabel = QStringLiteral("Genycaster public Mini Apps catalogue"),
        .format = MiniAppCatalogFormat::Json,
    };
    fallbackMiniAppCatalogue = MiniAppCatalogEndpoint{};
#endif
    miniAppCatalogControllerOwner = std::make_unique<MiniAppCatalogController>(
        settings,
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
        std::move(primaryMiniAppCatalogue),
        std::move(fallbackMiniAppCatalogue),
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest);
    MiniAppCatalogController& miniAppCatalog = *miniAppCatalogControllerOwner;
    // Optional provider-neutral presentation capability, independent of the
    // user's selected read node. The image API has no browser CORS grant,
    // so WASM needs a deployment-owned, same-origin read-only relay.
    QString channelDirectoryBaseUrl = optionValue(
        arguments, QStringLiteral("--channel-directory-base-url="));
    if (channelDirectoryBaseUrl.isEmpty())
        channelDirectoryBaseUrl =
            qEnvironmentVariable("GENYCASTER_CHANNEL_DIRECTORY_BASE_URL").trimmed();
    if (channelDirectoryBaseUrl.isEmpty())
        channelDirectoryBaseUrl = settings.value(
            QStringLiteral("network/channelDirectoryBaseUrl")).toString().trimmed();
#if defined(__EMSCRIPTEN__)
    if (channelDirectoryBaseUrl.isEmpty()) {
        // Discover this optional capability from the configured first-party
        // gateway, not an additional hard-coded host. A custom deployment or
        // self-hosted instance retains its own gateway origin.
        if (!mediaGatewayBaseUrlText.isEmpty()) {
            channelDirectoryBaseUrl = mediaGatewayBaseUrlText;
            while (channelDirectoryBaseUrl.endsWith(QLatin1Char('/')))
                channelDirectoryBaseUrl.chop(1);
            channelDirectoryBaseUrl += QStringLiteral("/v1/browser/channels");
        }
    }
#else
    if (channelDirectoryBaseUrl.isEmpty())
        channelDirectoryBaseUrl = QStringLiteral("https://client.warpcast.com/v2");
#endif
    if (!channelDirectoryBaseUrl.isEmpty()
        && !isValidHttpServiceUrl(channelDirectoryBaseUrl))
        channelDirectoryBaseUrl.clear();
    channelPickerControllerOwner = std::make_unique<ChannelPickerController>(
        settings, appliedNodeEndpoint, channelDirectoryBaseUrl,
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest);
    ChannelPickerController& channelPickerController = *channelPickerControllerOwner;
    miniAppTaskManagerControllerOwner = std::make_unique<MiniAppTaskManagerController>();
    MiniAppTaskManagerController& miniAppTaskManager = *miniAppTaskManagerControllerOwner;

    miniAppHost.setReduceMotion(appearanceController.reduceMotion());
    QObject::connect(&appearanceController, &AppearanceController::appearanceChanged, &miniAppHost,
        [&appearanceController, &miniAppHost] {
            miniAppHost.setReduceMotion(appearanceController.reduceMotion());
        });
    mediaWebHost.setReduceMotion(appearanceController.reduceMotion());
    QObject::connect(&appearanceController, &AppearanceController::appearanceChanged, &mediaWebHost,
        [&appearanceController, &mediaWebHost] {
            mediaWebHost.setReduceMotion(appearanceController.reduceMotion());
        });
#if FARCASTER_HAS_MINIAPP_EVM_WALLET
    {
        auto walletBridge = farcaster::provider::compatibility::browser_wallet::bridge::makeBridge({
            .launch = makeWalletApprovalLauncher(walletLaunchTemplate),
            .supportedChains = miniAppEthereumChains(),
        });
        if (walletBridge) {
            miniAppWalletBridgeOwner = std::move(*walletBridge);
        } else {
            qWarning().noquote() << "Mini App EIP-1193 wallet bridge unavailable:"
                                 << QString::fromStdString(walletBridge.error().message);
        }
    }
#endif
#if GENYCASTER_HAS_MINIAPP_HOST
    miniAppRuntimeOwner = std::make_unique<farcaster::runtime::miniapp::MiniAppRuntime>(
        miniAppHost, miniAppCatalog, miniAppTaskManager,
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
        static_cast<qulonglong>(appFid.value_or(0))
#if FARCASTER_HAS_MINIAPP_EVM_WALLET
        , miniAppWalletBridgeOwner.get()
#else
        , nullptr
#endif
    );
#endif
    const auto updateMiniAppUser = [&presentationBridge, &miniAppHost] {
        QVariantMap user;
        if (presentationBridge.isAuthenticated() && presentationBridge.accountFid() > 0
            && presentationBridge.accountFid() <= 9007199254740991ULL) {
            user = {{QStringLiteral("fid"), presentationBridge.accountFid()},
                    {QStringLiteral("username"), presentationBridge.accountUsername()},
                    {QStringLiteral("displayName"), presentationBridge.accountName()},
                    {QStringLiteral("pfpUrl"), presentationBridge.accountAvatarUrl()}};
        }
        miniAppHost.setUser(user);
    };
    QObject::connect(&presentationBridge, &PresentationBridge::stateChanged,
        &miniAppHost, updateMiniAppUser);
    updateMiniAppUser();
    QObject::connect(&application, &QCoreApplication::aboutToQuit, &miniAppHost, &MiniAppHostController::closeAll);
    QObject::connect(&presentationBridge, &PresentationBridge::embeddedMediaRequested,
        &mediaWebHost, [&presentationBridge, &mediaWebHost](const QString& url, const QString& title) {
            if (!mediaWebHost.open(url, title)) {
                presentationBridge.openExternalUrl(url);
            }
        });
    QObject::connect(&application, &QCoreApplication::aboutToQuit,
        &mediaWebHost, &MediaWebHostController::close);

    qulonglong notificationSettingsFid = 0;
    bool notificationBadgeBootstrapped = false;
    QObject::connect(&presentationBridge, &PresentationBridge::notificationSeenAtChanged,
        &presentationBridge, [&settings](const qulonglong fid, const qlonglong observedAtMs) {
            if (fid == 0) {
                return;
            }
            settings.setValue(
                QStringLiteral("notifications/seen_at/%1").arg(fid),
                observedAtMs);
            settings.sync();
        });
    QObject::connect(&application, &QGuiApplication::applicationStateChanged,
        &presentationBridge, [&presentationBridge](const Qt::ApplicationState state) {
            if (state != Qt::ApplicationActive || !presentationBridge.isAuthenticated()) {
                return;
            }
            // Refresh after focus settles so returning from sleep/background does not
            // leave a stale navigation badge for another polling interval.
            QTimer::singleShot(250, &presentationBridge, [&presentationBridge] {
                presentationBridge.refreshNotificationBadge();
            });
        });
    QObject::connect(&presentationBridge, &PresentationBridge::stateChanged,
        &presentationBridge,
        [&presentationBridge, &settings, &notificationSettingsFid, &notificationBadgeBootstrapped] {
            const qulonglong fid = presentationBridge.isAuthenticated()
                ? presentationBridge.accountFid() : 0;
            if (fid != notificationSettingsFid) {
                notificationSettingsFid = fid;
                notificationBadgeBootstrapped = false;
                if (fid != 0) {
                    presentationBridge.restoreNotificationSeenAt(
                        fid,
                        settings.value(
                            QStringLiteral("notifications/seen_at/%1").arg(fid),
                            0).toLongLong());
                }
            }
            if (fid != 0 && presentationBridge.runtimeReady() && !notificationBadgeBootstrapped) {
                notificationBadgeBootstrapped = true;
                QTimer::singleShot(750, &presentationBridge, [&presentationBridge] {
                    presentationBridge.refreshNotificationBadge();
                });
            }
        });


    richComposerControllerOwner = std::make_unique<RichComposerController>(settings);
    RichComposerController& richComposerController = *richComposerControllerOwner;
    emojiControllerOwner = std::make_unique<EmojiController>(settings);
    EmojiController& emojiController = *emojiControllerOwner;
    emojiRuntimeOwner = std::make_unique<farcaster::runtime::emoji::EmojiRuntime>(emojiController);
    mediaUploadControllerOwner = std::make_unique<MediaUploadController>();
    MediaUploadController& mediaUploadController = *mediaUploadControllerOwner;
    mediaClassifierControllerOwner = std::make_unique<MediaClassifierController>();
    MediaClassifierController& mediaClassifier = *mediaClassifierControllerOwner;
    mediaPlaybackCoordinatorOwner = std::make_unique<MediaPlaybackCoordinator>(settings);
    MediaPlaybackCoordinator& mediaPlayback = *mediaPlaybackCoordinatorOwner;
    // Connections in GenyVideoPlayer retain this context object as a signal
    // target. Keep its lifetime unequivocally C++-owned across QML teardown.
    QQmlEngine::setObjectOwnership(&mediaPlayback, QQmlEngine::CppOwnership);
    QObject::connect(&localizationController, &LocalizationController::localeChanged,
        &presentationBridge, [&presentationBridge] {
            presentationBridge.retranslateTransientState();
        });
    if (!isSmokeTest && !isVisualTest) {
        QNetworkInformation::loadDefaultBackend();
        if (auto* networkInformation = QNetworkInformation::instance()) {
            const auto initialReachability = networkInformation->reachability();
            const auto applyReachability = [&presentationBridge](const QNetworkInformation::Reachability reachability) {
                const bool known = reachability != QNetworkInformation::Reachability::Unknown;
                const bool online = reachability != QNetworkInformation::Reachability::Disconnected;
                presentationBridge.applyNetworkReachability(known, online);
            };
            applyReachability(initialReachability);
            QObject::connect(networkInformation, &QNetworkInformation::reachabilityChanged,
                &presentationBridge,
                [&presentationBridge, previousReachability = initialReachability, applyReachability]
                (const QNetworkInformation::Reachability reachability) mutable {
                    const bool reconnected = previousReachability == QNetworkInformation::Reachability::Disconnected
                        && reachability != QNetworkInformation::Reachability::Disconnected
                        && reachability != QNetworkInformation::Reachability::Unknown;
                    previousReachability = reachability;
                    applyReachability(reachability);
                    if (reconnected && presentationBridge.runtimeReady() && !presentationBridge.isBusy()) {
                        QTimer::singleShot(350, &presentationBridge, [&presentationBridge] {
                            if (!presentationBridge.isBusy() && !presentationBridge.isOffline()) {
                                presentationBridge.retryAutomatically();
                            }
                        });
                    }
                });
        } else {
            presentationBridge.applyNetworkReachability(false, true);
        }

        auto* const sourceRecoveryTimer = new QTimer(&presentationBridge);
        sourceRecoveryTimer->setInterval(35'000);
        QObject::connect(sourceRecoveryTimer, &QTimer::timeout, &presentationBridge,
            [&presentationBridge] {
                if (!presentationBridge.runtimeReady() || presentationBridge.isBusy() ||
                    presentationBridge.isOffline() || !presentationBridge.sourceUnavailable()) {
                    return;
                }
                presentationBridge.retryAutomatically();
            });
        sourceRecoveryTimer->start();
    }
    if (isSmokeTest || isVisualTest) {
        const bool showInitialSetup =
            isVisualTest && arguments.contains(QStringLiteral("--open-initial-setup"));
        if (showInitialSetup) {
            presentationBridge.applyRuntimeState(true);
            presentationBridge.applyAuthenticationState(
                true,
                QStringLiteral("signed-out"),
                0,
                {},
                {},
                {},
                {},
                {},
                {});
        } else {
            presentationBridge.applyVerificationFixture();
        }
        if (isVisualTest && arguments.contains(QStringLiteral("--open-profile"))) {
            applyProfileVerificationFixture(presentationBridge);
        }
    }
#if defined(Q_OS_ANDROID)
    // Android GPUs may render Qt Quick MultiEffect colorization as a filled
    // rectangle instead of preserving each SVG's alpha mask. Rasterize the
    // approved local Genyleap icon SVG with QtSvg and tint via SourceIn,
    // avoiding a second, device-dependent postprocessing shader pass.
    engine.addImageProvider(QStringLiteral("genycaster-icons"),
                            new AndroidTintedIconProvider{});
#endif
    engine.addImageProvider(
        QStringLiteral("remote"),
        new RemoteImageProvider{
            networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
            dataStorageController.runtimeStateHandle()});
    engine.rootContext()->setContextProperty(QStringLiteral("platformTextFontFamily"), platformTextFontFamily);
    engine.rootContext()->setContextProperty(QStringLiteral("platformRtlFontFamily"), platformRtlFontFamily);
    engine.rootContext()->setContextProperty(QStringLiteral("platformDevanagariFontFamily"), platformDevanagariFontFamily);
    engine.rootContext()->setContextProperty(QStringLiteral("platformJapaneseFontFamily"), platformJapaneseFontFamily);
    engine.rootContext()->setContextProperty(QStringLiteral("platformKoreanFontFamily"), platformKoreanFontFamily);
    engine.rootContext()->setContextProperty(QStringLiteral("platformChineseFontFamily"), platformChineseFontFamily);
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledPersianFontFamily"), bundledPersianFontFamily);
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledDevanagariFontFamily"), bundledDevanagariFontFamily);
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledJapaneseFontFamily"), bundledJapaneseFontFamily);
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledKoreanFontFamily"), bundledKoreanFontFamily);
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledChineseFontFamily"), bundledChineseFontFamily);
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledIconFontFamily"), bundledIconFontFamily);
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledIconRegularFontFamily"), bundledIconRegularFontFamily);
    GenyPulseActivityController genyPulseActivity;
    engine.rootContext()->setContextProperty(
        QStringLiteral("genyPulseActivity"), &genyPulseActivity);
    OpenProofConnectionsController openProofConnections{nullptr, &settings};
    engine.rootContext()->setContextProperty(
        QStringLiteral("openProofConnections"), &openProofConnections);
    // The market cap follows a server-verified GENY balance, never the
    // client's public market-info estimate or a QML-supplied wallet.
    QObject::connect(&openProofConnections,
                     &OpenProofConnectionsController::proEntitlementsChanged,
                     &liveInsightsController, [&openProofConnections, &liveInsightsController] {
        const auto status = openProofConnections.proEntitlements();
        const bool pro = status.value(QStringLiteral("status")).toString()
                             == QStringLiteral("verified")
                      && status.value(QStringLiteral("eligible")).toBool();
        liveInsightsController.setProMarketLimit(pro ? 16 : 4);
    });
    QObject::connect(&openProofConnections,
                     &OpenProofConnectionsController::stateChanged,
                     &openProofConnections, [&openProofConnections] {
        if (openProofConnections.cloudAuthorizationReady())
            openProofConnections.fetchProEntitlements();
    });
    QObject::connect(&presentationBridge, &PresentationBridge::authenticationStateChanged,
        &openProofConnections, [&presentationBridge, &openProofConnections, &richComposerController] {
            const auto fid = presentationBridge.isAuthenticated()
                ? presentationBridge.accountFid() : 0;
            // Account-scoped unpublished drafts must never carry over when the
            // active Farcaster identity changes on a shared device.
            richComposerController.setViewerFid(fid);
            openProofConnections.setViewerFid(fid);
        });
    const auto initialFid = presentationBridge.isAuthenticated()
        ? presentationBridge.accountFid() : 0;
    richComposerController.setViewerFid(initialFid);
    openProofConnections.setViewerFid(initialFid);
    QObject::connect(&richComposerController, &RichComposerController::draftLocallyChanged,
                     &openProofConnections,
                     [&openProofConnections](const qulonglong fid, const bool deleted) {
        openProofConnections.recordDraftMutation(fid, deleted);
    });
    QObject::connect(&openProofConnections,
                     &OpenProofConnectionsController::cloudDraftSnapshotReceived,
                     &richComposerController,
                     [&richComposerController, &openProofConnections](
                         const qulonglong fid, const QByteArray& document,
                         const bool deleted, const int revision) {
        if (richComposerController.applyAccountDraft(fid, document, deleted))
            openProofConnections.confirmDraftSnapshot(fid, revision);
    });
    // User actions update the local reading list immediately; Genycaster
    // propagates owner-scoped mutations through its durable outbox.
    QObject::connect(&presentationBridge, &PresentationBridge::bookmarkMutated,
                     &openProofConnections,
                     [&openProofConnections,&presentationBridge](
                         const QString& castId, const bool deleted) {
        openProofConnections.recordBookmarkMutation(
            castId,deleted,deleted ? QVariantMap{} : presentationBridge.bookmarkEntry(castId));
    });
    QObject::connect(&openProofConnections,
                     &OpenProofConnectionsController::cloudBookmarkChangesReceived,
                     &presentationBridge,
                     [&presentationBridge,&openProofConnections](
                         const QVariantList& changes) {
        presentationBridge.applyCloudBookmarkChanges(changes);
        if (!openProofConnections.bookmarkPreviewSupported())return;
        for(const auto& value:changes) {
            const auto row=value.toMap();
            if(row.value(QStringLiteral("is_deleted")).toBool()
               || !row.value(QStringLiteral("preview")).toMap().isEmpty())
                continue;
            const auto id=row.value(QStringLiteral("cast_hash")).toString();
            openProofConnections.publishBookmarkPreviewIfMissing(
                id,presentationBridge.bookmarkEntry(id));
        }
    });
    QObject::connect(&presentationBridge, &PresentationBridge::notificationSeenAtChanged,
                     &openProofConnections,
                     [&openProofConnections](const qulonglong fid, const qlonglong seenAtMs) {
        openProofConnections.recordNotificationSeenAt(fid,seenAtMs);
    });
    QObject::connect(&openProofConnections,
                     &OpenProofConnectionsController::cloudNotificationSeenAtReceived,
                     &presentationBridge,
                     [&presentationBridge](const qulonglong fid, const qlonglong seenAtMs) {
        if (presentationBridge.isAuthenticated() && fid==presentationBridge.accountFid())
            presentationBridge.restoreNotificationSeenAt(fid,seenAtMs);
    });
    const auto migrateExistingBookmarks = [&presentationBridge, &openProofConnections] {
        if (presentationBridge.isAuthenticated()
            && openProofConnections.cloudServiceAvailable()
            && openProofConnections.cloudAuthorizationReady())
            openProofConnections.migrateLocalBookmarks(presentationBridge.bookmarkedCastIds());
    };
    QObject::connect(&openProofConnections, &OpenProofConnectionsController::cloudStateChanged,
                     &presentationBridge, migrateExistingBookmarks);
    QObject::connect(&openProofConnections, &OpenProofConnectionsController::stateChanged,
                     &presentationBridge, migrateExistingBookmarks);
    engine.rootContext()->setContextProperty(QStringLiteral("presentationBridge"), &presentationBridge);
    engine.rootContext()->setContextProperty(QStringLiteral("channelDirectory"), &channelPickerController);
    engine.rootContext()->setContextProperty(QStringLiteral("dataStorage"), &dataStorageController);
    engine.rootContext()->setContextProperty(QStringLiteral("richComposer"), &richComposerController);
    engine.rootContext()->setContextProperty(QStringLiteral("emojiController"), &emojiController);
    engine.rootContext()->setContextProperty(QStringLiteral("mediaUpload"), &mediaUploadController);
    engine.rootContext()->setContextProperty(QStringLiteral("mediaClassifier"), &mediaClassifier);
    engine.rootContext()->setContextProperty(QStringLiteral("mediaPlayback"), &mediaPlayback);
    // Qt 6.12 QAudioOutput unconditionally probes host audio devices. The
    // offscreen/minimal QPA backends do not have a device service, and some
    // Linux PipeWire builds crash during that probe. Headless smoke and
    // automation must never activate a platform audio device.
    const QString qpaBackend = QGuiApplication::platformName();
    engine.rootContext()->setContextProperty(QStringLiteral("nativeAudioOutputEnabled"),
        qpaBackend != QStringLiteral("offscreen")
        && qpaBackend != QStringLiteral("minimal"));
    engine.rootContext()->setContextProperty(QStringLiteral("localizationController"), &localizationController);
    engine.rootContext()->setContextProperty(QStringLiteral("geoLocationResolver"), &geoLocationResolver);
    engine.rootContext()->setContextProperty(QStringLiteral("linkPreviewResolver"), &linkPreviewResolver);
    engine.rootContext()->setContextProperty(
        QStringLiteral("browserLinkMetadataController"), &browserLinkMetadataController);
    engine.rootContext()->setContextProperty(
        QStringLiteral("browserTokenPreviewController"), &browserTokenPreviewController);
    engine.rootContext()->setContextProperty(QStringLiteral("castViewController"), &castViewController);
    engine.rootContext()->setContextProperty(
        QStringLiteral("browserReactionController"), &browserReactionController);
    engine.rootContext()->setContextProperty(QStringLiteral("miniAppHost"), &miniAppHost);
    engine.rootContext()->setContextProperty(QStringLiteral("miniAppCatalog"), &miniAppCatalog);
    engine.rootContext()->setContextProperty(QStringLiteral("miniAppTaskManager"), &miniAppTaskManager);
    engine.rootContext()->setContextProperty(QStringLiteral("liveInsights"), &liveInsightsController);
    engine.rootContext()->setContextProperty(QStringLiteral("nodeSelectionController"), &nodeSelectionController);
    engine.rootContext()->setContextProperty(QStringLiteral("providerProfiles"), &providerProfilesController);
    engine.rootContext()->setContextProperty(QStringLiteral("updateController"), &updateController);
#if defined(__EMSCRIPTEN__)
    engine.rootContext()->setContextProperty(QStringLiteral("wasmReadOnly"), QVariant{true});
#else
    engine.rootContext()->setContextProperty(QStringLiteral("wasmReadOnly"), QVariant{false});
#endif
    engine.rootContext()->setContextProperty(QStringLiteral("appearanceController"), &appearanceController);
    engine.rootContext()->setContextProperty(QStringLiteral("renderingController"), &renderingController);
    engine.rootContext()->setContextProperty(QStringLiteral("textSelectionSettings"), &textSelectionSettings);
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    engine.rootContext()->setContextProperty(QStringLiteral("nodeProviderLabel"),
        providerProfilesController.activeReadProviderLabel());
    engine.rootContext()->setContextProperty(QStringLiteral("nodeProviderIcon"),
        providerProfilesController.activeReadContract() == QStringLiteral("snapchain-v1")
            ? QString{} : QStringLiteral("quilibrium/hypersnap"));
#else
    engine.rootContext()->setContextProperty(QStringLiteral("nodeProviderLabel"), QStringLiteral("Self-hosted"));
    engine.rootContext()->setContextProperty(QStringLiteral("nodeProviderIcon"), QStringLiteral("quilibrium/quilibrium"));
#endif
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("Farcaster.Client"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
#if defined(__EMSCRIPTEN__)
        // The HTML Qt loader may have declared the runtime ready already.
        // Surface QML construction failures rather than leaving a blank page.
        EM_ASM({
            window.__genycasterQmlStartup?.(false,
                "QQmlApplicationEngine could not create the Main interface");
        });
#endif
        return 1;
    }
    auto* const mainWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (!mainWindow) {
#if defined(__EMSCRIPTEN__)
        EM_ASM({
            window.__genycasterQmlStartup?.(false,
                "Qt Main did not produce a QQuickWindow");
        });
#endif
        return 1;
    }
    renderingController.attachWindow(mainWindow);
#if defined(__EMSCRIPTEN__)
    installBrowserWheelBridge(mainWindow);
    genycasterInstallBrowserEmojiRecovery();
    // This is the only reliable readiness event: the Main.qml object tree
    // exists, rather than just the Emscripten/Qt runtime being initialized.
    EM_ASM({ window.__genycasterQmlStartup?.(true, ""); });
#endif

    // Incoming links are resolved only once the cache-first runtime can serve
    // cast/profile navigation. Browser history changes are accepted without
    // reloading WASM; desktop installers may forward --open-url=<https URL>.
    QString lastIncomingLink{};
    const QString nativeIncomingLink = optionValue(arguments, QStringLiteral("--open-url="));
    auto dispatchIncomingLink = [&presentationBridge, &lastIncomingLink, &nativeIncomingLink] {
        if (!presentationBridge.runtimeReady()
            || presentationBridge.initialSetupRequired()) return;
        QString incoming = nativeIncomingLink;
#if defined(__EMSCRIPTEN__)
        const char* const href = emscripten_run_script_string("window.location.href");
        incoming = href != nullptr ? QString::fromUtf8(href) : QString{};
#endif
        if (incoming.isEmpty() || incoming == lastIncomingLink) return;
        lastIncomingLink = incoming;
        static_cast<void>(presentationBridge.openSharedLink(incoming));
    };
    QObject::connect(&presentationBridge, &PresentationBridge::runtimeStateChanged,
                     &application, dispatchIncomingLink);
    QObject::connect(&presentationBridge, &PresentationBridge::setupStateChanged,
                     &application, dispatchIncomingLink);
#if defined(__EMSCRIPTEN__)
    QTimer incomingLinkPoll{&application};
    incomingLinkPoll.setInterval(750);
    QObject::connect(&incomingLinkPoll, &QTimer::timeout, &application, dispatchIncomingLink);
    incomingLinkPoll.start();
#endif

    std::shared_ptr<Runtime> runtime{};
#if !defined(__EMSCRIPTEN__)
    // Android NDK 27's libc++ does not provide std::jthread. Bootstrap has no
    // stop_token parameter, so a joining std::thread preserves its lifetime
    // without detaching or exposing referenced Qt objects to a dangling worker.
    struct BootstrapThread final {
        std::thread worker{};

        ~BootstrapThread()
        {
            if (worker.joinable()) {
                worker.join();
            }
        }
    };
    BootstrapThread bootstrap{};
#endif
    if (!isSmokeTest && !isVisualTest) {
        const QString databasePathText = optionValue(
            arguments, QStringLiteral("--database-path="));
        const std::filesystem::path databasePath = databasePathText.isEmpty()
            ? std::filesystem::path{
                  QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toStdString()}
                  / "genycaster.sqlite3"
            : std::filesystem::path{databasePathText.toStdString()};
        auto bootstrapRuntime = [&presentationBridge, &richComposerController, &openProofConnections,
                                 &mediaUploadController,
                                  &mediaPlayback, &runtime,
                                  databasePath, networkProxyPolicy, providerBaseUrl,
                                  farcasterReadBaseUrl, farcasterReadApiKey,
                                  additionalReadProviders, readProviderApiKeys,
                                  preferredReadProviderId, allowCompatibleReadFallback,
                                  additionalWriteProviders, preferredWriteProviderId, authRelayUrl,
                                  optimismRpcUrl, snapchainBaseUrl, snapchainWriteBaseUrl,
                                  mediaGatewayBaseUrl, snapchainAuthorizationHeader,
                                  snapchainAuthorizationValue,
                                  snapchainWriteAuthorizationHeader,
                                  snapchainWriteAuthorizationValue, appFid,
                                  signerAttestationUrl, walletLaunchTemplate,
                                  siwfDomain, siwfUri, startupIdentity,
                                  openExploreForTest] {
            auto made = makeRuntime(
                databasePath,
                networkProxyPolicy,
                providerBaseUrl,
                farcasterReadBaseUrl,
                farcasterReadApiKey,
                additionalReadProviders,
                readProviderApiKeys,
                preferredReadProviderId,
                allowCompatibleReadFallback,
                additionalWriteProviders,
                preferredWriteProviderId,
                authRelayUrl,
                optimismRpcUrl,
                snapchainBaseUrl,
                snapchainWriteBaseUrl,
                mediaGatewayBaseUrl,
                snapchainAuthorizationHeader,
                snapchainAuthorizationValue,
                snapchainWriteAuthorizationHeader,
                snapchainWriteAuthorizationValue,
                appFid,
                signerAttestationUrl,
                makeWalletApprovalLauncher(walletLaunchTemplate, true));
            if (!made) {
                const QString message = QString::fromStdString(made.error().message);
                QMetaObject::invokeMethod(&presentationBridge,
                    [&presentationBridge, message] { presentationBridge.applyRuntimeState(false, message); },
                    Qt::QueuedConnection);
                return;
            }
            auto readyRuntime = std::shared_ptr<Runtime>{std::move(*made)};
            QMetaObject::invokeMethod(&presentationBridge,
                [&presentationBridge, &richComposerController, &openProofConnections,
                 &mediaUploadController,
                 &mediaPlayback, &runtime, readyRuntime = std::move(readyRuntime),
                 siwfDomain, siwfUri, startupIdentity,
                 snapchainBaseUrl, openExploreForTest]() mutable {
                    runtime = std::move(readyRuntime);
                    connectRuntime(
                        presentationBridge, richComposerController, openProofConnections,
                        runtime, authenticationAvailable,
                        siwfDomain, siwfUri,
                        snapchainBaseUrl ? QString::fromStdString(*snapchainBaseUrl)
                                         : QStringLiteral("https://snapchain.genyleap.com"));
                    connectMediaPlayback(mediaPlayback, runtime);
#if defined(__EMSCRIPTEN__)
                    connectBrowserMediaUpload(mediaUploadController, presentationBridge,
                                              richComposerController);
#elif FARCASTER_HAS_MEDIA_UPLOAD
                    connectMediaUpload(mediaUploadController, runtime);
#else
                    mediaUploadController.setAvailable(false);
#endif
                    presentationBridge.applyRuntimeState(true);
                    if (startupIdentity.isEmpty()) {
                        if (openExploreForTest) {
                            presentationBridge.selectFeed(QStringLiteral("recent"));
                        }
                        return;
                    }
                    // PresentationBridge owns the identity-to-Home transition for both
                    // restored and manually opened identities. Explorer is an
                    // explicit Recent capability, independent of personal Home.
                    presentationBridge.openIdentity(startupIdentity);
                }, Qt::QueuedConnection);
        };
#if defined(__EMSCRIPTEN__)
        // A worker thread cannot start in a single-threaded WASM browser runtime.
        // Defer initialization until Qt's main event loop is active.
        QTimer::singleShot(0, &application, std::move(bootstrapRuntime));
#else
        bootstrap.worker = std::thread(std::move(bootstrapRuntime));
#endif
    }

    if (isSmokeTest) {
        QObject* const rootObject = engine.rootObjects().constFirst();
        bool ready = rootObject != nullptr
            && rootObject->objectName() == QStringLiteral("rootWindow")
            && rootObject->property("isReady").toBool()
            && rootObject->findChild<QObject*>(QStringLiteral("appShell")) != nullptr
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
#if defined(__EMSCRIPTEN__)
    // IDBFS is async: periodically flush SQLite's virtual file changes to
    // IndexedDB. The preRun hook restores the directory before main().
    QTimer wasmPersistenceFlush{&application};
    wasmPersistenceFlush.setInterval(8'000);
    const auto flushBrowserData = [] {
        EM_ASM({
            Module.genycasterSyncBrowserData().catch(function(error) {
                console.error('[Genycaster] IndexedDB sync failed', error);
            });
        });
    };
    QObject::connect(&wasmPersistenceFlush, &QTimer::timeout,
                     &application, flushBrowserData);
    wasmPersistenceFlush.start();
    // A provider choice must be flushed before the user can reload the page.
    // The reload operation queues one last flush and awaits its completion.
    QObject::connect(&providerProfilesController, &ProviderProfilesController::selectionChanged,
                     &application, flushBrowserData);
    QObject::connect(&providerProfilesController, &ProviderProfilesController::profilesChanged,
                     &application, flushBrowserData);
    // Persist onboarding/consent immediately instead of waiting for the
    // periodic SQLite flush. Never synchronize a private mnemonic here.
    QObject::connect(&presentationBridge, &PresentationBridge::setupStateChanged,
                     &application, [&settings, flushBrowserData] {
        settings.sync();
        flushBrowserData();
    });
#endif
    const int exitCode = application.exec();
#if defined(Q_OS_ANDROID)
    // Android HWUI may still have an active rendering thread when Qt's Java
    // bridge returns from main(). Tear down the scene graph and the native
    // QQuickWindow while QGuiApplication and its platform resources remain
    // alive, rather than leaving window cleanup to the C++ object unwind.
    // The bootstrap worker may refer to UI controllers; join it first.
    if (bootstrap.worker.joinable()) {
        bootstrap.worker.join();
    }
    if (mainWindow != nullptr) {
        mainWindow->setPersistentGraphics(false);
        mainWindow->setPersistentSceneGraph(false);
        mainWindow->hide();
        mainWindow->releaseResources();
        delete mainWindow;
    }
#endif
    return exitCode;
}
