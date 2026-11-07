/*!
 * @file        ui/app/bootstrap/auth.cpp
 * @brief       Authentication endpoint and relying-party configuration.
 *
 * @details
 * Validates optional startup overrides without accessing the signer vault.
 * Fails closed for malformed client identity or signer attestation origins
 * and never infers signing authority from the selected Read/Write provider.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <QDebug>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

module genycaster.ui.bootstrap.auth;

import genycaster.ui.bootstrap;

namespace genycaster::ui::bootstrap {

std::optional<AuthenticationOptions> loadAuthenticationOptions(
    const QStringList& arguments,
    const std::uint64_t defaultAppFid,
    const QString& defaultSiwfDomain,
    const QString& defaultSiwfUri,
    const bool openProofAvailable,
    const bool compatibilityAuthAvailable)
{
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
            return std::nullopt;
        }
        appFid = static_cast<std::uint64_t>(parsed);
    }
    if (!appFid && defaultAppFid != 0U) {
        appFid = defaultAppFid;
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
    if (!signerAttestationUrlText.isEmpty()
        && !isValidHttpServiceUrl(signerAttestationUrlText)) {
        qCritical().noquote()
            << "Signer attestation URL must be a credential-free HTTP(S) service.";
        return std::nullopt;
    }
    const std::optional<std::string> signerAttestationUrl = signerAttestationUrlText.isEmpty()
        ? std::nullopt : std::optional<std::string>{signerAttestationUrlText.toStdString()};

    QString walletLaunchTemplate = optionValue(
        arguments, QStringLiteral("--wallet-launch-url-template="));
    if (walletLaunchTemplate.isEmpty()) {
        walletLaunchTemplate =
            qEnvironmentVariable("GENYCASTER_WALLET_LAUNCH_URL_TEMPLATE").trimmed();
    }

    const QString compiledSiwfDomain = normalizeConfigurationToken(
        defaultSiwfDomain);
    const QString compiledSiwfUri = normalizeConfigurationToken(
        defaultSiwfUri);

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
    const bool authenticationAvailable = openProofAvailable
        || (compatibilityAuthAvailable && !siwfDomain.empty() && !siwfUri.empty());


    return AuthenticationOptions{
        .appFid = appFid,
        .signerAttestationUrl = signerAttestationUrl,
        .walletLaunchTemplate = walletLaunchTemplate.toStdString(),
        .siwfDomain = siwfDomain,
        .siwfUri = siwfUri,
        .authRelayUrl = authRelayUrl,
        .optimismRpcUrl = optimismRpcUrl,
        .authenticationAvailable = authenticationAvailable,
    };
}

} // namespace genycaster::ui::bootstrap
