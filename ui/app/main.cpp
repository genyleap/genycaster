#include "genycaster_deployment.hpp"
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QEvent>
#include <QPointer>
#include <QDir>
#include <QDebug>
#include <QFont>
#include <QGuiApplication>
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(Q_OS_WASM)
#include <QApplication>
#include "platform/desktop_background.hpp"
#endif
#if (defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID) && defined(GENYCASTER_HAS_LINUX_MINIAPP_WIDGETS)) \
 || (defined(Q_OS_WIN) && defined(GENYCASTER_HAS_WINDOWS_MINIAPP_HOST))
#include <QtWebEngineQuick>
#include "miniapp/browser_linux_quick.hpp"
#endif
#include <QIcon>
#if defined(Q_OS_ANDROID)
#include <QInputMethod>
#include <QJniObject>
#include <QKeyEvent>
#if QT_CONFIG(vulkan)
#include <QVulkanFunctions>
#include <QVulkanInstance>
#endif
#include <sys/system_properties.h>
#endif
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkInformation>
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <QSslSocket>
#endif
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#if defined(Q_OS_ANDROID)
#include "media/android_icon.hpp"
#include "media/android_document_picker.hpp"
#include <QtMultimedia/private/qplatformmediaintegration_p.h>
#endif
#if defined(GENYCASTER_HAS_MOBILE_MINIAPP_WEBVIEW)
#include <QtWebView/QtWebView>
#endif
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
#include "diagnostics/gui_event_trace.hpp"
#include "emoji_controller.hpp"
#include "rich_inline_text_item.hpp"
#include "rich_composer_controller.hpp"
#include "social/channels.hpp"
#include "social/channel_administration.hpp"
#include "settings/selection.hpp"
#include "appearance_controller.hpp"
#include "localization_controller.hpp"
#include "media_upload_controller.hpp"
#include "media_classifier_controller.hpp"
#include "media_playback_coordinator.hpp"
#include "network/geo.hpp"
#include "network/link_preview.hpp"
#include "social/cast_views.hpp"
#include "social/stats.hpp"
#include "data/market.hpp"
#include "browser/reactions.hpp"
#include "browser/link_metadata.hpp"
#include "browser/token_preview.hpp"
#include "miniapp/host.hpp"
#include "media/web_host.hpp"
#include "miniapp/catalog.hpp"
#include "miniapp/tasks.hpp"
#include "data/insights.hpp"
#include "network/node_selection.hpp"
#include "settings/providers.hpp"
#include "update/controller.hpp"

#if defined(Q_OS_WASM)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include "media/remote_image.hpp"
#include "identity/browser_auth.hpp"
#include "identity/connections.hpp"
#include "identity/native_registration.hpp"
#include "identity/native_prepare.hpp"
#include "identity/native_publish.hpp"
#include "identity/native_wallet.hpp"
#include "identity/neynar_registration_controller.hpp"
#include "identity/pro_checkout.hpp"
#if defined(Q_OS_WASM)
#include "identity/browser_pro_wallet.hpp"
#endif
#include "social/activity.hpp"
#if defined(Q_OS_WASM)
#include "browser/signer_bridge.hpp"
#include "browser/cast_payload.hpp"
#include "browser/emoji_recovery.hpp"
#endif
#include "data/storage.hpp"

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
import genycaster.ui.shutdown;
import genycaster.ui.bootstrap;
import genycaster.ui.bootstrap.fonts;
import genycaster.ui.bootstrap.context;
import genycaster.ui.bootstrap.monitoring;
import genycaster.ui.bootstrap.providers;
import genycaster.ui.bootstrap.profiles;
import genycaster.ui.bootstrap.routes;
import genycaster.ui.bootstrap.auth;
import genycaster.ui.bootstrap.wallet;
#if !defined(Q_OS_WASM)
import genycaster.ui.bootstrap.teardown;
import genycaster.ui.bootstrap.worker;
#endif
import genycaster.ui.diagnostics.snapshots;
import genycaster.ui.diagnostics.smoke;
import genycaster.ui.web.wheel;
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(Q_OS_WASM)
import genycaster.ui.desktop.instance;
#endif
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
import farcaster.provider.hypersnap.config;
#endif
#if FARCASTER_HAS_MINIAPP_EVM_WALLET
import farcaster.capability.wallet;
import farcaster.provider.compatibility.browser_wallet.bridge;
#endif

#include "runtime/bindings.hpp"


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
#if defined(Q_OS_WASM)
using genycaster::ui::bindings::connectBrowserMediaUpload;
#endif

using genycaster::ui::bootstrap::optionValue;
using genycaster::ui::bootstrap::startupRenderingProfile;
using genycaster::ui::bootstrap::normalizeConfigurationToken;
using genycaster::ui::bootstrap::isValidHttpServiceUrl;
using genycaster::ui::bootstrap::isValidSiwfUri;
using genycaster::ui::bootstrap::isValidSiwfDomain;
using genycaster::ui::bootstrap::siwfDomainMatchesUri;

[[nodiscard]] std::function<foundation::result::Status(const std::string&)>
makeWalletApprovalLauncher(QString launchTemplate, const bool preferCustodyWalletBrowser = false)
{
    return [launchTemplate = std::move(launchTemplate), preferCustodyWalletBrowser](
               const std::string& approvalUrl) -> foundation::result::Status {
        const auto prepared = genycaster::ui::bootstrap::prepareWalletApproval(
            approvalUrl, launchTemplate.toStdString(), preferCustodyWalletBrowser);
        if (!prepared) {
            const bool invalidTemplate =
                prepared.error() == genycaster::ui::bootstrap::WalletLaunchError::InvalidTemplate;
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                invalidTemplate
                    ? "Wallet launch URL template must contain {url} or {rawUrl}."
                    : "Wallet approval launcher produced an invalid or unsafe URL."));
        }
        // The launcher policy is shared; the Qt event-loop shutdown boundary
        // remains owned by this composition root.
#if defined(Q_OS_WASM)
        const auto dispatch = [](std::function<bool()> operation) {
            return operation();
        };
#else
        const auto dispatch = [](std::function<bool()> operation) {
            return genycaster::ui::shutdown::invokeOnApplication(std::move(operation));
        };
#endif
        if (!genycaster::ui::bootstrap::openWalletApproval(*prepared, dispatch)) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "The platform could not open a browser or wallet for Mini App approval."));
        }
        return {};
    };
}

#if defined(Q_OS_ANDROID)
#if QT_CONFIG(vulkan)
void selectAndroidEmulatorRenderer()
{
    // The ranchu GLES translator can present valid RGBA images as black
    // quads. Use Vulkan only on that emulator when its driver has a device.
    char emulator[PROP_VALUE_MAX]{};
    char driver[PROP_VALUE_MAX]{};
    if (!qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND")
        || !qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND")
        || __system_property_get("ro.boot.qemu", emulator) <= 0 || QByteArrayView{emulator} != "1"
        || __system_property_get("ro.hardware.vulkan", driver) <= 0 || QByteArrayView{driver} != "ranchu")
        return;
    QVulkanInstance probe;
    std::uint32_t deviceCount{};
    if (probe.supportedExtensions().contains("VK_KHR_android_surface") && probe.create()
        && probe.functions()->vkEnumeratePhysicalDevices(probe.vkInstance(), &deviceCount, nullptr) == VK_SUCCESS
        && deviceCount > 0)
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
}
#endif

// The Android back gesture arrives at the Qt window, not necessarily the
// focused QML item (especially with a modal overlay). Dispatch centrally,
// while letting the platform close an active software keyboard first.
class AndroidBackDispatcher final : public QObject {
public:
    AndroidBackDispatcher(QQuickWindow* window, QObject* shell)
        : QObject{window}, m_window{window}, m_shell{shell} {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched != m_window || !m_shell ||
            (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease))
            return false;
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() != Qt::Key_Back && key->key() != Qt::Key_Escape)
            return false;
        if (event->type() == QEvent::KeyRelease) {
            const bool consumed = m_consumedKey == key->key();
            if (consumed) m_consumedKey = 0;
            return consumed;
        }
        if (key->isAutoRepeat() && m_consumedKey == key->key()) return true;
        m_consumedKey = 0;
        if (QGuiApplication::inputMethod()
            && QGuiApplication::inputMethod()->isVisible())
            return false;
        QVariant handled{};
        if (!QMetaObject::invokeMethod(m_shell.data(), "handlePlatformBack",
                                       Q_RETURN_ARG(QVariant, handled)))
            return false;
        if (handled.toBool()) {
            m_consumedKey = key->key();
            return true;
        }
        if (key->key() != Qt::Key_Back ||
            !QNativeInterface::QAndroidApplication::isActivityContext())
            return false;
        // Root Back backgrounds the task rather than finishing the Activity.
        // Android still owns process suspension and low-memory eviction.
        m_consumedKey = key->key();
        (void)QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
            if (!QNativeInterface::QAndroidApplication::isActivityContext()) return;
            const auto activity = QNativeInterface::QAndroidApplication::context();
            if (activity.isValid())
                (void)activity.callMethod<jboolean>("moveTaskToBack", "(Z)Z", JNI_TRUE);
        });
        return true;
    }

private:
    QQuickWindow* m_window{};
    QPointer<QObject> m_shell{};
    int m_consumedKey{0};
};
#endif

}

