/*!
 * @file        src/capabilities/auth.cppm
 * @brief       Identity authentication capability contracts.
 *
 * @details
 * Defines sign-in challenges, verified account grants and routing between
 * supported authentication methods.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

export module farcaster.capability.auth;

import farcaster.domain.identity;
import farcaster.domain.session;
import farcaster.foundation.result;
import farcaster.foundation.time;

export namespace farcaster::capability::auth {

/**
 * @brief Product-selected identity entry point.
 *
 * The Genyleap route uses an OpenProof account. The Farcaster route uses a
 * separate native SIWF relay and verifies the wallet signature and on-chain
 * FID ownership without requiring an OpenProof session.
 */
enum class SignInRoute : std::uint8_t {
    OpenProof,
    Farcaster,
};

/** @brief App-owned values used to start one authentication route. */
struct SignInRequest final {
    SignInRoute route{SignInRoute::Farcaster};
    std::string domain{};
    std::string siweUri{};
};

/**
 * @brief Transient user-facing approval information for an authentication flow.
 *
 * The opaque approval URI may contain short-lived provider challenge material.
 * It is not a persisted session credential and must not be logged or exposed as
 * editable/readable presentation text.
 */
struct AuthenticationChallenge final {
    std::string approvalUrl{};
};

/** @brief Observable state of one authentication attempt. */
enum class PollState : std::uint8_t {
    Pending,
    FarcasterLinkRequired,
    Completed,
};

/** Short-lived, FID-verified account capability for native composition only.
 * Never persist, log, or expose this value to the QML presentation layer.
 * A restored device-local session is not an authorization grant. */
struct VerifiedAccountGrant final {
    std::string bearer{};
    std::string subject{};
    std::uint64_t fid{};
    int expiresSeconds{};
};

/** @brief One poll result after provider response validation. */
struct PollResult final {
    PollState state{PollState::Pending};
    std::optional<domain::session::ClientSession> session{};
    std::optional<std::string> approvalUrl{};
    std::optional<VerifiedAccountGrant> accountGrant{};
};

/**
 * @brief Opaque in-progress authentication attempt.
 *
 * Concrete implementations retain provider tokens inside this object. Polling
 * may block and must execute away from the Qt GUI thread.
 */
class IAuthenticationAttempt {
public:
    virtual ~IAuthenticationAttempt() = default;

    /** @brief Polls once for user action and returns only normalized state. */
    [[nodiscard]] virtual foundation::result::Result<PollResult> poll() = 0;
};

/** @brief Result of starting an authentication attempt. */
struct StartedAuthentication final {
    AuthenticationChallenge challenge{};
    std::unique_ptr<IAuthenticationAttempt> attempt{};
};

/**
 * @brief Provider-neutral client authentication boundary.
 *
 * A completed result must contain a verified Farcaster-linked ClientSession.
 * Genyleap OAuth and direct SIWF use distinct providers, each requiring
 * authoritative proof before persisting a local session.
 */
class IAuthenticationProvider {
public:
    virtual ~IAuthenticationProvider() = default;

    /** @brief Starts one product-selected authentication route. */
    [[nodiscard]] virtual foundation::result::Result<StartedAuthentication> begin(
        const SignInRequest& request) = 0;
};

/**
 * @brief Farcaster identity metadata resolver used after authentication.
 *
 * Authentication and protocol custody metadata are deliberately independent:
 * OpenProof owns the account identity while signer provisioning still needs the
 * canonical Farcaster custody address.
 */
class IFarcasterIdentityResolver {
public:
    virtual ~IFarcasterIdentityResolver() = default;

    [[nodiscard]] virtual foundation::result::Result<std::string> resolveCustodyAddress(
        domain::identity::Fid fid) = 0;
};

/**
 * @brief Legacy Farcaster compatibility adapter.
 *
 * Kept as a separate optional boundary for self-hosted/compatibility builds and
 * custody-address resolution. Shipping Genycaster composes OpenProof as the
 * primary IAuthenticationProvider.
 */
class ICompatibilityAuthentication
    : public IAuthenticationProvider,
      public IFarcasterIdentityResolver {
public:
    ~ICompatibilityAuthentication() override = default;
};

}
