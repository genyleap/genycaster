/*!
 * @file        src/domain/session.cppm
 * @brief       Verified Farcaster client session identity.
 *
 * @details
 * Models authenticated identity and proof method independently from
 * signer registration and wallet account access.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstdint>
#include <optional>
#include <string>

export module farcaster.domain.session;

import farcaster.domain.identity;
import farcaster.foundation.time;

export namespace farcaster::domain::session {

/** @brief Method that proved control of a Farcaster identity for client sign-in. */
enum class AuthenticationMethod : std::uint8_t {
    Custody,
    AuthAddress,
};

/**
 * @brief Verified local client-session identity.
 *
 * This snapshot proves application sign-in only. It is not a protocol signer
 * and therefore does not by itself authorize Cast, Reaction or Link messages.
 */
struct ClientSession final {
    /**
     * @brief Canonical account issuer when the session is OpenProof-backed.
     *
     * The pair (identityIssuer, identitySubject) identifies Genyleap/OpenProof
     * sessions. Native Farcaster SIWF sessions leave these fields empty and are
     * keyed by the verified protocol FID below.
     */
    std::optional<std::string> identityIssuer{};
    std::optional<std::string> identitySubject{};
    domain::identity::Fid fid{};
    std::optional<std::string> username{};
    std::optional<std::string> displayName{};
    std::optional<std::string> avatarUrl{};
    /** @brief Current authoritative Farcaster custody address as normalized 0x-prefixed hex. */
    std::optional<std::string> custodyAddress{};
    AuthenticationMethod method{AuthenticationMethod::Custody};
    foundation::time::Timestamp authenticatedAt{};
    foundation::time::Timestamp expiresAt{};
};

/** @brief Returns true when the session may still represent an active local login. */
[[nodiscard]] bool isExpired(
    const ClientSession& session,
    foundation::time::Timestamp now) noexcept;

}
