#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QEvent>
#include <QPointer>
#include <QDir>
#include <QDebug>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkInformation>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#if defined(Q_OS_ANDROID)
#include "android_tinted_icon_provider.hpp"
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
import genycaster.ui.shutdown;
import genycaster.ui.bootstrap;
import genycaster.ui.bootstrap.fonts;
import genycaster.ui.diagnostics.snapshots;
import genycaster.ui.diagnostics.smoke;
import genycaster.ui.web.wheel;
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(__EMSCRIPTEN__)
import genycaster.ui.desktop.instance;
#endif
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
import farcaster.provider.hypersnap.config;
#endif
#if FARCASTER_HAS_MINIAPP_EVM_WALLET
import farcaster.capability.wallet;
import farcaster.provider.compatibility.browser_wallet.bridge;
#endif

#include "runtime_ui_bindings.hpp"


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

        const bool preferInstalledWalletBrowser = preferCustodyWalletBrowser && launchTemplate.isEmpty()
            && parsed.scheme() == QStringLiteral("http")
            && parsed.host() == QStringLiteral("127.0.0.1");
        const auto open = [parsed, preferInstalledWalletBrowser] {
            bool opened{false};
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
            return opened;
        };
#if defined(__EMSCRIPTEN__)
        const bool opened = open();
#else
        const bool opened = genycaster::ui::shutdown::invokeOnApplication(open);
#endif
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
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(__EMSCRIPTEN__)
    // Desktop window/task-switcher identity complements the bundled native
    // Windows PE and macOS application resources.
    QGuiApplication::setWindowIcon(QIcon{QStringLiteral(":/genycaster/app-icon.png")});
#endif
    QQuickStyle::setStyle(QStringLiteral(FARCASTER_QT_QUICK_CONTROLS_STYLE));
    genycaster::ui::bootstrap::FontFamilies fontFamilies{};
    if (!fontFamilies.load()) return 2;


    const QStringList arguments = QCoreApplication::arguments();
    const bool isSmokeTest = arguments.contains(QStringLiteral("--smoke-test"));
    const bool isVisualTest = arguments.contains(QStringLiteral("--visual-test"));
    const bool isRuntimeSmokeTest = arguments.contains(QStringLiteral("--runtime-smoke-test"));
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(__EMSCRIPTEN__)
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

    // The primary Farcaster message writer is selected by the active
    // protocol, not by the historical name of the Snapchain-compatible
    // /v1/ adapter. HyperSnap also accepts canonical signed protobuf at
    // /v1/submitMessage. Never silently send a HyperSnap user's block,
    // reaction or follow to a different Snapchain origin.
    const bool primaryProtocolWriter =
        !preferredWriteProviderId || *preferredWriteProviderId == "primary-protocol-writer";
    if (primaryProtocolWriter && preferredReadProviderId) {
        QString selectedProtocolOrigin{};
        if (*preferredReadProviderId == "hypersnap-v2") {
            selectedProtocolOrigin = appliedNodeEndpoint;
        } else if (*preferredReadProviderId != "farcaster-snapchain") {
            const auto match = std::find_if(
                additionalReadProviders.cbegin(), additionalReadProviders.cend(),
                [&](const genycaster::ui::runtime::ReadProviderProfile& profile) {
                    return profile.id == *preferredReadProviderId;
                });
            if (match != additionalReadProviders.cend()
                && (match->adapterContract == "hypersnap-v2"
                    || match->adapterContract == "snapchain-v1")) {
                selectedProtocolOrigin = QString::fromStdString(match->endpoint);
            } else {
                // Incompatible/unknown read contracts cannot be treated as
                // canonical write endpoints. Refuse rather than cross-post.
                qCritical().noquote()
                    << "Selected protocol has no compatible canonical message writer.";
                return 2;
            }
        }
        if (!selectedProtocolOrigin.isEmpty()) {
            if (!isValidHttpServiceUrl(selectedProtocolOrigin)) {
                qCritical().noquote() << "Selected protocol writer URL is invalid.";
                return 2;
            }
            // Both signer registry and signed writes must use the SAME
            // selected node. Caller may still choose an explicit separate
            // write provider through network/preferredWriteProviderId.
            snapchainBaseUrlText = selectedProtocolOrigin;
            snapchainWriteBaseUrlText = selectedProtocolOrigin;
        }
    }
    if (!snapchainWriteBaseUrlText.isEmpty()
        && !isValidHttpServiceUrl(snapchainWriteBaseUrlText)) {
        qCritical().noquote() << "Snapchain validator/write URL must be a credential-free HTTP(S) origin.";
        return 2;
    }
    const std::optional<std::string> snapchainBaseUrl{snapchainBaseUrlText.toStdString()};
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
#if !defined(__EMSCRIPTEN__)
    // Declared after the engine and controller owners so early-return cleanup
    // joins global workers before their borrowed presentation objects disappear.
    genycaster::ui::shutdown::ShutdownScope shutdownScope{};
