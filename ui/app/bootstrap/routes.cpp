/*!
 * @file        ui/app/bootstrap/routes.cpp
 * @brief       Protocol-aware endpoint validation and routing implementation.
 *
 * @details
 * Resolves runtime overrides against saved preferences and deployment-owned
 * defaults. Refuses invalid adapter contracts and origins instead of silently
 * rewriting canonical signed messages to an unrelated protocol.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <QDebug>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

module genycaster.ui.bootstrap.routes;

import genycaster.ui.bootstrap;

namespace genycaster::ui::bootstrap {

std::optional<ProviderRoutes> loadProviderRoutes(
    QSettings& settings,
    const QStringList& arguments,
    const QString& providerBaseUrlText,
    const QString& hypersnapDefault,
    const QString& snapchainDefault,
    const QString& mediaGatewayDefault,
    const ProviderProfileConfiguration& profiles)
{
    const auto& preferredWriteProviderId = profiles.preferredWriteProviderId;
    const auto& preferredReadProviderId = profiles.preferredReadProviderId;
    const auto& additionalReadProviders = profiles.readProviders;

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
    if (!hypersnapDefault.isEmpty()) {
        nodeCandidateEndpoints.append(hypersnapDefault);
        if (appliedNodeEndpoint.isEmpty())
            appliedNodeEndpoint = hypersnapDefault;
    }

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
            snapchainDefault.toStdString());
    }
    snapchainBaseUrlText = normalizeConfigurationToken(std::move(snapchainBaseUrlText));
    if (!isValidHttpServiceUrl(snapchainBaseUrlText)) {
        qCritical().noquote() << "Snapchain read URL must be a credential-free HTTP(S) origin.";
        return std::nullopt;
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
                [&](const ReadProviderProfile& profile) {
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
                    << "Selected protocol has no compatible canonical message writer."
                    << "Read provider ID:" << QString::fromStdString(*preferredReadProviderId)
                    << (match == additionalReadProviders.cend()
                        ? "(not registered)" : "(unsupported Read contract)");
                return std::nullopt;
            }
        }
        if (!selectedProtocolOrigin.isEmpty()) {
            if (!isValidHttpServiceUrl(selectedProtocolOrigin)) {
                qCritical().noquote() << "Selected protocol writer URL is invalid.";
                return std::nullopt;
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
        return std::nullopt;
    }
    const QString effectiveWriteBaseUrl = snapchainWriteBaseUrlText.isEmpty()
        ? snapchainBaseUrlText : snapchainWriteBaseUrlText;

    QString mediaGatewayBaseUrlText = optionValue(
        arguments, QStringLiteral("--media-gateway-base-url="));
    if (mediaGatewayBaseUrlText.isEmpty()) {
        mediaGatewayBaseUrlText =
            qEnvironmentVariable("GENYCASTER_MEDIA_GATEWAY_BASE_URL").trimmed();
    }
    if (mediaGatewayBaseUrlText.isEmpty()) {
        mediaGatewayBaseUrlText = QString::fromStdString(
            mediaGatewayDefault.toStdString());
    }
    mediaGatewayBaseUrlText = normalizeConfigurationToken(std::move(mediaGatewayBaseUrlText));
    if (!isValidHttpServiceUrl(mediaGatewayBaseUrlText)) {
        qCritical().noquote()
            << "GENYCASTER_MEDIA_GATEWAY_BASE_URL/--media-gateway-base-url must be a credential-free HTTP(S) origin.";
        return std::nullopt;
    }


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
        return std::nullopt;
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
        return std::nullopt;
    }


    ProviderRoutes routes{};
    routes.nodeCandidateEndpoints.reserve(static_cast<std::size_t>(nodeCandidateEndpoints.size()));
    for (const QString& candidate : nodeCandidateEndpoints)
        routes.nodeCandidateEndpoints.push_back(candidate.toStdString());
    routes.appliedNodeEndpoint = appliedNodeEndpoint.toStdString();
    routes.snapchainReadEndpoint = snapchainBaseUrlText.toStdString();
    routes.snapchainWriteEndpoint = effectiveWriteBaseUrl.toStdString();
    routes.mediaGatewayEndpoint = mediaGatewayBaseUrlText.toStdString();
    routes.readAuthorizationHeader = std::move(snapchainAuthorizationHeader);
    routes.readAuthorizationValue = std::move(snapchainAuthorizationValue);
    routes.writeAuthorizationHeader = std::move(snapchainWriteAuthorizationHeader);
    routes.writeAuthorizationValue = std::move(snapchainWriteAuthorizationValue);
    return routes;
}

} // namespace genycaster::ui::bootstrap
