module;

#include <memory>
#include <optional>
#include <string>

export module farcaster.capability.auth;

import farcaster.domain.identity;
import farcaster.domain.session;
import farcaster.foundation.result;
import farcaster.foundation.time;

export namespace farcaster::capability::auth {

/** @brief App-owned relying-party values used to create a SIWF challenge. */
struct SignInRequest final {
    std::string domain{};
    std::string siweUri{};
};

/**
 * @brief Transient user-facing approval information for a compatibility sign-in.
 *
 * The opaque approval URI may contain relay-defined challenge material. It is
 * not a persisted session credential and must not be logged or exposed as
 * editable/readable presentation text.
 */
struct AuthenticationChallenge final {
    std::string approvalUrl{};
};

/** @brief Observable state of one compatibility authentication attempt. */
enum class PollState {
    Pending,
    Completed,
};

/** @brief One poll result after provider response validation. */
struct PollResult final {
    PollState state{PollState::Pending};
    std::optional<domain::session::ClientSession> session{};
};

/**
 * @brief Opaque in-progress compatibility authentication attempt.
 *
 * Concrete implementations retain relay/provider tokens inside this object.
 * Polling may block and must execute away from the Qt GUI thread.
 */
class IAuthenticationAttempt {
public:
    /** @brief Destroys the attempt and any provider-private transient state. */
    virtual ~IAuthenticationAttempt() = default;

    /** @brief Polls once for user approval and returns only normalized state. */
    [[nodiscard]] virtual foundation::result::Result<PollResult> poll() = 0;
};

/** @brief Result of starting an authentication attempt. */
struct StartedAuthentication final {
    AuthenticationChallenge challenge{};
    std::unique_ptr<IAuthenticationAttempt> attempt{};
};

/**
 * @brief Optional compatibility login boundary, independent from protocol signing.
 *
 * Implementations may use a managed relay, QR/deep-link approval or another
 * compatibility mechanism. They must verify the returned identity before
 * producing a ClientSession.
 */
class ICompatibilityAuthentication {
public:
    /** @brief Destroys the authentication adapter. */
    virtual ~ICompatibilityAuthentication() = default;

    /**
     * @brief Starts one login challenge.
     * @param request App-owned SIWF domain and URI.
     * @return User-facing challenge plus an opaque pollable attempt.
     */
    [[nodiscard]] virtual foundation::result::Result<StartedAuthentication> begin(
        const SignInRequest& request) = 0;

    /**
     * @brief Resolves the current authoritative custody address for an existing FID.
     *
     * This is used to upgrade legacy persisted sessions without forcing a new SIWF
     * ceremony. Failure must not invalidate the already verified read session; it only
     * prevents custody-authorized signer provisioning until verification succeeds.
     */
    [[nodiscard]] virtual foundation::result::Result<std::string> resolveCustodyAddress(
        domain::identity::Fid fid) = 0;
};

}
