/*!
 * @file        src/capabilities/signer_attestation.cppm
 * @brief       App-FID signer attestation contract.
 *
 * @details
 * Models verifiable SignedKeyRequest attribution and the attestation
 * boundary without making a UI label proof of authorship.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstdint>
#include <string>
#include <string_view>

export module farcaster.capability.signer_attestation;

import farcaster.foundation.result;
import farcaster.foundation.time;

export namespace farcaster::capability::signer_attestation {

/** @brief App-owned SignedKeyRequest proof used to attribute a gasless signer to requestFid. */
struct SignedKeyRequestAttestation final {
    /** @brief Raw 20-byte Ethereum custody address of the app/request FID. */
    std::string requestSignerAddress{};

    /** @brief Raw 65-byte EIP-712 SignedKeyRequest signature by that custody address. */
    std::string signature{};
};

/**
 * @brief First-party app-attestation port for gasless signer attribution.
 *
 * The implementation must never receive or store the user's custody private key.
 * It signs only SignedKeyRequest(requestFid, publicKey, deadline) using the app
 * FID's custody authority and returns public signature material.
 */
class ISignedKeyRequestAttestor {
public:
    virtual ~ISignedKeyRequestAttestor() = default;

    /** @brief Returns an app proof bound to the exact request FID/key/deadline. */
    [[nodiscard]] virtual foundation::result::Result<SignedKeyRequestAttestation> attest(
        std::uint64_t requestFid,
        std::string_view publicKeyHex,
        foundation::time::Timestamp deadline) = 0;
};

} // namespace farcaster::capability::signer_attestation
