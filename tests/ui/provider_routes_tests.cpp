/*!
 * @file        tests/ui/provider_routes_tests.cpp
 * @brief       Independent Read and Write protocol route regression tests.
 *
 * @details
 * Checks HyperSnap default routing, explicit Snapchain and custom adapters,
 * fail-closed incompatible contracts, and separate authorization pairs.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QByteArray>

#include <array>
#include <optional>
#include <string>

import genycaster.ui.bootstrap.profiles;
import genycaster.ui.bootstrap.routes;

namespace {
class EnvironmentScope final {
public:
    struct Variable {
        const char* name{};
        QByteArray oldValue{};
        bool wasSet{};
    };
    std::array<Variable, 8> variables{{
        {"GENYCASTER_HYPERSNAP_NODE_CANDIDATES"},
        {"GENYCASTER_SNAPCHAIN_BASE_URL"},
        {"GENYCASTER_SNAPCHAIN_WRITE_BASE_URL"},
        {"GENYCASTER_MEDIA_GATEWAY_BASE_URL"},
        {"GENYCASTER_SNAPCHAIN_AUTH_HEADER"},
        {"GENYCASTER_SNAPCHAIN_AUTH_VALUE"},
        {"GENYCASTER_SNAPCHAIN_WRITE_AUTH_HEADER"},
        {"GENYCASTER_SNAPCHAIN_WRITE_AUTH_VALUE"},
    }};
    EnvironmentScope()
    {
        for (auto& variable : variables) {
            variable.wasSet = qEnvironmentVariableIsSet(variable.name);
            variable.oldValue = qgetenv(variable.name);
            qunsetenv(variable.name);
        }
    }
    ~EnvironmentScope()
    {
        for (const auto& variable : variables) {
            if (variable.wasSet) qputenv(variable.name, variable.oldValue);
            else qunsetenv(variable.name);
        }
    }
};
bool check(bool result, const char* label)
{
    if (!result) qCritical().noquote() << "Provider routing regression:" << label;
    return result;
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    EnvironmentScope isolation;
    QTemporaryDir directory{};
    if (!directory.isValid()) return 2;
    QSettings settings(directory.filePath(QStringLiteral("routes.ini")),
                       QSettings::IniFormat);
    using genycaster::ui::bootstrap::ProviderProfileConfiguration;
    using genycaster::ui::bootstrap::ReadProviderProfile;
    using genycaster::ui::bootstrap::loadProviderRoutes;

    const QString hypersnapDefault = QStringLiteral("https://hypersnap.default.example");
    const QString snapchainDefault = QStringLiteral("https://snapchain.default.example");
    const QString mediaDefault = QStringLiteral("https://media.default.example");
    const QString blank{};

    bool passed = true;
    ProviderProfileConfiguration profiles{};
    profiles.preferredReadProviderId = "hypersnap-v2";
    {
        const auto routes = loadProviderRoutes(
            settings, {}, blank, hypersnapDefault, snapchainDefault,
            mediaDefault, profiles);
        passed &= check(routes.has_value(), "default configuration valid");
        if (!routes) return 1;
        passed &= check(routes->appliedNodeEndpoint ==
                            "https://hypersnap.default.example",
                        "default node from compiled HyperSnap adapter");
        passed &= check(routes->snapchainReadEndpoint ==
                            "https://hypersnap.default.example"
                        && routes->snapchainWriteEndpoint ==
                            "https://hypersnap.default.example",
                        "canonical signed-message route stays with selected HyperSnap");
        passed &= check(routes->nodeCandidateEndpoints.size() == 1
                            && routes->nodeCandidateEndpoints.front() ==
                                "https://hypersnap.default.example",
                        "compiled HyperSnap included in node candidates");
        passed &= check(routes->mediaGatewayEndpoint == "https://media.default.example",
                        "media endpoint independently selected");
    }

    profiles.preferredReadProviderId = "farcaster-snapchain";
    settings.setValue(QStringLiteral("network/snapchainWriteBaseUrl"),
                      QStringLiteral("https://validator.separate.example"));
    {
        const auto routes = loadProviderRoutes(
            settings, {}, blank, hypersnapDefault, snapchainDefault,
            mediaDefault, profiles);
        passed &= check(routes && routes->snapchainReadEndpoint ==
                            "https://snapchain.default.example"
                        && routes->snapchainWriteEndpoint ==
                            "https://validator.separate.example",
                        "Snapchain read-only and validator-write origins remain distinct");
    }

    profiles.preferredReadProviderId = "custom-read";
    profiles.readProviders.push_back(ReadProviderProfile{
        .id = "custom-read",
        .displayName = "Private HyperSnap",
        .endpoint = "https://my-node.example",
        .adapterContract = "hypersnap-v2",
        .selfHosted = true,
    });
    {
        const auto routes = loadProviderRoutes(
            settings, {}, blank, hypersnapDefault, snapchainDefault,
            mediaDefault, profiles);
        passed &= check(routes && routes->snapchainReadEndpoint ==
                            "https://my-node.example"
                        && routes->snapchainWriteEndpoint ==
                            "https://my-node.example",
                        "primary writer follows selected compatible custom node");
    }

    profiles.preferredWriteProviderId = "separate-writer";
    {
        const auto routes = loadProviderRoutes(
            settings, {}, blank, hypersnapDefault, snapchainDefault,
            mediaDefault, profiles);
        passed &= check(routes && routes->snapchainReadEndpoint ==
                            "https://snapchain.default.example"
                        && routes->snapchainWriteEndpoint ==
                            "https://validator.separate.example",
                        "explicit Write adapter choice is not overridden by Read node");
    }
    profiles.preferredWriteProviderId.reset();

    profiles.readProviders[0].adapterContract = "unrelated-v1";
    {
        const auto routes = loadProviderRoutes(
            settings, {}, blank, hypersnapDefault, snapchainDefault,
            mediaDefault, profiles);
        passed &= check(!routes, "unknown Read contract must never become a writer");
    }
    profiles.readProviders[0].adapterContract = "hypersnap-v2";

    // Defense in depth: direct routing must still reject an unresolved ID,
    // even though the bootstrap preferences layer repairs stale local
    // selections before this point. Never cross-post to the Snapchain default.
    profiles.preferredReadProviderId = "missing-read-instance";
    {
        const auto routes = loadProviderRoutes(
            settings, {}, blank, hypersnapDefault, snapchainDefault,
            mediaDefault, profiles);
        passed &= check(!routes, "unregistered selected provider cannot choose a writer");
    }
    profiles.preferredReadProviderId = "custom-read";

    {
        const auto routes = loadProviderRoutes(
            settings, {QStringLiteral("--snapchain-base-url=file:///etc/passwd")},
            blank, hypersnapDefault, snapchainDefault, mediaDefault, profiles);
        passed &= check(!routes, "invalid Snapchain URL fails closed");
    }
    {
        const auto routes = loadProviderRoutes(
            settings, {QStringLiteral("--snapchain-write-auth-header=Authorization")},
            blank, hypersnapDefault, snapchainDefault, mediaDefault, profiles);
        passed &= check(!routes, "partial write credential pair fails closed");
    }
    {
        const auto routes = loadProviderRoutes(
            settings, {QStringLiteral("--snapchain-auth-header=X-Api-Key"),
                       QStringLiteral("--snapchain-auth-value=secret-one"),
                       QStringLiteral("--snapchain-write-auth-header=X-Validator-Key"),
                       QStringLiteral("--snapchain-write-auth-value=secret-two")},
            blank, hypersnapDefault, snapchainDefault, mediaDefault, profiles);
        passed &= check(routes && routes->readAuthorizationValue == "secret-one"
                        && routes->writeAuthorizationValue == "secret-two",
                        "independent ephemeral Read/Write authorization is preserved");
    }
    {
        const auto routes = loadProviderRoutes(
            settings, {QStringLiteral("--media-gateway-base-url=ftp://example.com")},
            blank, hypersnapDefault, snapchainDefault, mediaDefault, profiles);
        passed &= check(!routes, "unsafe media upload gateway is rejected");
    }
    return passed ? 0 : 1;
}
