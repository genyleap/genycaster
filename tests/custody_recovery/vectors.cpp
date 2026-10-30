
#include <cassert>
#include <iostream>
#include <string>

import farcaster.platform.custody_recovery;

using farcaster::platform::custody_recovery::derivePublicCustodyAddress;
using farcaster::platform::custody_recovery::Error;

int main()
{
    // The public, non-secret Hardhat mnemonic is a cross-language BIP44 vector.
    const std::string known = "test test test test test test test test test test test junk";
    const auto address = derivePublicCustodyAddress(known);
    if (!address || *address != "0xf39fd6e51aad88f6f4ce6ab8827279cfffb92266") {
        std::cerr << "Native BIP32/Keccak diverged: " << (address ? *address : "error=" + std::to_string(static_cast<int>(address.error()))) << "\n";
        return 1;
    }
    // A one-shot OpenProof SIWE challenge must be signed locally, with no
    // exported phrase or private key and only the canonical public proof.
    const std::string challenge =
        "genyleap.com wants you to sign in with your Ethereum account:\n"
        + *address + "\n\nFarcaster Auth\n\n"
        "URI: https://genyleap.com/account\n"
        "Version: 1\n"
        "Chain ID: 10\n"
        "Nonce: abcdefgh123456\n"
        "Issued At: 2026-10-30T00:00:00Z\n"
        "Resources:\n- farcaster://fid/123";
    using farcaster::platform::custody_recovery::signOneTimeIdentityProof;
    const auto proof = signOneTimeIdentityProof(known, 123, challenge);
    if (!proof) {
        std::cerr << "Native EIP-191 proof refused: " << static_cast<int>(proof.error()) << '\n';
        return 11;
    }
    if (proof->address != *address || proof->signature.size() != 132
        || proof->signature.substr(0, 2) != "0x"
        || (proof->signature.substr(130) != "1b"
            && proof->signature.substr(130) != "1c"))
        return 12;
    const auto wrongFid = signOneTimeIdentityProof(known, 124, challenge);
    if (wrongFid || wrongFid.error() != Error::InvalidChallenge) return 13;
    std::string modified = challenge;
    modified.replace(modified.find("Chain ID: 10"), 12, "Chain ID: 1 ");
    const auto wrongChain = signOneTimeIdentityProof(known, 123, modified);
    if (wrongChain || wrongChain.error() != Error::InvalidChallenge) return 14;
    modified = challenge;
    modified.replace(modified.find(*address), address->size(),
                     "0x2222222222222222222222222222222222222222");
    const auto wrongAddress = signOneTimeIdentityProof(known, 123, modified);
    if (wrongAddress || wrongAddress.error() != Error::InvalidChallenge) return 15;

    const auto uppercase = derivePublicCustodyAddress(
        "  TEST  TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST JUNK  ");
    if (!uppercase || *uppercase != *address) return 2;
    const auto zero = derivePublicCustodyAddress(
        "abandon abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon abandon abandon art");
    if (!zero || *zero != "0xf278cf59f82edcf871d630f28ecc8056f25c1cdb") return 3;
    const auto checksum = derivePublicCustodyAddress(
        "test test test test test test test test test test test abandon");
    if (checksum || checksum.error() != Error::InvalidChecksum) return 4;
    const auto unknown = derivePublicCustodyAddress(
        "test test test test test test test test test test test notaword");
    if (unknown || unknown.error() != Error::UnknownWord) return 5;
    const auto tooShort = derivePublicCustodyAddress("abandon abandon abandon");
    if (tooShort || tooShort.error() != Error::InvalidWordCount) return 6;
    const auto arbitrary = derivePublicCustodyAddress(std::string(16, '\0'));
    if (arbitrary || arbitrary.error() != Error::InvalidPhrase) return 7;
    std::cout << "Native BIP39 checksum, BIP32, Keccak and input bounds passed\n";
}
