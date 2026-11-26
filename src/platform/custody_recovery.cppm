/*!
 * @file        src/platform/custody_recovery.cppm
 * @brief       Platform custody recovery and proof contract.
 *
 * @details
 * Defines authenticated recovery results and proof failures without
 * allowing seed material into application-facing storage.
 *
 * @author      Kambiz Asadzadeh
 * @since       30 Oct 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <cstddef>
#include <vector>
#include <string>
#include <string_view>

export module farcaster.platform.custody_recovery;

import farcaster.capability.signer;

export namespace farcaster::platform::custody_recovery {

/** A recovery phrase must be validated locally before any network request. */
enum class Error {
    InvalidPhrase,
    InvalidWordCount,
    UnknownWord,
    InvalidChecksum,
    KeyDerivationFailed,
    InvalidChallenge,
    SigningFailed,
};

/**
 * Fresh self-custody wallet generated exclusively in native C++.
 * Recovery words are stored in a move-only, zeroizing buffer until the user
 * confirms backup; they must never be stored in preferences or provider DTOs.
 */
struct GeneratedCustodyWallet final {
    std::string recoveryWords{};
    std::string address{};
    GeneratedCustodyWallet() = default;
    GeneratedCustodyWallet(GeneratedCustodyWallet&& other) noexcept;
    GeneratedCustodyWallet& operator=(GeneratedCustodyWallet&& other) noexcept;
    GeneratedCustodyWallet(const GeneratedCustodyWallet&) = delete;
    GeneratedCustodyWallet& operator=(const GeneratedCustodyWallet&) = delete;
    ~GeneratedCustodyWallet();
};

/** Generate a fresh 256-bit BIP39 wallet with a cryptographic RNG. */
[[nodiscard]] std::expected<GeneratedCustodyWallet, Error> generateCustodyWallet();

/**
 * Hashcash nonce for the exact HyperSnap native-onboarding challenge.
 * Returns a big-endian 8-byte nonce encoded as hex. Computation is CPU-bound:
 * call from a worker thread only, never from the Qt GUI thread.
 * The attempt budget limits resource usage; the caller may retry with a
 * refreshed anchor, must revalidate the anchor window before submission.
 */
[[nodiscard]] std::expected<std::string, Error> solveNativeOnboardPow(
    std::string_view custodyAddress, std::string_view anchorHex,
    std::uint32_t difficultyBits, std::uint64_t maxAttempts,
    std::uint64_t startNonce = 0,
    const std::atomic_bool* cancelled = nullptr);

/** Public-only native-onboarding custody proof. No wallet secret crosses the boundary. */
struct NativeOnboardSignature final {
    std::string address{};
    std::string signature{}; // 0x-prefixed EIP-712 r||s||v
};

/** Sign only a valid HyperSnap native Proof-of-Work bound to the exact anchor.
 * The caller must confirm live activation and submit only after user consent. */
[[nodiscard]] std::expected<NativeOnboardSignature, Error> signNativeOnboard(
    std::string_view recoveryWords, std::uint64_t anchorHeight,
    std::string_view anchorHex, std::string_view nonceHex,
    std::uint32_t difficultyBits);

/**
 * Farcaster on-chain registration/transfer authorization, NOT a HyperSnap
 * native registration. The EIP-712 domain is the canonical Optimism
 * IdRegistry contract (chainId 10); nonce must be read from that contract.
 * A Neynar first-party backend obtains a real FID reservation and validates
 * the custody address before this signature may be submitted.
 */
struct FarcasterIdTransferSignature final {
    std::string custodyAddress{};
    std::string signature{}; // EIP-712 low-s, 0x + r||s||v
    std::uint64_t fid{};
    std::uint64_t nonce{};
    std::uint64_t deadline{};
};

[[nodiscard]] std::expected<FarcasterIdTransferSignature, Error>
signFarcasterIdTransfer(std::string_view recoveryWords,
                       std::uint64_t reservedFid,
                       std::uint64_t idRegistryNonce,
                       std::uint64_t deadlineUnixSeconds);

/** Public-only EIP-191 proof for a server-issued one-shot OpenProof challenge. */
struct RecoveryProof final {
    std::string address{};
    std::string signature{}; // 0x-prefixed 65-byte r || s || v, v=27/28
};

/**
 * Derive only the public Ethereum custody address using Farcaster's BIP39
 * English dictionary and BIP44 m/44'/60'/0'/0/0 path.
 *
 * The implementation holds mnemonic/seed/child keys in short-lived cleansed
 * buffers. This primitive does NOT establish ownership of a Farcaster FID.
 * Optimism IdRegistry verification, a challenge-bound custody signature and
 * Snapchain signer authorization are separate required security gates.
 */
[[nodiscard]] std::expected<std::string, Error>
derivePublicCustodyAddress(std::string_view phrase);

/**
 * Sign an exact domain- and FID-bound OpenProof recovery challenge using an
 * ephemeral custody key. The caller MUST already have confirmed this address
 * maps to the same FID on Optimism and obtained the challenge from the trusted
 * first-party HTTPS gateway. No key material is returned or retained.
 */
[[nodiscard]] std::expected<RecoveryProof, Error>
signOneTimeIdentityProof(std::string_view phrase, std::uint64_t fid,
                         std::string_view challenge);

/** @brief Sign a one-use EIP-4361 OpenProof Ethereum-wallet challenge.
 * @details This proves only the custody address, never HyperSnap FID
 * ownership. The caller separately checks the confirmed native registry
 * before invoking this primitive. The exact origin, address, statement,
 * URI, version and chain ID are verified before deriving any secret.
 */
[[nodiscard]] std::expected<RecoveryProof, Error>
signOpenProofWalletChallenge(std::string_view phrase,
                             std::string_view expectedAddress,
                             std::string_view challenge);

/** Sign only the exact HyperSnap cloud-device enrollment domain. */
[[nodiscard]] std::expected<RecoveryProof, Error>
signCloudDeviceEnrollment(std::string_view phrase, std::uint64_t fid,
    std::string_view expectedAddress, std::string_view publicKey, std::string_view nonce);

/**
 * Materialize one strictly ephemeral custody authorizer after the first-party
 * recovery challenge has proved the same address owns the user FID.
 * The returned handle signs only a single exact EIP-712 KEY_ADD request,
 * zeroizes the derived key after authorization, and cannot export it.
 * No phrase or private key may be persisted in Qt properties or a network DTO.
 */
[[nodiscard]] std::expected<std::shared_ptr<capability::signer::ICustodyAuthorizer>, Error>
makeOneShotRecoveryAuthorizer(std::string_view phrase,
                              std::string_view verifiedCustodyAddress,
                              std::uint64_t verifiedFid);

} // namespace farcaster::platform::custody_recovery
