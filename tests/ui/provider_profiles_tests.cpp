/*!
 * @file        tests/ui/provider_profiles_tests.cpp
 * @brief       Provider profile bootstrap policy and security regressions.
 *
 * @details
 * Exercises Read/Write selection, identifier uniqueness, endpoint validation
 * and ephemeral credential routing through the production C++ module.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <array>

import genycaster.ui.bootstrap.profiles;

namespace {

class ScopedEnvironment final {
public:
    struct Entry {
        const char* name{};
        QByteArray previous{};
        bool existed{false};
    };

    std::array<Entry, 8> entries{{
        {"GENYCASTER_READ_PROVIDER_PROFILES_JSON"},
        {"GENYCASTER_READ_PROVIDER_API_KEYS_JSON"},
        {"GENYCASTER_PREFERRED_READ_PROVIDER_ID"},
        {"GENYCASTER_WRITE_PROVIDER_PROFILES_JSON"},
        {"GENYCASTER_PREFERRED_WRITE_PROVIDER_ID"},
        {"GENYCASTER_ALLOW_COMPATIBLE_READ_FALLBACK"},
        {"GENYCASTER_NETWORK_PROXY"},
        {"GENYCASTER_FARCASTER_READ_API_KEY"},
    }};

    ScopedEnvironment()
    {
        for (auto& entry : entries) {
            entry.existed = qEnvironmentVariableIsSet(entry.name);
            entry.previous = qgetenv(entry.name);
            qunsetenv(entry.name);
        }
    }

    ~ScopedEnvironment()
    {
        for (const auto& entry : entries) {
            if (entry.existed) qputenv(entry.name, entry.previous);
            else qunsetenv(entry.name);
        }
    }
};

bool check(bool condition, const char* message)
{
    if (!condition) qCritical().noquote() << "Provider profile test failed:" << message;
    return condition;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application{argc, argv};
    ScopedEnvironment environment{};
    QTemporaryDir dataDirectory{};
    if (!dataDirectory.isValid()) return 2;
    QSettings settings{dataDirectory.filePath(QStringLiteral("preferences.ini")),
                       QSettings::IniFormat};

    using genycaster::ui::bootstrap::loadProviderProfiles;
    bool passed = true;

    {
        const auto config = loadProviderProfiles(settings, {}, true);
        passed &= check(config.has_value(), "ordinary profile configuration");
        if (!config) return 1;
        passed &= check(config->preferredReadProviderId == "hypersnap-v2",
                        "HyperSnap remains the default Read adapter");
        passed &= check(!config->preferredWriteProviderId,
                        "default Read preference must not force a Write adapter");
        passed &= check(!config->allowCompatibleReadFallback,
                        "cross-provider Read fallback must require consent");
        passed &= check(config->readProviderApiKeys.empty(),
                        "Read credentials must be opt-in");
    }

    settings.remove(QStringLiteral("network/preferredReadProviderId"));
    {
        const auto config = loadProviderProfiles(settings, {}, false);
        passed &= check(config && !config->preferredReadProviderId,
                        "strict self-hosted deployment must not force HyperSnap");
    }
    {
        const auto config = loadProviderProfiles(
            settings, {QStringLiteral("--preferred-read-provider-id=farcaster-snapchain")}, true);
        passed &= check(config && config->preferredReadProviderId == "farcaster-snapchain",
                        "explicit Read selection overrides HyperSnap default");
    }

    const QString readJson = QStringLiteral(R"([
      {"id":"read-private","contract":"hypersnap-v2",
       "endpoint":"https://node.example","label":"Own node","selfHosted":true}
    ])");
    const QString writeJson = QStringLiteral(R"([
      {"id":"write-private","contract":"snapchain-v1",
       "endpoint":"https://validator.example",
       "readEndpoint":"https://node.example","selfHosted":true}
    ])");
    settings.setValue(QStringLiteral("network/readProviderProfilesJson"), readJson);
    settings.setValue(QStringLiteral("network/writeProviderProfilesJson"), writeJson);
    {
        const auto config = loadProviderProfiles(
            settings,
            {QStringLiteral("--preferred-read-provider-id=read-private"),
             QStringLiteral("--preferred-write-provider-id=write-private"),
             QStringLiteral("--allow-compatible-read-fallback")},
            true);
        passed &= check(config && config->readProviders.size() == 1
                        && config->writeProviders.size() == 1,
                        "both independent adapter profiles parsed");
        if (config) {
            passed &= check(config->readProviders[0].selfHosted
                            && config->readProviders[0].adapterContract == "hypersnap-v2",
                            "Read contract and hosting class preserved");
            passed &= check(config->writeProviders[0].readEndpoint == "https://node.example",
                            "separate Write read endpoint preserved");
            passed &= check(config->preferredReadProviderId == "read-private"
                            && config->preferredWriteProviderId == "write-private",
                            "Read and Write selections stay independent");
            passed &= check(config->allowCompatibleReadFallback,
                            "explicit Read fallback consent honored");
            passed &= check(config->readProfilesJson == readJson.toStdString()
                            && config->writeProfilesJson == writeJson.toStdString(),
                            "profile editor receives its original non-secret JSON");
        }
    }

    qputenv("GENYCASTER_READ_PROVIDER_API_KEYS_JSON",
            R"({"read-private":"temporary-read-token"})");
    {
        const auto config = loadProviderProfiles(settings, {}, false);
        passed &= check(config && config->readProviderApiKeys.at("read-private")
                                 == "temporary-read-token",
                        "ephemeral HTTPS credential bound to named instance");
        passed &= check(!settings.contains(QStringLiteral("network/readProviderApiKeysJson")),
                        "secret never written into settings");
        passed &= check(!readJson.contains(QStringLiteral("temporary-read-token")),
                        "secret absent from persistent profile JSON");
    }
    settings.setValue(QStringLiteral("network/readProviderProfilesJson"),
                      readJson.replace(QStringLiteral("https://node.example"),
                                       QStringLiteral("http://node.example")));
    {
        const auto config = loadProviderProfiles(settings, {}, false);
        passed &= check(!config, "credential cannot bind to HTTP-only provider");
    }
    qunsetenv("GENYCASTER_READ_PROVIDER_API_KEYS_JSON");
    settings.setValue(QStringLiteral("network/readProviderProfilesJson"), readJson);
    settings.setValue(QStringLiteral("network/writeProviderProfilesJson"),
                      QStringLiteral(R"([
                        {"id":"read-private","contract":"snapchain-v1",
                         "endpoint":"https://writer.example","selfHosted":true}
                      ])"));
    {
        const auto config = loadProviderProfiles(settings, {}, false);
        passed &= check(!config, "Read and Write IDs must not collide");
    }
    settings.setValue(QStringLiteral("network/writeProviderProfilesJson"), writeJson);
    settings.setValue(QStringLiteral("network/readProviderProfilesJson"),
                      QStringLiteral(R"([
                        {"id":"read-private","contract":"hypersnap-v2",
                         "endpoint":"https://user:password@node.example","selfHosted":true}
                      ])"));
    {
        const auto config = loadProviderProfiles(settings, {}, false);
        passed &= check(!config, "credential-bearing URL forbidden in public profile");
    }

    return passed ? 0 : 1;
}
