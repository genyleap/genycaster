/*!
 * @file        src/providers/openproof_auth/auth.cppm
 * @brief       External identity provider adapter.
 *
 * @details
 * Composes verified identity-provider sessions into the client
 * authentication capability without taking custody of its credentials.
 *
 * @author      Kambiz Asadzadeh
 * @since       29 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include "genycaster_deployment.hpp"
#include <chrono>
#include <memory>
#include <string>

export module farcaster.provider.identity.openproof;

import farcaster.capability.auth;
import farcaster.foundation.result;
import farcaster.transport.http;

export namespace farcaster::provider::identity::openproof {

/** @brief OpenProof/Genyleap identity composition for the Genycaster native client. */
struct Config final {
    std::string issuer{genycaster::deployment::openProofIssuer};
    std::string clientId{"genycaster-native-v1"};
    std::string genyleapAccountUrl{std::string{genycaster::deployment::companyOrigin} + "/account"};
    std::string farcasterRelayBaseUrl{"https://relay.farcaster.xyz"};
    std::string farcasterRelayVersion{"v1"};
    std::chrono::milliseconds requestTimeout{10'000};
    std::chrono::minutes attemptLifetime{10};
    std::chrono::hours sessionLifetime{24};
};

/**
 * @brief Creates the OpenProof-backed authentication provider.
 *
 * The OpenProof route uses RFC 8628 device authorization and deliberately opens
 * the verification page through genyleap.com so an existing first-party browser
 * session is reused. The direct Farcaster route verifies the Farcaster proof
 * through OpenProof and therefore still produces a canonical OpenProof identity.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<capability::auth::IAuthenticationProvider>>
makeAuthentication(
    transport::http::IHttpTransport& transport,
    Config config = {});

}
