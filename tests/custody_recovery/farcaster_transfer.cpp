#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

import farcaster.platform.custody_recovery;

int main()
{
    using namespace farcaster::platform::custody_recovery;
    constexpr auto mnemonic =
        "abandon abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon abandon abandon art";
    const auto now = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    const auto transfer = signFarcasterIdTransfer(mnemonic, 1777777, 3, now + 1800);
    assert(transfer);
    assert(transfer->custodyAddress
           == "0xf278cf59f82edcf871d630f28ecc8056f25c1cdb");
    assert(transfer->signature.size() == 132);
    assert(transfer->fid == 1777777 && transfer->nonce == 3);
    assert(!signFarcasterIdTransfer(mnemonic, 0, 0, now + 1800));
    // Network identity is scoped by its registry, not by any numeric FID
    // threshold; the reserver must prove this FID belongs to IdRegistry.
    assert(signFarcasterIdTransfer(mnemonic, (1ULL << 32), 0, now + 1800));
    assert(!signFarcasterIdTransfer(mnemonic, 1777777, 3, now - 10));
    assert(!signFarcasterIdTransfer(mnemonic, 1777777, 3, now + 7200));
    assert(!signFarcasterIdTransfer("not a phrase", 1777777, 3, now + 1800));
    // The independently verified JS EIP-712 implementation checks the
    // signature domain contract and recovers the expected custody address.
    std::cout << transfer->custodyAddress << '\n'
              << transfer->fid << '\n'
              << transfer->nonce << '\n'
              << transfer->deadline << '\n'
              << transfer->signature << '\n';
}
