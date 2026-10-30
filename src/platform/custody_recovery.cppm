module;

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

} // namespace farcaster::platform::custody_recovery
