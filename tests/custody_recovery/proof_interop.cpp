#include <iostream>
#include <string>
import farcaster.platform.custody_recovery;

int main()
{
    // Non-secret public Hardhat mnemonic, never a real user's recovery words.
    const std::string mnemonic =
        "test test test test test test test test test test test junk";
    constexpr auto address = "0xf39fd6e51aad88f6f4ce6ab8827279cfffb92266";
    const std::string challenge =
        "genyleap.com wants you to sign in with your Ethereum account:\n"
        + std::string{address} + "\n\nFarcaster Auth\n\n"
        "URI: https://genyleap.com/account\n"
        "Version: 1\n"
        "Chain ID: 10\n"
        "Nonce: abcdefgh123456\n"
        "Issued At: 2026-10-30T00:00:00Z\n"
        "Resources:\n- farcaster://fid/123";
    const auto proof =
        farcaster::platform::custody_recovery::signOneTimeIdentityProof(
            mnemonic, 123, challenge);
    if (!proof || proof->address != address) return 1;
    std::cout << proof->signature << '\n';
}
