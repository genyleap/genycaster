/*!
 * @file        src/application/session.cppm
 * @brief       Client authentication session lifecycle.
 *
 * @details
 * Exposes verified session phases and account state separately from
 * custody approval and protocol signing authority.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <functional>
#include <memory>
#include <optional>
#include <string>

export module farcaster.application.session;

import farcaster.application.executor;
import farcaster.capability.auth;
import farcaster.domain.session;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.local.session;

export namespace farcaster::application::session {

/** @brief Product-visible phase of the client authentication lifecycle. */
enum class SessionPhase {
    Guest,
    Starting,
    AwaitingApproval,
    LinkingFarcaster,
    Authenticated,
    Failed,
};

/** @brief Normalized application session state safe for presentation adapters. */
struct SessionState final {
    SessionPhase phase{SessionPhase::Guest};
    std::optional<domain::session::ClientSession> session{};
    std::string approvalUrl{};
    std::optional<foundation::error::Error> error{};
};

/** @brief Reason a protocol-changing interaction cannot run yet. */
enum class WriteAccess {
    SignInRequired,
    SignerRequired,
    Ready,
};

/**
 * @brief Evaluates the two independent gates required before protocol writes.
 * @param state Current application login state.
 * @param signerReady True only when a valid protocol signer is available.
 */
[[nodiscard]] WriteAccess evaluateWriteAccess(
    const SessionState& state,
    bool signerReady) noexcept;

/** @brief Observer called from the configured application executor. */
using Observer = std::function<void(SessionState)>;

/** Native-only, transient account capability delivered to trusted composition.
 * The UI layer and persisted client session must never receive its bearer. */
using AccountGrantObserver =
    std::function<void(capability::auth::VerifiedAccountGrant)>;

/**
 * @brief Orchestrates guest/authenticated client-session transitions.
 *
 * All persistence and compatibility-auth calls are scheduled through the
 * supplied executor. QML and presentation adapters never call provider or
 * storage APIs directly.
 */
class SessionService final {
public:
    /** @brief Creates a session service over application-owned ports. */
    SessionService(
        executor::IExecutor& executor,
        local::session::IClientSessionStore& store,
        capability::auth::IAuthenticationProvider* authentication,
        capability::auth::IFarcasterIdentityResolver* farcasterIdentityResolver,
        capability::auth::IAuthenticationProvider* directFarcasterAuthentication = nullptr);

    /** @brief Destroys the service after its executor has drained owned work. */
    ~SessionService();

    SessionService(const SessionService&) = delete;
    auto operator=(const SessionService&) -> SessionService& = delete;
    SessionService(SessionService&&) = delete;
    auto operator=(SessionService&&) -> SessionService& = delete;

    /** @brief Restores a non-expired persisted session or reports Guest. */
    [[nodiscard]] foundation::result::Status restore(Observer observer);

    /** @brief Starts the requested canonical identity sign-in flow. */
    [[nodiscard]] foundation::result::Status beginSignIn(
        capability::auth::SignInRequest request,
        Observer observer);

    /** @brief Polls the active authentication/linking attempt once. */
    [[nodiscard]] foundation::result::Status pollSignIn(
        Observer observer, AccountGrantObserver grantObserver = {});

    /**
     * @brief Commit an independently verified, short-lived custody session.
     *
     * The trusted native composition boundary calls this ONLY after Optimism
     * IdRegistry verification and completion of a one-time OpenProof challenge.
     * No recovery words, custody keys, signatures or account bearer enter
     * this service or the persistent session store.
     */
    [[nodiscard]] foundation::result::Status acceptVerifiedRecovery(
        domain::session::ClientSession session,
        Observer observer);

    /**
     * Commit a short-lived HyperSnap-native FID after a trusted C++ caller
     * has checked shard-0 registration for the same custody wallet and
     * derived a one-use custody signing permit. This is NOT OpenProof or
     * Optimism IdRegistry authentication and is not callable from QML.
     */
    [[nodiscard]] foundation::result::Status acceptVerifiedNativeCustody(
        domain::session::ClientSession session, Observer observer);

    /** Farcaster-specific commit after exact Optimism IdRegistry ownership. */
    [[nodiscard]] foundation::result::Status acceptVerifiedFarcasterCustody(
        domain::session::ClientSession session, Observer observer);

    /** @brief Cancels transient approval state while retaining any prior session. */
    [[nodiscard]] foundation::result::Status cancelSignIn(Observer observer);

    /** @brief Clears the client session without deleting cached public content. */
    [[nodiscard]] foundation::result::Status signOut(Observer observer);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}
