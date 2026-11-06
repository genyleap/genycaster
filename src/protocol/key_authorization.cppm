/*!
 * @file        src/protocol/key_authorization.cppm
 * @brief       Custody approval for signer key registration.
 *
 * @details
 * Constructs canonical EIP-712 KEY_ADD authorization inputs and documents
 * for a custody wallet.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstdint>
#include <string>
#include <vector>

export module farcaster.protocol.key_authorization;

import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.result;

export namespace farcaster::protocol::key_authorization {

/** @brief Exact Snapchain gasless KEY_ADD fields covered by custody EIP-712 authorization. */
struct KeyAddInput final {
    domain::identity::Fid fid{};
    domain::signer::PublicKey publicKey{};
    std::vector<domain::signer::Scope> scopes{};
    std::uint32_t ttlSeconds{};
    std::uint32_t nonce{};
    std::uint32_t deadline{};
    domain::identity::Fid requestFid{};
};

/** @brief Canonical EIP-712 JSON documents to be signed by an Ethereum custody wallet. */
struct TypedDataDocuments final {
    std::string signedKeyRequestJson{};
    std::string keyAddJson{};
};

/**
 * @brief Builds deterministic EIP-712 documents for SignedKeyRequest and KeyAdd.
 *
 * Both documents use the current Snapchain domain `Farcaster KeyAdd`, version
 * `1`, chainId `1`, and no verifying contract. The function validates key,
 * scope, TTL, nonce, deadline and FID invariants before producing wallet input.
 */
[[nodiscard]] foundation::result::Result<TypedDataDocuments> buildTypedData(
    const KeyAddInput& input);

/**
 * @brief ABI-encodes SignedKeyRequestMetadata for KeyAddBody.metadata.
 * @param requestFid App/request FID bound to the signer request.
 * @param requestSignerAddress Raw 20-byte Ethereum custody address.
 * @param signedKeyRequestSignature Raw 65-byte ECDSA signature returned by the wallet.
 * @param deadline Farcaster-epoch deadline covered by SignedKeyRequest.
 * @return Raw ABI tuple bytes `(uint256,address,bytes,uint256)`.
 */
[[nodiscard]] foundation::result::Result<std::string> encodeSignedKeyRequestMetadata(
    domain::identity::Fid requestFid,
    const std::string& requestSignerAddress,
    const std::string& signedKeyRequestSignature,
    std::uint32_t deadline);

}