int main(int argc, char* argv[])
{
#if defined(Q_OS_WASM)
    // Qt Quick's browser scene graph must use the GPU-oriented OpenGL RHI.
    // Emscripten translates this backend into WebGL 2 (or WebGL 1 when
    // constrained by the browser). A browser/driver software GPU fallback
    // cannot be overridden from application code.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGLRhi);
#endif
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // Parallels' virtual GPU can create a window without presenting frames.
    // Keep Qt's RHI scene graph (Shapes and shader effects require it) and
    // render its OpenGL backend through Mesa's CPU llvmpipe driver instead.
    // QT_QUICK_BACKEND=software replaces the entire scene graph and can
    // yield a blank UI even though QML startup succeeds. Honour explicit
    // user/administrator backend overrides.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND")
        && qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND")) {
        QFile virtualMachineProduct{QStringLiteral("/sys/class/dmi/id/product_name")};
        QFile virtualMachineVendor{QStringLiteral("/sys/class/dmi/id/sys_vendor")};
        QFile virtualMachineArmModel{QStringLiteral("/sys/firmware/devicetree/base/model")};
        const auto containsParallels = [](QFile& file) {
            return file.open(QIODevice::ReadOnly)
                && file.read(256).toLower().contains("parallels");
        };
        if (containsParallels(virtualMachineProduct)
            || containsParallels(virtualMachineVendor)
            || containsParallels(virtualMachineArmModel)) {
            qputenv("QSG_RHI_BACKEND", "opengl");
            if (qEnvironmentVariableIsEmpty("LIBGL_ALWAYS_SOFTWARE"))
                qputenv("LIBGL_ALWAYS_SOFTWARE", "1");
            qInfo() << "GenyCaster: Parallels detected; using Qt RHI"
                       " OpenGL through Mesa software rasterization";
        }
    }
#endif
#if defined(Q_OS_ANDROID)
    // Match the bundled OpenSSL 3 sonames before Qt initializes TLS/networking.
    qputenv("ANDROID_OPENSSL_SUFFIX", "_3");
    // Qt 6.12 native Android video uses a separate GLES texture thread.
    // Keep emulator Vulkan UI rendering, but decode its video on the CPU:
    // no cross-API native texture sharing on the virtual GPU.
    char emulatorMedia[PROP_VALUE_MAX]{};
    if (__system_property_get("ro.boot.qemu", emulatorMedia) > 0
        && QByteArrayView{emulatorMedia} == "1") {
        if (!qEnvironmentVariableIsSet("QT_FFMPEG_DECODING_HW_DEVICE_TYPES"))
            qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", ",");
        if (!qEnvironmentVariableIsSet("QT_DISABLE_HW_TEXTURES_CONVERSION"))
            qputenv("QT_DISABLE_HW_TEXTURES_CONVERSION", "1");
    }
#endif
#if defined(GENYCASTER_HAS_MOBILE_MINIAPP_WEBVIEW)
    // Initialize native mobile WebView before constructing the GUI application.
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

#if (defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID) && defined(GENYCASTER_HAS_LINUX_MINIAPP_WIDGETS)) \
 || (defined(Q_OS_WIN) && defined(GENYCASTER_HAS_WINDOWS_MINIAPP_HOST))
    // WebEngine Quick's shared context must exist before QApplication.
    // Desktop widgets also provide the native system tray.
    QtWebEngineQuick::initialize();
#endif
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(Q_OS_WASM)
    genycaster::ui::diagnostics::GuiEventTraceApplication<
        QApplication, &RenderingController::traceClockMs> application(argc, argv);
#else
    genycaster::ui::diagnostics::GuiEventTraceApplication<
        QGuiApplication, &RenderingController::traceClockMs> application(argc, argv);
#endif
#if defined(Q_OS_ANDROID)
    // Qt 6.12 private API: unlike QT_MEDIA_BACKEND, setBackend() does NOT
    // silently fall back to the Android native plugin if FFmpeg cannot load.
    // In that case QMediaPlayer reports unavailable instead of initializing
    // the incompatible native texture renderer. Must precede all media objects.
    QPlatformMediaIntegration::setBackend(QStringLiteral("ffmpeg"));
    qInfo() << "Genycaster: Android media backend requested: FFmpeg";
    const auto mediaBackends = QPlatformMediaIntegration::availableBackends();
    qInfo() << "Genycaster: Android discovered media backends:" << mediaBackends;
    if (!mediaBackends.contains(QStringLiteral("ffmpeg"), Qt::CaseInsensitive))
        qWarning() << "Genycaster: FFmpeg plugin not discovered; video/audio unavailable";
    else
        qInfo() << "Genycaster: FFmpeg plugin discovered; decoder initialization is lazy";
#if QT_CONFIG(vulkan)
    selectAndroidEmulatorRenderer();
