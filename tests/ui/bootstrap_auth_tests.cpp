/*!
 * @file        tests/ui/bootstrap_auth_tests.cpp
 * @brief       Startup identity, relying-party and attestor policy tests.
 *
 * @details
 * Verifies signed-login origin matching, explicit authentication fallbacks,
 * app FID validation, and the absence of inferred attestation authority.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#include <QCoreApplication>
#include <QDebug>
#include <QString>
#include <QStringList>
#include <QByteArray>

#include <array>
#include <cstdint>
#include <optional>
#include <string>

import genycaster.ui.bootstrap.auth;

namespace {
struct EnvironmentScope final {
    struct Variable { const char* key; QByteArray previous; bool existed; };
    std::array<Variable, 8> vars{{
        {"GENYCASTER_APP_FID", {}, false},
        {"GENYCASTER_SIGNER_ATTESTATION_URL", {}, false},
        {"GENYCASTER_WALLET_LAUNCH_URL_TEMPLATE", {}, false},
        {"GENYCASTER_SIWF_DOMAIN", {}, false},
        {"GENYCASTER_SIWF_URI", {}, false},
        {"GENYCASTER_FARCASTER_AUTH_RELAY", {}, false},
        {"GENYCASTER_OPTIMISM_RPC_URL", {}, false},
        {"GENYCASTER_HYPERSNAP_BASE_URL", {}, false}
    }};
    EnvironmentScope()
    {
        for (auto& item : vars) {
            item.existed = qEnvironmentVariableIsSet(item.key);
            item.previous = qgetenv(item.key);
            qunsetenv(item.key);
        }
    }
    ~EnvironmentScope()
    {
        for (const auto& item : vars) {
            if (item.existed) qputenv(item.key, item.previous);
            else qunsetenv(item.key);
        }
    }
};
bool require(bool condition, const char* caseName)
{
    if (!condition) qCritical().noquote() << "Auth bootstrap contract failed:" << caseName;
    return condition;
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    EnvironmentScope environment{};
    using genycaster::ui::bootstrap::loadAuthenticationOptions;
    constexpr std::uint64_t defaultFid = 866408U;
    const QString domain = QStringLiteral("genycaster.xyz");
    const QString uri = QStringLiteral("https://genycaster.xyz/login");
    bool passed = true;
    {
        const auto configuration = loadAuthenticationOptions(
            {}, defaultFid, domain, uri, false, true);
        passed &= require(configuration.has_value(), "valid default configuration");
        if (!configuration) return 1;
        passed &= require(configuration->appFid == defaultFid,
                          "compiled app FID is preserved");
        passed &= require(configuration->siwfDomain == "genycaster.xyz"
                              && configuration->siwfUri ==
                                  "https://genycaster.xyz/login",
                          "deployment SIWF identity preserved");
        passed &= require(configuration->authenticationAvailable,
                          "compatibility login uses valid SIWF identity");
        passed &= require(!configuration->signerAttestationUrl,
                          "Read/Write origins must not infer signer attestation");
    }
    {
        const auto configuration = loadAuthenticationOptions(
            {}, defaultFid, QStringLiteral("invalid domain"),
            QStringLiteral("file:///invalid"), false, true);
        passed &= require(configuration && !configuration->authenticationAvailable
                              && configuration->siwfDomain.empty()
                              && configuration->siwfUri.empty(),
                          "invalid SIWF disables compatibility login");
    }
    {
        const auto configuration = loadAuthenticationOptions(
            {}, defaultFid, QStringLiteral("invalid domain"),
            QStringLiteral("file:///invalid"), true, true);
        passed &= require(configuration && configuration->authenticationAvailable,
                          "OpenProof remains independent of incompatible SIWF");
    }
    {
        const auto configuration = loadAuthenticationOptions(
            {QStringLiteral("--app-fid=42")}, defaultFid, domain, uri, false, true);
        passed &= require(configuration && configuration->appFid == 42U,
                          "explicit valid app FID overrides compiled identity");
    }
    for (const QString& invalid : {QStringLiteral("0"), QStringLiteral("-3"),
                                  QStringLiteral("not-a-number")}) {
        const auto configuration = loadAuthenticationOptions(
            {QStringLiteral("--app-fid=") + invalid},
            defaultFid, domain, uri, false, true);
        passed &= require(!configuration, "invalid app FID rejected");
    }
    {
        const auto configuration = loadAuthenticationOptions(
            {QStringLiteral("--siwf-domain=example.com"),
             QStringLiteral("--siwf-uri=https://example.com/login")},
            defaultFid, domain, uri, false, true);
        passed &= require(configuration && configuration->siwfDomain == "example.com"
                              && configuration->siwfUri == "https://example.com/login",
                          "consistent explicit SIWF identity accepted");
    }
    {
        const auto configuration = loadAuthenticationOptions(
            {QStringLiteral("--siwf-domain=evil.example")},
            defaultFid, domain, uri, false, true);
        passed &= require(configuration && configuration->siwfDomain ==
                              "genycaster.xyz",
                          "partial SIWF identity override is not applied");
    }
    {
        const auto configuration = loadAuthenticationOptions(
            {QStringLiteral("--siwf-domain=example.com"),
             QStringLiteral("--siwf-uri=https://other.example/login")},
            defaultFid, domain, uri, false, true);
        passed &= require(configuration && configuration->siwfDomain ==
                              "genycaster.xyz",
                          "SIWF domain/URI mismatch cannot replace compiled identity");
    }
    {
        const auto configuration = loadAuthenticationOptions(
            {QStringLiteral("--signer-attestation-url=https://attestor.example")},
            defaultFid, domain, uri, false, true);
        passed &= require(configuration && configuration->signerAttestationUrl ==
                              "https://attestor.example",
                          "explicit attestor origin retained");
    }
    for (const QString& unsafe : {
             QStringLiteral("file:///etc/passwd"),
             QStringLiteral("https://user:password@attestor.example")}) {
        const auto configuration = loadAuthenticationOptions(
            {QStringLiteral("--signer-attestation-url=") + unsafe},
            defaultFid, domain, uri, false, true);
        passed &= require(!configuration, "unsafe attestation URL must fail closed");
    }
    {
        const auto configuration = loadAuthenticationOptions(
            {QStringLiteral("--optimism-rpc-url=https://rpc.example/?key=public"),
             QStringLiteral("--farcaster-auth-relay=https://relay.example")},
            defaultFid, domain, uri, false, true);
        passed &= require(configuration && configuration->optimismRpcUrl ==
                              "https://rpc.example/?key=public"
                              && configuration->authRelayUrl == "https://relay.example",
                          "independent allowed relay and RPC endpoints retained");
    }
    return passed ? 0 : 1;
}
