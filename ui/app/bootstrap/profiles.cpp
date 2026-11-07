/*!
 * @file        ui/app/bootstrap/profiles.cpp
 * @brief       Provider profile parsing and validation implementation.
 *
 * @details
 * Parses explicit Read/Write adapter instances, optional session-only API
 * credentials and preferred protocol routes. Enforces unique identifiers,
 * credential-free public endpoints and HTTPS credential isolation.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <QByteArray>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(Q_OS_WASM)
#include <emscripten.h>
#endif

module genycaster.ui.bootstrap.profiles;

import genycaster.ui.bootstrap;

namespace genycaster::ui::bootstrap {

std::optional<ProviderProfileConfiguration> loadProviderProfiles(
    QSettings& settings, const QStringList& arguments, const bool enableHypersnapDefault)
{
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
    std::vector<ReadProviderProfile> additionalReadProviders{};
    if (!providerProfilesJson.isEmpty()) {
        QJsonParseError parseError{};
        const auto document = QJsonDocument::fromJson(providerProfilesJson.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isArray()
            || document.array().size() > 16) {
            qCritical().noquote() << "Read provider profiles must be a JSON array of at most 16 entries.";
            return std::nullopt;
        }
        std::unordered_set<std::string> seenIds{"hypersnap-v2", "farcaster-snapchain"};
        for (const QJsonValue& entry : document.array()) {
            if (!entry.isObject()) {
                qCritical().noquote() << "Every read provider profile must be a JSON object.";
                return std::nullopt;
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
                return std::nullopt;
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
#if defined(Q_OS_WASM)
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
            return std::nullopt;
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
                return std::nullopt;
            }
            const QByteArray value = it.value().toString().toUtf8();
            if (value.isEmpty() || value.size() > 256
                || std::any_of(value.cbegin(), value.cend(), [](const char character) {
                    const auto ascii = static_cast<unsigned char>(character);
                    return ascii < 33U || ascii > 126U;
                })) {
                qCritical().noquote() << "Invalid indexed read credential format.";
                return std::nullopt;
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
    // HyperSnap defaults to Read only when deployment policy allows it.
    // Neither an explicit user choice nor the Write adapter is overridden.
    if (preferredReadProviderText.isEmpty() && enableHypersnapDefault) {
        preferredReadProviderText = QStringLiteral("hypersnap-v2");
        // Keep the settings UI in agreement with the actual applied runtime.
        // In a read-only settings backend, the runtime preference still works.
        settings.setValue(QStringLiteral("network/preferredReadProviderId"),
                          preferredReadProviderText);
        settings.sync();
    }
    if (!preferredReadProviderText.isEmpty() && !validProviderId(preferredReadProviderText)) {
        qCritical().noquote() << "Preferred read provider id is invalid.";
        return std::nullopt;
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
    std::vector<WriteProviderProfile> additionalWriteProviders{};
    if (!writeProfilesJson.isEmpty()) {
        QJsonParseError parseError{};
        const auto document = QJsonDocument::fromJson(writeProfilesJson.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isArray()
            || document.array().size() > 16) {
            qCritical().noquote()
                << "Write provider profiles must be a JSON array of at most 16 entries.";
            return std::nullopt;
        }
        std::unordered_set<std::string> ids{"primary-protocol-writer",
            "hypersnap-v2", "farcaster-snapchain"};
        for (const auto& read : additionalReadProviders) ids.insert(read.id);
        for (const QJsonValue& entry : document.array()) {
            if (!entry.isObject()) {
                qCritical().noquote() << "Write provider profile must be an object.";
                return std::nullopt;
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
                return std::nullopt;
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
        return std::nullopt;
    }
    const std::optional<std::string> preferredWriteProviderId =
        preferredWriteProviderText.isEmpty() ? std::nullopt
            : std::optional<std::string>{preferredWriteProviderText.toStdString()};


    return ProviderProfileConfiguration{
        .readProviders = std::move(additionalReadProviders),
        .writeProviders = std::move(additionalWriteProviders),
        .readProviderApiKeys = std::move(readProviderApiKeys),
        .preferredReadProviderId = preferredReadProviderId,
        .preferredWriteProviderId = preferredWriteProviderId,
        .allowCompatibleReadFallback = allowCompatibleReadFallback,
        .readProfilesJson = providerProfilesJson.toStdString(),
        .writeProfilesJson = writeProfilesJson.toStdString(),
        .preferredReadProviderText = preferredReadProviderText.toStdString(),
        .preferredWriteProviderText = preferredWriteProviderText.toStdString(),
    };
}

} // namespace genycaster::ui::bootstrap