#endif

    appearanceControllerOwner = std::make_unique<AppearanceController>(
        settings, std::move(appearanceOverrides));
    AppearanceController& appearanceController = *appearanceControllerOwner;
#if defined(__EMSCRIPTEN__)
    // Browser chrome uses the HTML document theme independently of Qt Quick's
    // canvas. Keep it in sync with the effective app appearance, including
    // manual overrides and system changes.
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

    const auto applyApplicationLocaleFont = [&] {
        QFont localeFont = application.font();
        localeFont.setFamily(fontFamilies.forLanguage(localizationController.effectiveLanguageTag()));
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
            .launch = walletLaunchTemplate.isEmpty()
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
    engine.rootContext()->setContextProperty(QStringLiteral("platformTextFontFamily"), fontFamilies.systemText());
    engine.rootContext()->setContextProperty(QStringLiteral("platformRtlFontFamily"), fontFamilies.systemRtl());
    engine.rootContext()->setContextProperty(QStringLiteral("platformDevanagariFontFamily"), fontFamilies.systemDevanagari());
    engine.rootContext()->setContextProperty(QStringLiteral("platformJapaneseFontFamily"), fontFamilies.systemJapanese());
    engine.rootContext()->setContextProperty(QStringLiteral("platformKoreanFontFamily"), fontFamilies.systemKorean());
    engine.rootContext()->setContextProperty(QStringLiteral("platformChineseFontFamily"), fontFamilies.systemChinese());
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledPersianFontFamily"), fontFamilies.persian());
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledDevanagariFontFamily"), fontFamilies.devanagari());
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledJapaneseFontFamily"), fontFamilies.japanese());
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledKoreanFontFamily"), fontFamilies.korean());
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledChineseFontFamily"), fontFamilies.chinese());
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledIconFontFamily"), fontFamilies.iconSolid());
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledIconRegularFontFamily"), fontFamilies.iconRegular());
    GenyPulseActivityController genyPulseActivity;
    engine.rootContext()->setContextProperty(
        QStringLiteral("genyPulseActivity"), &genyPulseActivity);
    OpenProofConnectionsController openProofConnections{nullptr, &settings};
    genyPulseActivity.setAccountConnections(&openProofConnections);
    engine.rootContext()->setContextProperty(
        QStringLiteral("openProofConnections"), &openProofConnections);
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
#if defined(__EMSCRIPTEN__)
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
            openProofConnections.setViewerFid(fid);
        });
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
    // QAudioOutput probes host audio devices. The
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
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(__EMSCRIPTEN__)
    desktopInstance.setWindow(mainWindow);
#endif
    renderingController.attachWindow(mainWindow);
#if defined(__EMSCRIPTEN__)
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
#if defined(__EMSCRIPTEN__)
        const char* const href = emscripten_run_script_string("window.location.href");
        incoming = href != nullptr ? QString::fromUtf8(href) : QString{};
#endif
        if (incoming.isEmpty() || incoming == lastIncomingLink) return;
        lastIncomingLink = incoming;
        static_cast<void>(presentationBridge.openSharedLink(incoming));
    };
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(__EMSCRIPTEN__)
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

    const int smokeCheck = genycaster::ui::diagnostics::configureSmokeTests(
        application, engine, presentationBridge, isSmokeTest, isRuntimeSmokeTest);
    if (smokeCheck != 0) return smokeCheck;
    const int snapshotSetup = genycaster::ui::diagnostics::scheduleSnapshots(
        application, engine, presentationBridge, richComposerController,
        arguments, isVisualTest, openExploreForTest, snapshotPath);
    if (snapshotSetup != 0) return snapshotSetup;
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
#if !defined(__EMSCRIPTEN__)
    // Qt's event dispatchers and QPA/scene-graph resources still exist here.
    // If we leave QML root destruction to QQmlApplicationEngine's destructor,
    // it runs during stack unwinding after the shared runtime has been
    // released, while async Loaders may still be incubating. The controller
    // owners outlive the engine, but their runtime dependencies need not.
    // Stop producers, finish bootstrap, and dismantle the QML object tree
    // before either the runtime or the QGuiApplication can disappear.
    shutdownScope.stop();
    if (bootstrap.worker.joinable()) {
        bootstrap.worker.join();
    }
    if (mainWindow != nullptr) {
        mainWindow->setPersistentGraphics(false);
        mainWindow->setPersistentSceneGraph(false);
        mainWindow->hide();
        mainWindow->releaseResources();
    }
    // QQmlApplicationEngine tracks root deletion via QObject::destroyed.
    // Do not use deleteLater(): the main event loop has already stopped.
    for (QObject* const rootObject : engine.rootObjects()) {
        delete rootObject;
    }
    // Drain QML/Qt deferred child cleanup while a GUI dispatcher remains
    // installed; do not spin another event loop or restart network work.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    engine.collectGarbage();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
#endif
    return exitCode;
}