#endif
#endif
    QCoreApplication::setOrganizationName(QStringLiteral("Genycaster"));
    QCoreApplication::setApplicationName(QStringLiteral("Genycaster"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(GENYCASTER_APP_VERSION));
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(Q_OS_WASM)
    // Desktop window/task-switcher identity complements the bundled native
    // Windows PE and macOS application resources.
    QGuiApplication::setWindowIcon(QIcon{QStringLiteral(":/genycaster/app-icon.png")});
#endif
    QQuickStyle::setStyle(QStringLiteral(FARCASTER_QT_QUICK_CONTROLS_STYLE));
    genycaster::ui::bootstrap::FontFamilies fontFamilies{};
    if (!fontFamilies.load()) return 2;


    const QStringList arguments = QCoreApplication::arguments();
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (arguments.contains(QStringLiteral("--check-tls"))) {
        // Do not let a correctly linked Qt/QML shell mask an unusable
        // runtime OpenSSL backend. This probes actual dlopen/Qt TLS loading.
        const bool supportsTls = QSslSocket::supportsSsl();
        const QString backend = QSslSocket::activeBackend();
        if (!supportsTls || backend != QStringLiteral("openssl")) {
            qCritical().noquote()
                << "GenyCaster Linux TLS preflight failed: backend=" << backend
                << "sslAvailable=" << supportsTls;
            return 7;
        }
        qInfo().noquote() << "GenyCaster Linux TLS preflight: PASS"
                            << QSslSocket::sslLibraryVersionString();
        return 0;
    }
#endif
    const bool isSmokeTest = arguments.contains(QStringLiteral("--smoke-test"));
    const bool isVisualTest = arguments.contains(QStringLiteral("--visual-test"));
    const bool isRuntimeSmokeTest = arguments.contains(QStringLiteral("--runtime-smoke-test"));
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(Q_OS_WASM)
    // The real desktop client is one process per OS user. Test runners
    // intentionally retain independent instances and isolated fixtures.
    genycaster::ui::DesktopSingleInstance desktopInstance{};
    if (!isSmokeTest && !isVisualTest && !isRuntimeSmokeTest) {
        const auto result = desktopInstance.acquire(
            optionValue(arguments, QStringLiteral("--open-url=")));
        if (result == genycaster::ui::DesktopSingleInstance::Result::ActivatedExisting)
            return 0;
        if (result == genycaster::ui::DesktopSingleInstance::Result::Error) {
            qCritical().noquote() << "GenyCaster startup:" << desktopInstance.errorString();
            return 4; // Never start a second copy with shared writable state.
        }
    }
#endif
    const bool openExploreForTest = arguments.contains(QStringLiteral("--open-explore"));
    const qsizetype snapshotIndex = arguments.indexOf(QStringLiteral("--snapshot"));
    const QString snapshotPath = snapshotIndex >= 0 && snapshotIndex + 1 < arguments.size()
        ? arguments[snapshotIndex + 1] : QString{};

    const QString settingsPath = optionValue(arguments, QStringLiteral("--settings-path="));
#if defined(Q_OS_WASM)
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

    genycaster::ui::bootstrap::ProviderEndpoints endpoints{settings, arguments};
    if (!endpoints.valid()) return 2;
    const QString& networkProxyText = endpoints.proxyMode();
    const auto networkProxyPolicy = endpoints.proxyDirect()
        ? farcaster::transport::qtnetwork::ProxyPolicy::Direct
        : farcaster::transport::qtnetwork::ProxyPolicy::System;

    const QString& providerBaseUrlText = endpoints.hypersnapBaseUrl();
    const std::optional<std::string> providerBaseUrl = providerBaseUrlText.isEmpty()
        ? std::nullopt : std::optional<std::string>{providerBaseUrlText.toStdString()};

    const QString& farcasterReadBaseUrlText = endpoints.farcasterReadBaseUrl();
    const std::optional<std::string> farcasterReadBaseUrl = farcasterReadBaseUrlText.isEmpty()
        ? std::nullopt : std::optional<std::string>{farcasterReadBaseUrlText.toStdString()};
    const QString& farcasterReadApiKeyText = endpoints.farcasterReadApiKey();
    const std::optional<std::string> farcasterReadApiKey = farcasterReadApiKeyText.isEmpty()
        ? std::nullopt : std::optional<std::string>{farcasterReadApiKeyText.toStdString()};

    // Keep Read and Write profiles separate: the selected Read provider
    // never authorizes an implicit cross-protocol write destination.
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    const bool enableHypersnapDefault =
        application::configuration::configuredDeploymentProfile()
        != application::configuration::DeploymentProfile::StrictSelfHosted;
#else
    constexpr bool enableHypersnapDefault = false;
#endif
    auto configuredProfiles = genycaster::ui::bootstrap::loadProviderProfiles(
        settings, arguments, enableHypersnapDefault);
    if (!configuredProfiles) return 2;

    // Resolve routes while ProviderProfileConfiguration still owns its
    // original selections. Moving an optional<string> keeps it engaged on
    // common standard libraries but can empty the contained string; resolving
    // after that move caused macOS startup to exit with an empty provider ID.
    // Read and Write choices remain independent. Never cross-post by accident.
    QString compiledHypersnapEndpoint{};
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    compiledHypersnapEndpoint = QString::fromStdString(
        farcaster::provider::hypersnap::config::Config{}.baseUrl);
#endif
    auto routingPolicy = genycaster::ui::bootstrap::loadProviderRoutes(
        settings, arguments, providerBaseUrlText, compiledHypersnapEndpoint,
        QString::fromStdString(std::string{deploymentEndpoints.snapchainBaseUrl}),
        QString::fromStdString(std::string{deploymentEndpoints.mediaGatewayBaseUrl}),
        *configuredProfiles);
    if (!routingPolicy) return 2;


    std::vector<genycaster::ui::runtime::ReadProviderProfile> additionalReadProviders{};
    additionalReadProviders.reserve(configuredProfiles->readProviders.size());
    for (auto& profile : configuredProfiles->readProviders) {
        additionalReadProviders.push_back({
            .id = profile.id,
            .displayName = profile.displayName,
            .endpoint = profile.endpoint,
            .adapterContract = profile.adapterContract,
            .selfHosted = profile.selfHosted,
        });
    }

    const bool usePublicReadPool = enableHypersnapDefault
        && (providerBaseUrlText.isEmpty() || providerBaseUrlText == compiledHypersnapEndpoint)
        && settings.value(QStringLiteral("network/nodeSelectionMode"), QStringLiteral("auto")).toString() == QStringLiteral("auto")
        && configuredProfiles->preferredReadProviderId.value_or("") == "hypersnap-v2"
        && configuredProfiles->readProviderApiKeys.empty()
        && !qEnvironmentVariableIsSet("GENYCASTER_HYPERSNAP_BASE_URL")
        && !qEnvironmentVariableIsSet("GENYCASTER_PREFERRED_READ_PROVIDER_ID")
        && !std::any_of(arguments.cbegin(), arguments.cend(), [](const QString& arg) {
            return arg.startsWith(QStringLiteral("--preferred-read-provider-id="))
                || arg.startsWith(QStringLiteral("--hypersnap-base-url="));
        });
    std::vector<genycaster::ui::runtime::WriteProviderProfile> additionalWriteProviders{};
    additionalWriteProviders.reserve(configuredProfiles->writeProviders.size());
    for (auto& profile : configuredProfiles->writeProviders) {
        additionalWriteProviders.push_back({
            .id = std::move(profile.id),
            .displayName = std::move(profile.displayName),
            .endpoint = std::move(profile.endpoint),
            .readEndpoint = profile.readEndpoint,
            .adapterContract = std::move(profile.adapterContract),
            .selfHosted = profile.selfHosted,
        });
    }

    auto readProviderApiKeys = std::move(configuredProfiles->readProviderApiKeys);
    const bool allowCompatibleReadFallback = configuredProfiles->allowCompatibleReadFallback;
    // Keep the tiny selection IDs intact in the original configuration too.
    const auto preferredReadProviderId = configuredProfiles->preferredReadProviderId;
    const auto preferredWriteProviderId = configuredProfiles->preferredWriteProviderId;
    const QString providerProfilesJson =
        QString::fromStdString(configuredProfiles->readProfilesJson);
    const QString writeProfilesJson =
        QString::fromStdString(configuredProfiles->writeProfilesJson);
    const QString preferredReadProviderText =
        QString::fromStdString(configuredProfiles->preferredReadProviderText);
    const QString preferredWriteProviderText =
        QString::fromStdString(configuredProfiles->preferredWriteProviderText);

    QStringList nodeCandidateEndpoints{};
    for (const std::string& endpoint : routingPolicy->nodeCandidateEndpoints)
        nodeCandidateEndpoints.push_back(QString::fromStdString(endpoint));
    const QString appliedNodeEndpoint =
        QString::fromStdString(routingPolicy->appliedNodeEndpoint);
    const QString mediaGatewayBaseUrlText =
        QString::fromStdString(routingPolicy->mediaGatewayEndpoint);
    const std::optional<std::string> snapchainBaseUrl{routingPolicy->snapchainReadEndpoint};
    const std::optional<std::string> snapchainWriteBaseUrl{routingPolicy->snapchainWriteEndpoint};
    const std::optional<std::string> mediaGatewayBaseUrl{routingPolicy->mediaGatewayEndpoint};
    const auto& snapchainAuthorizationHeader = routingPolicy->readAuthorizationHeader;
    const auto& snapchainAuthorizationValue = routingPolicy->readAuthorizationValue;
    const auto& snapchainWriteAuthorizationHeader = routingPolicy->writeAuthorizationHeader;
    const auto& snapchainWriteAuthorizationValue = routingPolicy->writeAuthorizationValue;

    // Authentication is independent from the selected provider route.
    // A Read/Write gateway never becomes a signer attestation authority.
#if defined(Q_OS_WASM) || GENYCASTER_HAS_OPENPROOF_AUTH
    constexpr bool openProofLoginAvailable = true;
#else
    constexpr bool openProofLoginAvailable = false;
#endif
#if FARCASTER_HAS_FARCASTER_AUTH
    constexpr bool compatibleLoginAvailable = true;
#else
    constexpr bool compatibleLoginAvailable = false;
#endif
    const auto authenticationOptions = genycaster::ui::bootstrap::loadAuthenticationOptions(
        arguments, deploymentIdentity.appFid,
        QString::fromStdString(std::string{deploymentEndpoints.siwfDomain}),
        QString::fromStdString(std::string{deploymentEndpoints.siwfUri}),
        openProofLoginAvailable, compatibleLoginAvailable);
    if (!authenticationOptions) return 2;

    const auto& appFid = authenticationOptions->appFid;
    const auto& signerAttestationUrl = authenticationOptions->signerAttestationUrl;
    const QString walletLaunchTemplate =
        QString::fromStdString(authenticationOptions->walletLaunchTemplate);
    const QString siwfUriText = QString::fromStdString(authenticationOptions->siwfUri);
    const auto& siwfDomain = authenticationOptions->siwfDomain;
    const auto& siwfUri = authenticationOptions->siwfUri;
    const auto& authRelayUrl = authenticationOptions->authRelayUrl;
    const auto& optimismRpcUrl = authenticationOptions->optimismRpcUrl;
    const bool authenticationAvailable = authenticationOptions->authenticationAvailable;

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
#if defined(Q_OS_ANDROID)
    std::unique_ptr<AndroidComposerMediaPicker> androidMediaPickerOwner{};
#endif
    std::unique_ptr<MediaClassifierController> mediaClassifierControllerOwner{};
    std::unique_ptr<MediaPlaybackCoordinator> mediaPlaybackCoordinatorOwner{};
    std::unique_ptr<LiveInsightsController> liveInsightsControllerOwner{};
    std::unique_ptr<NodeSelectionController> nodeSelectionControllerOwner{};
    std::unique_ptr<ProviderProfilesController> providerProfilesControllerOwner{};
    std::unique_ptr<NativeRegistrationStatusController> nativeRegistrationStatusOwner{};
    std::unique_ptr<NativeCustodyWalletController> nativeCustodyWalletOwner{};
    std::unique_ptr<NativeRegistrationPreparationController> nativePreparationOwner{};
    std::unique_ptr<HyperSnapRegistrationPublisher> nativePublisherOwner{};
    std::unique_ptr<NeynarRegistrationController> neynarRegistrationOwner{};
    std::unique_ptr<UpdateController> updateControllerOwner{};
    std::unique_ptr<MiniAppHostController> miniAppHostControllerOwner{};
    std::unique_ptr<MiniAppHostController> proWalletHostOwner{};
    std::unique_ptr<MediaWebHostController> mediaWebHostControllerOwner{};
    std::unique_ptr<MiniAppCatalogController> miniAppCatalogControllerOwner{};
    std::unique_ptr<ChannelPickerController> channelPickerControllerOwner{};
    std::unique_ptr<ChannelPickerController> channelBrowserControllerOwner{};
    std::unique_ptr<ChannelAdministrationController> channelAdministrationControllerOwner{};
    std::unique_ptr<MiniAppTaskManagerController> miniAppTaskManagerControllerOwner{};
#if FARCASTER_HAS_MINIAPP_EVM_WALLET
    std::unique_ptr<farcaster::capability::wallet::IEthereumWalletBridge> miniAppWalletBridgeOwner{};
    std::unique_ptr<farcaster::capability::wallet::IEthereumWalletBridge> proWalletBridgeOwner{};
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
    qmlRegisterType<NativeRecoveryWordsItem>(
        "Farcaster.Client", 1, 0, "NativeRecoveryWords");

    QQmlApplicationEngine engine{};
    engine.rootContext()->setContextProperty(QStringLiteral("deploymentEndpoints"),
        QVariantMap{
            {QStringLiteral("publicOrigin"), QString::fromUtf8(genycaster::deployment::publicOrigin.data())},
            {QStringLiteral("openProofIssuer"), QString::fromUtf8(genycaster::deployment::openProofIssuer.data())},
            {QStringLiteral("browserGateway"), QString::fromUtf8(genycaster::deployment::browserGateway.data())},
            {QStringLiteral("mediaPublicOrigin"), QString::fromUtf8(genycaster::deployment::mediaPublicOrigin.data())},
            {QStringLiteral("companyOrigin"), QString::fromUtf8(genycaster::deployment::companyOrigin.data())},
        });
#if !defined(Q_OS_WASM)
    // Declared after the engine and controller owners so early-return cleanup
    // joins global workers before their borrowed presentation objects disappear.
    genycaster::ui::shutdown::ShutdownScope shutdownScope{};
#endif

    appearanceControllerOwner = std::make_unique<AppearanceController>(
        settings, std::move(appearanceOverrides));
    AppearanceController& appearanceController = *appearanceControllerOwner;
#if defined(Q_OS_WASM)
    // Browser chrome uses the HTML document theme independently of Qt Quick's
    // canvas. Keep it in sync with the effective app appearance, including
    // manual overrides and system changes.
    const auto updateBrowserChromeTheme = [&appearanceController] {
        // The browser-owned download sheet and chrome must follow the *actual*
        // Qt appearance and user-selected accent, never OS-native HTML colors.
        const QByteArray themeId = appearanceController.colorThemeId().toUtf8();
        const QByteArray customAccent = appearanceController.customAccentColor().toUtf8();
        const QByteArray platformAccent = appearanceController.platformAccentColor().toUtf8();
        EM_ASM({
            if (typeof window.__genycasterSetBrowserChromeTheme === "function") {
                window.__genycasterSetBrowserChromeTheme(
                    $0 !== 0, UTF8ToString($1), UTF8ToString($2), UTF8ToString($3));
            }
        }, appearanceController.effectiveDark() ? 1 : 0,
           themeId.constData(), customAccent.constData(), platformAccent.constData());
    };
    updateBrowserChromeTheme();
    QObject::connect(&appearanceController, &AppearanceController::effectiveDarkChanged,
                     &application, updateBrowserChromeTheme);
    QObject::connect(&appearanceController, &AppearanceController::colorThemeIdChanged,
                     &application, updateBrowserChromeTheme);
    QObject::connect(&appearanceController, &AppearanceController::customAccentColorChanged,
                     &application, updateBrowserChromeTheme);
#endif
    renderingControllerOwner = std::make_unique<RenderingController>(settings);
    RenderingController& renderingController = *renderingControllerOwner;
#if !defined(Q_OS_WASM)
    // Register before mini-app/media close callbacks: they can publish QML
    // state changes during aboutToQuit. Close-to-tray does not emit this.
    QObject::connect(&application, &QCoreApplication::aboutToQuit,
                     &renderingController, &RenderingController::beginShutdown);
#endif
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
    // Use the actual applied HyperSnap node (including user-provided
    // command-line and node-profile overrides) rather than silently probing
    // the built-in public host when a different node was selected.
    const bool directoryReadOnly = providerProfilesController.activeReadProviderId()
        .startsWith(QStringLiteral("directory-"));
    QString nativeRegistrationEndpoint = directoryReadOnly
        ? appliedNodeEndpoint
        : providerProfilesController.activeReadContract() == QStringLiteral("hypersnap-v2")
            ? providerProfilesController.activeReadEndpoint() : compiledHypersnapEndpoint;
    const QStringList nativeSettingsKeys = settings.allKeys();
    const bool hasPendingNativeRegistration = std::any_of(
        nativeSettingsKeys.cbegin(), nativeSettingsKeys.cend(), [](const QString& key) {
            return key.startsWith(QStringLiteral("nativeRegistration/expiry-v1/"));
        });
    const bool automaticRegistration = usePublicReadPool && !directoryReadOnly
        && !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest;
    if (automaticRegistration && hasPendingNativeRegistration) {
        const QString pinned = settings.value(
            QStringLiteral("nativeRegistration/lastRuntimeOrigin")).toString();
        // Legacy pending journals were created on the originally configured node.
        const QUrl pinnedUrl{pinned, QUrl::StrictMode};
        if (pinnedUrl.isValid() && pinnedUrl.scheme() == QStringLiteral("https")
            && !pinnedUrl.host().isEmpty() && pinnedUrl.userInfo().isEmpty()
            && !pinnedUrl.hasQuery() && !pinnedUrl.hasFragment() && pinnedUrl.path().isEmpty())
            nativeRegistrationEndpoint = pinned;
    }
    nativeRegistrationStatusOwner = std::make_unique<NativeRegistrationStatusController>(
        nativeRegistrationEndpoint);
    NativeRegistrationStatusController& nativeRegistrationStatus =
        *nativeRegistrationStatusOwner;
    nativeCustodyWalletOwner = std::make_unique<NativeCustodyWalletController>();
    NativeCustodyWalletController& nativeCustodyWallet = *nativeCustodyWalletOwner;
    nativePreparationOwner = std::make_unique<NativeRegistrationPreparationController>(
        NativePreparationPorts{
            .backupVerified = [&nativeCustodyWallet] {
                return nativeCustodyWallet.backupVerified();
            },
            .custodyAddress = [&nativeCustodyWallet] {
                return nativeCustodyWallet.address();
            },
            .walletIsUnregistered = [&nativeRegistrationStatus](const QString& wallet) {
                return nativeRegistrationStatus.verifiedAbsentForCustody(wallet);
            },
            .challenge = [&nativeRegistrationStatus] {
                // A reachable but lagging shard-0 head is not safe for a
                // fresh irreversible NATIVE_ONBOARD proof.
                return nativeRegistrationStatus.syncedForRegistration()
                    ? nativeRegistrationStatus.snapshot()
                    : std::optional<NativeRegistrationStatusController::Challenge>{};
            },
            .seal = [&nativeCustodyWallet](
                const NativeRegistrationStatusController::Challenge& anchor,
                const QString& nonce) {
                return nativeCustodyWallet.sealNativeOnboard(
                    anchor.anchorHeight, anchor.anchorHash, nonce,
                    anchor.difficultyBits).value_or(QByteArray{});
            }
        });
    NativeRegistrationPreparationController& nativeRegistrationPreparation =
        *nativePreparationOwner;
    nativePublisherOwner = std::make_unique<HyperSnapRegistrationPublisher>(
        QUrl{nativeRegistrationEndpoint},
        NativePublicationPorts{
            .backupVerified = [&nativeCustodyWallet] {
                return nativeCustodyWallet.backupVerified();
            },
            .custodyAddress = [&nativeCustodyWallet] {
                return nativeCustodyWallet.address();
            },
            .anchorIsFresh = [&nativeRegistrationStatus] {
                return nativeRegistrationStatus.syncedForRegistration()
                    && nativeRegistrationStatus.snapshot().has_value();
            },
            .walletIsUnregistered = [&nativeRegistrationStatus](const QString& wallet) {
                return nativeRegistrationStatus.verifiedAbsentForCustody(wallet);
            },
            .proofPrepared = [&nativeRegistrationPreparation] {
                return nativeRegistrationPreparation.prepared();
            },
            .takeProof = [&nativeRegistrationPreparation](const QString& wallet) {
                return nativeRegistrationPreparation.takeForSubmission(wallet);
            },
            .cancelProof = [&nativeRegistrationPreparation] {
                nativeRegistrationPreparation.cancel();
            },
            .registrationWindow = [&nativeRegistrationStatus]()
                -> std::optional<std::pair<std::uint64_t, std::uint32_t>> {
                const auto anchor = nativeRegistrationStatus.snapshot();
                if (!anchor) return std::nullopt;
                return std::pair{anchor->nextBlock, anchor->window};
            },
            .existingFidForCustody = [&nativeRegistrationStatus](const QString& address) {
                return nativeRegistrationStatus.verifiedExistingFidForCustody(address);
            }
        });
    HyperSnapRegistrationPublisher& nativeRegistrationPublisher = *nativePublisherOwner;
    neynarRegistrationOwner = std::make_unique<NeynarRegistrationController>(
        QUrl{QString::fromUtf8(genycaster::deployment::publicOrigin.data())},
        FarcasterRegistrationPorts{
            .backupVerified = [&nativeCustodyWallet] {
                return nativeCustodyWallet.backupVerified();
            },
            .custodyAddress = [&nativeCustodyWallet] {
                return nativeCustodyWallet.address();
            },
            .signTransfer = [&nativeCustodyWallet](
                const std::uint64_t fid, const std::uint64_t nonce,
                const std::uint64_t deadline) {
                return nativeCustodyWallet.signReservedFarcasterTransfer(
                    fid, nonce, deadline);
            }
        });
    NeynarRegistrationController& neynarRegistration = *neynarRegistrationOwner;
    updateControllerOwner = std::make_unique<UpdateController>(
        settings, !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest, &application);
    UpdateController& updateController = *updateControllerOwner;
    const auto updateReleaseLocale = [&] {
        updateController.setLanguageTag(localizationController.effectiveLanguageTag());
    };
    updateReleaseLocale();
    QObject::connect(&localizationController, &LocalizationController::localeChanged,
                     &application, updateReleaseLocale);

    const auto applyApplicationLocaleFont = [&] {
        QFont localeFont = application.font();
        localeFont.setFamily(fontFamilies.forLanguage(localizationController.effectiveLanguageTag()));
        application.setFont(localeFont);
    };
    applyApplicationLocaleFont();
    QObject::connect(
        &localizationController, &LocalizationController::localeChanged,
        &application, applyApplicationLocaleFont);
#if defined(Q_OS_WASM)
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
#if defined(Q_OS_WASM)
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest
#else
        false
#endif
    };
    LinkPreviewResolver linkPreviewResolver{
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct};
    MarketController marketController{
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest};
    StatsController statsController{
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest};
    CastViewController castViewController{
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
        GENYCASTER_CAST_VIEWS_ENABLED != 0
            && !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest};
    BrowserReactionController browserReactionController{
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
#if defined(Q_OS_WASM)
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

#if defined(Q_OS_WASM) || defined(GENYCASTER_HAS_MOBILE_MINIAPP_WEBVIEW)
    // Browser/WASM and native mobile host public web content. Mobile does
    // not expose signer or wallet permissions through this public WebView.
    constexpr bool miniAppLaunchAvailable = true;
#else
    constexpr bool miniAppLaunchAvailable = GENYCASTER_HAS_MINIAPP_HOST != 0;
#endif
    miniAppHostControllerOwner = std::make_unique<MiniAppHostController>(
        miniAppLaunchAvailable, settings);
    MiniAppHostController& miniAppHost = *miniAppHostControllerOwner;
    miniAppHost.setNativeSdkAvailable(GENYCASTER_HAS_MINIAPP_HOST != 0);
    proWalletHostOwner = std::make_unique<MiniAppHostController>(true, settings);
    MiniAppHostController& proWalletHost = *proWalletHostOwner;
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
#if defined(Q_OS_WASM)
    // Keep upstream API credentials out of browser binaries and avoid CORS.
    // The first-party gateway only exposes bounded public catalogue reads.
    primaryMiniAppCatalogue = MiniAppCatalogEndpoint{
        .catalogueUrl = (QString::fromUtf8(genycaster::deployment::browserGateway.data()) + QStringLiteral("/v1/browser/miniapps?kind=catalog")),
        .searchUrl = (QString::fromUtf8(genycaster::deployment::browserGateway.data()) + QStringLiteral("/v1/browser/miniapps?kind=search")),
        .relevantUrl = (QString::fromUtf8(genycaster::deployment::browserGateway.data()) + QStringLiteral("/v1/browser/miniapps?kind=relevant")),
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
#if defined(Q_OS_WASM)
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
    // Browsing public channels and selecting signed compose destinations must
    // never mutate one another's search, membership or avatar model.
    channelBrowserControllerOwner = std::make_unique<ChannelPickerController>(
        settings, appliedNodeEndpoint, channelDirectoryBaseUrl,
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest, true, nullptr,
        farcasterReadApiKeyText);
    ChannelPickerController& channelBrowser = *channelBrowserControllerOwner;
    // Managed-signer UUIDs are injected only through the native bootstrap;
    // neither credentials nor signer ownership are writable from QML.
    // Public channel-follows reads may use the explicitly configured Neynar
    // catalogue credential when the active HyperSnap provider has no Neynar
    // read key. Never fall back to the public documentation token, and never
    // treat this read credential as a channel write authorization.
    const QString channelReadApiKey = farcasterReadApiKeyText.isEmpty()
        ? QString::fromUtf8(configuredMiniAppDiscoveryApiKey)
        : farcasterReadApiKeyText;
    channelAdministrationControllerOwner = std::make_unique<ChannelAdministrationController>(
        channelReadApiKey, qEnvironmentVariable("GENYCASTER_NEYNAR_SIGNER_UUID"),
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest,
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct);
    ChannelAdministrationController& channelAdmin = *channelAdministrationControllerOwner;
    miniAppTaskManagerControllerOwner = std::make_unique<MiniAppTaskManagerController>();
    MiniAppTaskManagerController& miniAppTaskManager = *miniAppTaskManagerControllerOwner;

    miniAppHost.setReduceMotion(appearanceController.reduceMotion());
    QObject::connect(&appearanceController, &AppearanceController::reduceMotionChanged, &miniAppHost,
        [&appearanceController, &miniAppHost] {
            miniAppHost.setReduceMotion(appearanceController.reduceMotion());
        });
    mediaWebHost.setReduceMotion(appearanceController.reduceMotion());
    QObject::connect(&appearanceController, &AppearanceController::reduceMotionChanged, &mediaWebHost,
        [&appearanceController, &mediaWebHost] {
            mediaWebHost.setReduceMotion(appearanceController.reduceMotion());
        });
#if FARCASTER_HAS_MINIAPP_EVM_WALLET
    {
        std::string nativeWalletConnectModule{};
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
        QFile moduleFile{QStringLiteral(":/genycaster/miniapp/genycaster-walletconnect.mjs")};
        if (moduleFile.open(QIODevice::ReadOnly))
            nativeWalletConnectModule = moduleFile.readAll().toStdString();
        if (nativeWalletConnectModule.empty()) {
            qWarning() << "Native Mini App WalletConnect resource unavailable";
        } else
#endif
        {
            auto walletBridge = farcaster::provider::compatibility::browser_wallet::bridge::makeBridge({
                .launch = (!nativeWalletConnectModule.empty() || walletLaunchTemplate.isEmpty())
                    ? farcaster::provider::compatibility::browser_wallet::bridge::WalletLauncher{
                        [host = QPointer<MiniAppHostController>{&miniAppHost}](const std::string& url)
                            -> foundation::result::Status {
                            if (!host) {
                                return std::unexpected(foundation::error::Error::make(
                                    foundation::error::ErrorCode::Unavailable,
                                    "Mini App wallet picker is no longer available."));
                            }
                            const QString sessionUrl = QString::fromStdString(url);
                            QMetaObject::invokeMethod(host, [host, sessionUrl] {
                                if (host) host->requestWalletBrowser(sessionUrl);
                            }, Qt::QueuedConnection);
                            return {};
                        }}
                    : makeWalletApprovalLauncher(walletLaunchTemplate),
                .supportedChains = genycaster::ui::bootstrap::miniAppEthereumChains(),
                .nativeApprovalSettled = [host = QPointer<MiniAppHostController>{&miniAppHost}]
                    (const std::string& url, bool successful) {
                        if (host) QMetaObject::invokeMethod(host, [host, sessionUrl = QString::fromStdString(url), successful] {
                            if (host) host->nativeWalletApprovalSettled(sessionUrl, successful);
                        }, Qt::QueuedConnection);
                    },
                .walletConnectModule = std::move(nativeWalletConnectModule),
            });
            if (walletBridge) {
                miniAppWalletBridgeOwner = std::move(*walletBridge);
            } else {
                qWarning().noquote() << "Mini App EIP-1193 wallet bridge unavailable:"
                                     << QString::fromStdString(walletBridge.error().message);
            }
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
        , QString::fromStdString(optimismRpcUrl.value_or("https://mainnet.optimism.io")).toStdString()
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

    genycaster::ui::bootstrap::connectNotificationBadge(
        application, settings, presentationBridge);

    richComposerControllerOwner = std::make_unique<RichComposerController>(settings);
    RichComposerController& richComposerController = *richComposerControllerOwner;
    emojiControllerOwner = std::make_unique<EmojiController>(settings);
    EmojiController& emojiController = *emojiControllerOwner;
    emojiRuntimeOwner = std::make_unique<farcaster::runtime::emoji::EmojiRuntime>(emojiController);
    mediaUploadControllerOwner = std::make_unique<MediaUploadController>();
    MediaUploadController& mediaUploadController = *mediaUploadControllerOwner;
#if defined(Q_OS_ANDROID)
    // Android SAF returns content://, not an uploadable local file URL.
    // The native picker imports a user-granted document into a private file.
    androidMediaPickerOwner = std::make_unique<AndroidComposerMediaPicker>(mediaUploadController);
    QObject::connect(&mediaUploadController, &MediaUploadController::browserImagePickRequested,
                     androidMediaPickerOwner.get(), &AndroidComposerMediaPicker::open);
#endif
    mediaClassifierControllerOwner = std::make_unique<MediaClassifierController>();
    MediaClassifierController& mediaClassifier = *mediaClassifierControllerOwner;
    mediaPlaybackCoordinatorOwner = std::make_unique<MediaPlaybackCoordinator>(settings);
    MediaPlaybackCoordinator& mediaPlayback = *mediaPlaybackCoordinatorOwner;
    // Connections in GenyVideoPlayer retain this context object as a signal
    // target. Keep its lifetime unequivocally C++-owned across QML teardown.
    QQmlEngine::setObjectOwnership(&mediaPlayback, QQmlEngine::CppOwnership);
    QObject::connect(&presentationBridge, &PresentationBridge::signOutRequested,
                     &mediaPlayback, &MediaPlaybackCoordinator::releaseAll);
    QObject::connect(&presentationBridge, &PresentationBridge::authenticationStateChanged,
        &mediaPlayback, [&presentationBridge, &mediaPlayback,
                         previousFid = presentationBridge.accountFid()]() mutable {
            const auto currentFid = presentationBridge.isAuthenticated()
                ? presentationBridge.accountFid() : decltype(presentationBridge.accountFid()){};
            if (previousFid != 0 && currentFid != previousFid)
                mediaPlayback.releaseAll();
            previousFid = currentFid;
        });
    QObject::connect(qGuiApp, &QGuiApplication::applicationStateChanged,
        &mediaPlayback, [&mediaPlayback](const Qt::ApplicationState state) {
            bool foreground = state != Qt::ApplicationHidden && state != Qt::ApplicationSuspended;
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(Q_OS_WASM)
            const auto windows = QGuiApplication::topLevelWindows();
            for (auto* window : windows) {
                if (window->objectName() == QStringLiteral("rootWindow")) {
                    foreground = foreground && window->isVisible()
                        && window->visibility() != QWindow::Minimized;
                    break;
                }
            }
#endif
            RemoteImageProvider::setForeground(foreground);
            if (!foreground) mediaPlayback.releaseAll();
        });
    QObject::connect(&localizationController, &LocalizationController::localeChanged,
        &presentationBridge, [&presentationBridge] {
            presentationBridge.retranslateTransientState();
        });
    genycaster::ui::bootstrap::connectNetworkRecovery(
        presentationBridge, !isSmokeTest && !isVisualTest);
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
#if defined(Q_OS_WASM)
        // Verification fixtures already provide the terminal account state
        // and bypass the browser restore worker.
        presentationBridge.finishBrowserAccountRestore();
#endif
    }
#if defined(Q_OS_ANDROID)
    // Android GPUs may render Qt Quick MultiEffect colorization as a filled
    // rectangle instead of preserving each SVG's alpha mask. Rasterize the
    // approved local Genyleap icon SVG with QtSvg and tint via SourceIn,
    // avoiding a second, device-dependent postprocessing shader pass.
    engine.addImageProvider(QStringLiteral("genycaster-icons"),
                            new AndroidTintedIconProvider{});
    engine.addImageProvider(QStringLiteral("genycaster-brand"),
                            new AndroidBrandImageProvider{});
#endif
    auto* const remoteAvatarImageProvider = new RemoteImageProvider{
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
        dataStorageController.runtimeStateHandle()};
    engine.addImageProvider(QStringLiteral("remote"), remoteAvatarImageProvider);
    // Stats must reuse the same bounded and cached avatar source as the UI;
    // a separate CDN allowlist rejects otherwise visible profile portraits.
    mediaUploadController.setStatsAvatarImageProvider(remoteAvatarImageProvider);
    genycaster::ui::bootstrap::exposeFontFamilies(engine, fontFamilies);
    GenyPulseActivityController genyPulseActivity;
    OpenProofConnectionsController openProofConnections{nullptr, &settings};
    // Only the trusted account-sync client owns account cookies and OpenProof
    // bearer tokens. Mini Apps never receive them or the server-side 0x key.
    QObject::connect(&openProofConnections,
                     &OpenProofConnectionsController::swapAvailabilityChanged,
                     &miniAppHost, &MiniAppHostController::setSwapAvailable);
    QObject::connect(&miniAppHost, &MiniAppHostController::swapQuoteNetworkRequested,
                     &openProofConnections, &OpenProofConnectionsController::requestSwapQuote);
    QObject::connect(&openProofConnections, &OpenProofConnectionsController::swapQuoteReceived,
                     &miniAppHost, &MiniAppHostController::receiveSwapQuoteResult);
    ProCheckoutController proCheckout{openProofConnections};
#if defined(Q_OS_WASM)
    genycaster::ui::identity::BrowserProWallet proBrowserWallet{proCheckout};
#endif
#if FARCASTER_HAS_MINIAPP_EVM_WALLET
    // Checkout has a separate, first-party, ephemeral wallet session.
    // Mini App origins, permissions and pending requests never cross into Pro.
    {
        auto checkoutBridge = farcaster::provider::compatibility::browser_wallet::bridge::makeBridge({
            .launch = [host=QPointer<MiniAppHostController>{&proWalletHost}]
                (const std::string& url) -> foundation::result::Status {
                if (!host) {
                    return std::unexpected(foundation::error::Error::make(
                        foundation::error::ErrorCode::Unavailable,
                        "GenyCaster Pro wallet chooser is unavailable."));
                }
                const QString localUrl = QString::fromStdString(url);
                QMetaObject::invokeMethod(host, [host,localUrl] {
                    if (host) host->requestWalletBrowser(localUrl);
                }, Qt::QueuedConnection);
                return {};
            },
            .supportedChains = {"eip155:8453"}
        });
        if (checkoutBridge) {
            proWalletBridgeOwner = std::move(*checkoutBridge);
            auto* bridge = proWalletBridgeOwner.get();
            auto sequence = std::make_shared<std::atomic<std::uint64_t>>(0);
            proCheckout.setWalletTransport(
                [bridge,&proWalletHost] {
                    proWalletHost.setWalletBrowserContext(
                        QStringLiteral("GenyCaster Pro"),
                        QString::fromUtf8(genycaster::deployment::publicOrigin.data()));
                    const auto session=bridge->beginSession(
                        "GenyCaster Pro",std::string{genycaster::deployment::publicOrigin});
                    if (!session) proWalletHost.walletBrowserSessionEnded();
                    return session.has_value();
                },
                [bridge,&proWalletHost,sequence](
                    QString method,QJsonArray params,ProCheckoutController::RpcComplete complete) {
                    const auto data=QJsonDocument{params}.toJson(QJsonDocument::Compact);
                    bridge->request({
                        .id = ++(*sequence),
                        .method = method.toStdString(),
                        .paramsJson = data.toStdString()
                    }, [host=QPointer<MiniAppHostController>{&proWalletHost},
                        complete=std::move(complete)](
                            farcaster::capability::wallet::EthereumRpcResponse response) mutable {
                        const bool success=!response.error.has_value();
                        if (host)
                            QMetaObject::invokeMethod(host, [host,success] {
                                if (host)host->walletBrowserRequestFinished(success);
                            },Qt::QueuedConnection);
                        if (!success) {
                            complete({},response.error->code==4001
                                ? QStringLiteral("wallet_rejected")
                                : QStringLiteral("wallet_failed"));
                            return;
                        }
                        const QByteArray encoded=QByteArray::fromStdString(response.resultJson);
                        const auto parsed=QJsonDocument::fromJson(
                            QByteArrayLiteral("[")+encoded+QByteArrayLiteral("]"));
                        if (!parsed.isArray() || parsed.array().isEmpty()) {
                            complete({},QStringLiteral("wallet_invalid_response"));
                            return;
                        }
                        complete(parsed.array().first(),{});
                    });
                },
                [bridge,&proWalletHost] {
                    bridge->endSession();
                    proWalletHost.walletBrowserSessionEnded();
                });
        }
    }
    QObject::connect(&proWalletHost,
                     &MiniAppHostController::walletBrowserCancelRequested,
                     &proCheckout, &ProCheckoutController::cancel);
#endif
    QObject::connect(&proCheckout, &ProCheckoutController::transactionSubmitted,
                     &openProofConnections, &OpenProofConnectionsController::verifyProPaymentForQuote);

    genyPulseActivity.setAccountConnections(&openProofConnections);
    // The market cap follows a server-verified GENY balance, never the
    // client's public market-info estimate or a QML-supplied wallet.
    QObject::connect(&openProofConnections,
                     &OpenProofConnectionsController::proEntitlementsChanged,
                     &liveInsightsController, [&openProofConnections, &liveInsightsController] {
        const auto status = openProofConnections.proEntitlements();
        // A transient grant refresh or an unavailable balance provider is
        // not an authoritative revocation of a previously verified Pro tier.
        if (status.value(QStringLiteral("status")).toString() != QStringLiteral("verified"))
            return;
        liveInsightsController.setProMarketLimit(
            status.value(QStringLiteral("eligible")).toBool() ? 16 : 4);
    });
#if GENYCASTER_HAS_MINIAPP_HOST
    QObject::connect(&openProofConnections,
                     &OpenProofConnectionsController::proEntitlementsChanged,
                     &openProofConnections,
                     [&openProofConnections, &miniAppRuntimeOwner] {
        if (!miniAppRuntimeOwner) return;
        const auto status = openProofConnections.proEntitlements();
        // Unknown/unavailable verification must not evict running Mini Apps.
        // Only a fresh, server-confirmed entitlement may change the limit.
        if (status.value(QStringLiteral("status")).toString() != QStringLiteral("verified"))
            return;
        miniAppRuntimeOwner->setSessionLimit(
            status.value(QStringLiteral("eligible")).toBool() ? 4 : 1);
    });
#endif
    QObject::connect(&openProofConnections,
                     &OpenProofConnectionsController::proEntitlementsChanged,
                     &miniAppHost, [&openProofConnections, &miniAppHost] {
        const auto status = openProofConnections.proEntitlements();
        if (status.value(QStringLiteral("status")).toString() != QStringLiteral("verified"))
            return;
        miniAppHost.setSessionLimit(
            status.value(QStringLiteral("eligible")).toBool() ? 4 : 1);
    });
    QObject::connect(&openProofConnections,
                     &OpenProofConnectionsController::stateChanged,
                     &openProofConnections, [&openProofConnections] {
        if (openProofConnections.cloudAuthorizationReady()) {
            openProofConnections.fetchProEntitlements();
#if defined(Q_OS_WASM)
            openProofConnections.fetchGenyAssets();
#endif
        }
    });
    const auto initialFid = presentationBridge.isAuthenticated()
        ? presentationBridge.accountFid() : 0;
    // A Pro allowance must never bleed into another FID on shared devices.
    qulonglong miniAppEntitlementFid = initialFid;
    QObject::connect(&presentationBridge, &PresentationBridge::authenticationStateChanged,
        &openProofConnections, [&presentationBridge, &openProofConnections,
                                &richComposerController, &miniAppHost, &miniAppEntitlementFid
#if GENYCASTER_HAS_MINIAPP_HOST
                                , &miniAppRuntimeOwner
#endif
                                ] {
            const auto fid = presentationBridge.isAuthenticated()
                ? presentationBridge.accountFid() : 0;
            if (fid != miniAppEntitlementFid) {
                miniAppEntitlementFid = fid;
                // End every session, including minimized documents and wallet
                // authority, before a new account may receive its own limit.
                miniAppHost.closeAll();
                miniAppHost.setSessionLimit(1);
#if GENYCASTER_HAS_MINIAPP_HOST
                if (miniAppRuntimeOwner) miniAppRuntimeOwner->setSessionLimit(1);
#endif
            }
            // Account-scoped unpublished drafts must never carry over when the
            // active Farcaster identity changes on a shared device.
            richComposerController.setViewerFid(fid);
            openProofConnections.setViewerPrincipal(fid,fid
                ? (presentationBridge.nativeCustodyAccount()
                    ? QStringLiteral("hypersnap") : QStringLiteral("farcaster"))
                : QString{});
        });
    richComposerController.setViewerFid(initialFid);
    openProofConnections.setViewerPrincipal(initialFid,initialFid
        ? (presentationBridge.nativeCustodyAccount()
            ? QStringLiteral("hypersnap") : QStringLiteral("farcaster"))
        : QString{});
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
    // Borrowed QML controller pointers remain valid until the QML roots
    // are released; registration transfers no ownership.
    if (!genycaster::ui::bootstrap::exposeControllers(engine, {
        {"genyPulseActivity", &genyPulseActivity},
        {"openProofConnections", &openProofConnections},
        {"proCheckout", &proCheckout},
        {"proWalletHost", &proWalletHost},
        {"presentationBridge", &presentationBridge},
        {"channelDirectory", &channelPickerController},
        {"channelBrowser", &channelBrowser},
        {"channelAdmin", &channelAdmin},
        {"dataStorage", &dataStorageController},
        {"richComposer", &richComposerController},
        {"emojiController", &emojiController},
        {"mediaUpload", &mediaUploadController},
        {"mediaClassifier", &mediaClassifier},
        {"mediaPlayback", &mediaPlayback},
        {"localizationController", &localizationController},
        {"geoLocationResolver", &geoLocationResolver},
        {"linkPreviewResolver", &linkPreviewResolver},
        {"browserLinkMetadataController", &browserLinkMetadataController},
        {"browserTokenPreviewController", &browserTokenPreviewController},
        {"castViewController", &castViewController},
        {"statsController", &statsController},
        {"marketController", &marketController},
        {"browserReactionController", &browserReactionController},
        {"miniAppHost", &miniAppHost},
        {"miniAppCatalog", &miniAppCatalog},
        {"miniAppTaskManager", &miniAppTaskManager},
        {"liveInsights", &liveInsightsController},
        {"nodeSelectionController", &nodeSelectionController},
        {"providerProfiles", &providerProfilesController},
        {"nativeRegistrationStatus", &nativeRegistrationStatus},
        {"nativeCustodyWallet", &nativeCustodyWallet},
        {"nativeRegistrationPreparation", &nativeRegistrationPreparation},
        {"nativeRegistrationPublisher", &nativeRegistrationPublisher},
        {"neynarRegistration", &neynarRegistration},
        {"updateController", &updateController},
        {"appearanceController", &appearanceController},
        {"renderingController", &renderingController},
        {"textSelectionSettings", &textSelectionSettings},
    })) return 2;
    // Headless QPA backends lack an audio output device.
    const QString qpaBackend = QGuiApplication::platformName();
#if defined(Q_OS_WASM)
    constexpr bool webReadOnly = true;
#else
    constexpr bool webReadOnly = false;
#endif
    genycaster::ui::bootstrap::exposePlatformFlags(
        engine, webReadOnly, qpaBackend != QStringLiteral("offscreen")
                               && qpaBackend != QStringLiteral("minimal"));
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
#if defined(Q_OS_WASM)
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
#if defined(Q_OS_WASM)
        EM_ASM({
            window.__genycasterQmlStartup?.(false,
                "Qt Main did not produce a QQuickWindow");
        });
#endif
        return 1;
    }
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(Q_OS_WASM)
    std::unique_ptr<genycaster::ui::DesktopBackgroundController> desktopBackground;
    if (!isSmokeTest && !isVisualTest && !isRuntimeSmokeTest) {
        desktopBackground = std::make_unique<genycaster::ui::DesktopBackgroundController>(
            application, *mainWindow);
        desktopInstance.setActivationHandler(
            [controller = QPointer<genycaster::ui::DesktopBackgroundController>{desktopBackground.get()}] {
                if (controller) controller->showMainWindow();
            });
    }
    desktopInstance.setWindow(mainWindow);
    // A hidden/minimized main window must not keep decoders alive just
    // because an auxiliary native window leaves the application active.
    QObject::connect(mainWindow, &QWindow::visibilityChanged, &mediaPlayback,
        [&mediaPlayback](QWindow::Visibility visibility) {
            const bool foreground = visibility != QWindow::Hidden &&
                visibility != QWindow::Minimized &&
                QGuiApplication::applicationState() != Qt::ApplicationHidden &&
                QGuiApplication::applicationState() != Qt::ApplicationSuspended;
            RemoteImageProvider::setForeground(foreground);
            if (!foreground) mediaPlayback.releaseAll();
        });
#endif
    renderingController.attachWindow(mainWindow);
#if defined(Q_OS_WASM)
    qInfo() << "Genycaster WebAssembly Qt Quick renderer:"
            << mainWindow->rendererInterface()->graphicsApi();
#endif
#if defined(Q_OS_ANDROID)
    if (auto* shell = mainWindow->findChild<QObject*>(QStringLiteral("appShell"))) {
        mainWindow->installEventFilter(new AndroidBackDispatcher{mainWindow, shell});
    }
#endif
#if (defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID) && defined(GENYCASTER_HAS_LINUX_MINIAPP_WIDGETS)) \
 || (defined(Q_OS_WIN) && defined(GENYCASTER_HAS_WINDOWS_MINIAPP_HOST))
    // Native in-window Mini App drawers must use the stable Qt Quick host,
    // not whichever transient Mini App window most recently gained focus.
    farcaster::platform::miniapp::setLinuxMiniAppQuickHost(mainWindow, &engine);
#endif
#if defined(Q_OS_WASM)
    genycaster::ui::web::installWheelBridge(mainWindow);
    genycasterInstallBrowserEmojiRecovery();
    // This is the only reliable readiness event: the Main.qml object tree
    // exists, rather than just the Emscripten/Qt runtime being initialized.
    EM_ASM({ window.__genycasterQmlStartup?.(true, ""); });
#endif

    // Incoming links are resolved only once the cache-first runtime can serve
    // cast/profile navigation. Browser history changes are accepted without
    // reloading WASM; desktop installers may forward --open-url=<https URL>.
    QString lastIncomingLink{};
    QString nativeIncomingLink = optionValue(arguments, QStringLiteral("--open-url="));
    auto dispatchIncomingLink = [&presentationBridge, &lastIncomingLink, &nativeIncomingLink] {
        if (!presentationBridge.runtimeReady()
            || presentationBridge.initialSetupRequired()) return;
        QString incoming = nativeIncomingLink;
#if defined(Q_OS_WASM)
        const char* const href = emscripten_run_script_string("window.location.href");
        incoming = href != nullptr ? QString::fromUtf8(href) : QString{};
#endif
        if (incoming.isEmpty() || incoming == lastIncomingLink) return;
        lastIncomingLink = incoming;
        static_cast<void>(presentationBridge.openSharedLink(incoming));
    };
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(Q_OS_WASM)
    // Reuse the existing deep-link route after a second desktop launch.
    // If the primary is still bootstrapping, dispatchIncomingLink defers
    // navigation until the runtime/setup state allows opening the URL.
    desktopInstance.setOpenUrlHandler(
        [&nativeIncomingLink, &lastIncomingLink, &dispatchIncomingLink](
            const QString& forwardedUrl) {
            nativeIncomingLink = forwardedUrl;
            lastIncomingLink.clear();
            dispatchIncomingLink();
        });
#endif
    QObject::connect(&presentationBridge, &PresentationBridge::runtimeStateChanged,
                     &application, dispatchIncomingLink);
    QObject::connect(&presentationBridge, &PresentationBridge::setupStateChanged,
                     &application, dispatchIncomingLink);
#if defined(Q_OS_WASM)
    QTimer incomingLinkPoll{&application};
    incomingLinkPoll.setInterval(750);
    QObject::connect(&incomingLinkPoll, &QTimer::timeout, &application, dispatchIncomingLink);
    incomingLinkPoll.start();
#endif

    std::shared_ptr<Runtime> runtime{};
#if !defined(Q_OS_WASM)
    // Join this native worker before destroying any controller it borrows.
    genycaster::ui::bootstrap::BootstrapWorker bootstrap{};
#endif
    if (!isSmokeTest && !isVisualTest) {
        const QString databasePathText = optionValue(
            arguments, QStringLiteral("--database-path="));
        const std::filesystem::path databasePath = databasePathText.isEmpty()
            ? std::filesystem::path{
                  QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toStdString()}
                  / "genycaster.sqlite3"
            : std::filesystem::path{databasePathText.toStdString()};
        // Capture one immutable policy snapshot for this startup attempt.
        // Read and Write routes, signer attestation and ephemeral credentials
        // remain separately named, and the owned snapshot never enters QML.
        auto startupOptions = genycaster::ui::runtime::RuntimeConfiguration{
            .databasePath = databasePath,
            .networkProxyPolicy = networkProxyPolicy,
            .providerBaseUrl = providerBaseUrl,
            .hyperSnapNativeNodeBaseUrl = nativeRegistrationEndpoint.toStdString(),
            .farcasterReadBaseUrl = farcasterReadBaseUrl,
            .farcasterReadApiKey = farcasterReadApiKey,
            .additionalReadProviders = additionalReadProviders,
            .readProviderApiKeys = readProviderApiKeys,
            .preferredReadProviderId = preferredReadProviderId,
            .allowCompatibleReadFallback = allowCompatibleReadFallback,
            .publicReadPool = usePublicReadPool,
            .additionalWriteProviders = additionalWriteProviders,
            .preferredWriteProviderId = preferredWriteProviderId,
            .authRelayUrl = authRelayUrl,
            .optimismRpcUrl = optimismRpcUrl,
            .snapchainBaseUrl = snapchainBaseUrl,
            .snapchainWriteBaseUrl = snapchainWriteBaseUrl,
            .mediaGatewayBaseUrl = mediaGatewayBaseUrl,
            .snapchainAuthorizationHeader = snapchainAuthorizationHeader,
            .snapchainAuthorizationValue = snapchainAuthorizationValue,
            .snapchainWriteAuthorizationHeader = snapchainWriteAuthorizationHeader,
            .snapchainWriteAuthorizationValue = snapchainWriteAuthorizationValue,
            .appFid = appFid,
            .signerAttestationUrl = signerAttestationUrl,
            .approvalLauncher = makeWalletApprovalLauncher(walletLaunchTemplate, true),
        };
        auto bootstrapRuntime = [&presentationBridge, &richComposerController,
                                 &openProofConnections, &mediaUploadController,
                                 &mediaPlayback, &runtime,
                                 &nativeRegistrationPublisher, &neynarRegistration,
                                 &nativeCustodyWallet,
                                 startupOptions = std::move(startupOptions),
                                 siwfDomain, siwfUri, startupIdentity,
                                 snapchainBaseUrl, authenticationAvailable,
                                 openExploreForTest, compiledHypersnapEndpoint](const QString& selectedNativeOrigin, const QVariantList& directoryRows) mutable {
            startupOptions.hyperSnapNativeNodeBaseUrl = selectedNativeOrigin.toStdString();
            if (startupOptions.publicReadPool) {
                // Only this verified startup snapshot can admit public peers;
                // custom profiles can never become automatic fallback targets.
                for (const auto& value : directoryRows) {
                    const auto row = value.toMap();
                    const auto id = row.value(QStringLiteral("id")).toString().toStdString();
                    const auto endpoint = row.value(QStringLiteral("endpoint")).toString().toStdString();
                    if (!row.value(QStringLiteral("enabled")).toBool()
                        || row.value(QStringLiteral("contract")).toString() != QStringLiteral("hypersnap-v2")
                        || endpoint == compiledHypersnapEndpoint.toStdString()
                        || std::any_of(startupOptions.additionalReadProviders.cbegin(),
                            startupOptions.additionalReadProviders.cend(), [&](const auto& existing) {
                                return existing.id == id || existing.endpoint == endpoint;
                            })) continue;
                    startupOptions.additionalReadProviders.push_back({
                        .id = id, .displayName = row.value(QStringLiteral("name")).toString().toStdString(),
                        .endpoint = endpoint, .adapterContract = "hypersnap-v2", .selfHosted = false,
                    });
                    startupOptions.publicReadPeerIds.push_back(id);
                }
            }
            auto made = makeRuntime(std::move(startupOptions));
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
                 &mediaPlayback, &runtime,
                 &nativeRegistrationPublisher, &neynarRegistration,
                 &nativeCustodyWallet,
                 readyRuntime = std::move(readyRuntime),
                 siwfDomain, siwfUri, startupIdentity,
                 snapchainBaseUrl, authenticationAvailable, openExploreForTest]() mutable {
                    // The Qt presentation facade owns GUI-thread attachment;
                    // the bootstrap worker never publishes partial runtime state.
                    genycaster::ui::bindings::attachRuntime({
                        .presentationBridge = presentationBridge,
                        .richComposerController = richComposerController,
                        .openProofConnections = openProofConnections,
                        .mediaUploadController = mediaUploadController,
                        .mediaPlayback = mediaPlayback,
                        .runtime = runtime,
                        .siwfDomain = std::move(siwfDomain),
                        .siwfUri = std::move(siwfUri),
                        .browserSnapchainOrigin = snapchainBaseUrl
                            ? QString::fromStdString(*snapchainBaseUrl)
                            : QString::fromUtf8(genycaster::deployment::snapchainOrigin.data()),
                        .startupIdentity = std::move(startupIdentity),
                        .nativePublisher = &nativeRegistrationPublisher,
                        .farcasterPublisher = &neynarRegistration,
                        .nativeWallet = &nativeCustodyWallet,
                        .authenticationAvailable = authenticationAvailable,
                        .openExploreForTest = openExploreForTest,
                    }, std::move(readyRuntime));
                }, Qt::QueuedConnection);
        };
        auto launchRuntime = [&application, &settings, &nativeRegistrationStatus,
                              &nativeRegistrationPreparation, &nativeRegistrationPublisher,
                              &nativeRegistrationEndpoint, &providerProfilesController, automaticRegistration,
#if !defined(Q_OS_WASM)
                              &bootstrap,
#endif
                              bootstrapRuntime = std::move(bootstrapRuntime)](QString selected) mutable {
            const QString original = nativeRegistrationEndpoint;
            if (selected != original) {
                const QUrl candidate{selected};
                nativeRegistrationPreparation.cancel();
                if (!nativeRegistrationPublisher.setEndpointBeforeRuntime(candidate)
                    || !nativeRegistrationStatus.setEndpointBeforeRuntime(candidate)) {
                    (void)nativeRegistrationPublisher.setEndpointBeforeRuntime(QUrl{original});
                    (void)nativeRegistrationStatus.setEndpointBeforeRuntime(QUrl{original});
                    selected = original;
                }
            }
            if (automaticRegistration) {
                // Persist the origin before any native registration can submit.
                // A pending journal then restores this exact origin after restart.
                settings.setValue(QStringLiteral("nativeRegistration/lastRuntimeOrigin"), selected);
                settings.sync();
                if (settings.status() != QSettings::NoError && selected != original) {
                    (void)nativeRegistrationPublisher.setEndpointBeforeRuntime(QUrl{original});
                    (void)nativeRegistrationStatus.setEndpointBeforeRuntime(QUrl{original});
                    selected = original;
                }
            }
            nativeRegistrationEndpoint = selected;
            providerProfilesController.setRegistrationNode(selected);
            auto start = [bootstrapRuntime = std::move(bootstrapRuntime),
                          selected = std::move(selected), directoryRows = providerProfilesController.directoryNodes()]() mutable {
                bootstrapRuntime(selected, directoryRows);
            };
#if defined(Q_OS_WASM)
            QTimer::singleShot(0, &application, std::move(start));
#else
            bootstrap.start(std::move(start));
#endif
        };
        if (automaticRegistration) {
            // Wait for verified live/cached/embedded registry before Setup
            // preflight. Do not transfer authority to newly listed read nodes.
            QObject::connect(&providerProfilesController, &ProviderProfilesController::directoryRefreshFinished,
                &application, [&application, &providerProfilesController, &nativeRegistrationEndpoint, hasPendingNativeRegistration,
                               launch = std::move(launchRuntime)]() mutable {
                    if (hasPendingNativeRegistration) { launch(nativeRegistrationEndpoint); return; }
                    auto origins = providerProfilesController.publicRegistrationCandidates();
                    if (origins.isEmpty()) { launch(nativeRegistrationEndpoint); return; }
                    NativeRegistrationStatusController::selectBeforeRuntime(
                        origins, &application, std::move(launch), 5000,
                        [&providerProfilesController](QVariantList rows) {
                            providerProfilesController.setRegistrationPreflight(rows);
                        });
                }, Qt::SingleShotConnection);
            QTimer::singleShot(0, &providerProfilesController, &ProviderProfilesController::refreshDirectory);
        } else {
            launchRuntime(nativeRegistrationEndpoint);
            if (enableHypersnapDefault && !isRuntimeSmokeTest)
                QTimer::singleShot(0, &providerProfilesController, &ProviderProfilesController::refreshDirectory);
        }
    }

    if (automaticRegistration) {
        QObject::connect(&providerProfilesController, &ProviderProfilesController::directoryRefreshFinished,
            &application, [&application, &presentationBridge, &providerProfilesController] {
                if (!presentationBridge.runtimeReady() || presentationBridge.isAuthenticated()) return;
                const auto origins = providerProfilesController.publicRegistrationCandidates();
                if (origins.isEmpty()) return;
                NativeRegistrationStatusController::selectBeforeRuntime(
                    origins, &application, [](QString) {}, 5000,
                    [&providerProfilesController](QVariantList rows) {
                        providerProfilesController.setRegistrationPreflight(rows);
                    });
            });
    }
    QTimer directoryRefreshTimer{&application};
    directoryRefreshTimer.setInterval(240000);
    QObject::connect(&directoryRefreshTimer, &QTimer::timeout, &providerProfilesController,
        [&providerProfilesController] {
            if (QGuiApplication::applicationState() == Qt::ApplicationActive)
                providerProfilesController.refreshDirectory();
        });
    if (enableHypersnapDefault && !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest)
        directoryRefreshTimer.start();

    const int smokeCheck = genycaster::ui::diagnostics::configureSmokeTests(
        application, engine, presentationBridge, isSmokeTest, isRuntimeSmokeTest);
    if (smokeCheck != 0) return smokeCheck;
    const int snapshotSetup = genycaster::ui::diagnostics::scheduleSnapshots(
        application, engine, presentationBridge, richComposerController,
        arguments, isVisualTest, openExploreForTest, snapshotPath);
    if (snapshotSetup != 0) return snapshotSetup;
#if defined(Q_OS_WASM)
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
#if !defined(Q_OS_WASM)
    // Qt's event dispatchers and QPA/scene-graph resources still exist here.
    // If we leave QML root destruction to QQmlApplicationEngine's destructor,
    // it runs during stack unwinding after the shared runtime has been
    // released, while async Loaders may still be incubating. The controller
    // owners outlive the engine, but their runtime dependencies need not.
    // Stop producers, finish bootstrap, and dismantle the QML object tree
    // before either the runtime or the QGuiApplication can disappear.
    renderingController.beginShutdown();
    shutdownScope.stop();
    bootstrap.join();
    genycaster::ui::bootstrap::releaseQmlRoots(engine, mainWindow);
#endif
    return exitCode;
}
