
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
