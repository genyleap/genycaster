#ifdef NDEBUG
#undef NDEBUG // Security test assertions must run in Release CI builds.
#endif
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

import farcaster.platform.custody_recovery;
import farcaster.capability.signer;
import farcaster.capability.signer_attestation;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.time;

namespace {
constexpr auto ValidAddress = "0xf278cf59f82edcf871d630f28ecc8056f25c1cdb";
const std::string KnownMnemonic =
    "abandon abandon abandon abandon abandon abandon abandon abandon "
    "abandon abandon abandon abandon abandon abandon abandon abandon "
    "abandon abandon abandon abandon abandon abandon abandon art";

auto request(const std::uint64_t requestFid = 42U)
{
    using namespace farcaster;
    return capability::signer::KeyAddRequest{
        .fid = domain::identity::Fid{42},
        .publicKey = domain::signer::PublicKey{std::string(64, '1')},
        .scopes = domain::signer::standardSocialScopes(),
        .ttlSeconds = 3600,
        .nonce = 9,
        .deadline = foundation::time::now() + std::chrono::minutes{20},
        .requestFid = domain::identity::Fid{requestFid},
        .custodyAddress = ValidAddress,
    };
}
}

int main()
{
    using namespace farcaster;
    using platform::custody_recovery::makeOneShotRecoveryAuthorizer;
    auto signer = makeOneShotRecoveryAuthorizer(KnownMnemonic, ValidAddress, 42);
    assert(signer && *signer);
    auto requestOne = request();
    const auto signedKey = (*signer)->authorize(requestOne);
    assert(signedKey);
    assert(signedKey->custodySignature.size() == 65U);
    assert(signedKey->signedKeyRequestMetadata.size() >= 192U);
    // The private key cannot be reused for a second signer authorization.
    assert(!(*signer)->authorize(requestOne));

    auto wrongAddress = makeOneShotRecoveryAuthorizer(
        KnownMnemonic, "0x1111111111111111111111111111111111111111", 42);
    assert(!wrongAddress);
    const auto shortPhrase = makeOneShotRecoveryAuthorizer(
        "test test test test test test test test test test test junk",
        "0xf39fd6e51aad88f6f4ce6ab8827279cfffb92266", 42);
    assert(!shortPhrase);
    const auto wrongFid = makeOneShotRecoveryAuthorizer(KnownMnemonic, ValidAddress, 43);
    assert(wrongFid);
    assert(!(*wrongFid)->authorize(request()));
    signer = makeOneShotRecoveryAuthorizer(KnownMnemonic, ValidAddress, 42);
    assert(signer);
    auto swapped = request();
    swapped.custodyAddress = "0x2222222222222222222222222222222222222222";
    assert(!(*signer)->authorize(swapped));

    // App-attributed requests require an independent attestation, never
    // defaulting to self-attribution after changing the selected provider.
    signer = makeOneShotRecoveryAuthorizer(KnownMnemonic, ValidAddress, 42);
    assert(signer);
    assert(!(*signer)->authorize(request(99)));
    signer = makeOneShotRecoveryAuthorizer(KnownMnemonic, ValidAddress, 42);
    assert(signer);
    const capability::signer_attestation::SignedKeyRequestAttestation proof{
        .requestSignerAddress = std::string(20, '\x11'),
        .signature = std::string(65, '\x22'),
    };
    const auto attributed = (*signer)->authorize(request(99), proof);
    assert(attributed);
    assert(attributed->custodySignature.size() == 65U);
    assert(!attributed->signedKeyRequestMetadata.empty());

    std::cout << "Native one-shot EIP-712 KEY_ADD and replay isolation passed\n";
}
