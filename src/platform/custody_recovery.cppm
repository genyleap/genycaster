module;

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

export module farcaster.platform.custody_recovery;

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

} // namespace farcaster::platform::custody_recovery
