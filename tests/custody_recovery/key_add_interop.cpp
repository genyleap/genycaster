#include <chrono>
#include <iostream>
#include <string>
#include <string_view>

import farcaster.platform.custody_recovery;
import farcaster.capability.signer;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.time;
import farcaster.protocol.message;

namespace {
std::string hexString(std::string_view bytes)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string result{};
    result.reserve(bytes.size() * 2);
    for (unsigned char byte : bytes) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return result;
}
}

int main()
{
    using namespace farcaster;
    // Well-known BIP39 24-word vector, never actual user credentials.
    constexpr auto phrase = "abandon abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon abandon abandon art";
    auto custody = platform::custody_recovery::makeOneShotRecoveryAuthorizer(
        phrase, "0xf278cf59f82edcf871d630f28ecc8056f25c1cdb", 42);
    if (!custody) return 1;
    const capability::signer::KeyAddRequest request{
        .fid = domain::identity::Fid{42},
        .publicKey = domain::signer::PublicKey{std::string(64, '1')},
        .scopes = {domain::signer::Scope::CastAdd, domain::signer::Scope::ReactionAdd},
        .ttlSeconds = 3600, .nonce = 9,
        .deadline = foundation::time::Timestamp{std::chrono::milliseconds{1780000000000LL}},
        .requestFid = domain::identity::Fid{42},
        .custodyAddress = "0xf278cf59f82edcf871d630f28ecc8056f25c1cdb",
    };
    auto result = (*custody)->authorize(request);
    if (!result) return 2;
    const auto deadline = protocol::message::toFarcasterTimestamp(request.deadline);
    if (!deadline) return 3;
    std::cout << *deadline << '\n'
              << hexString(result->custodySignature) << '\n'
              << hexString(result->signedKeyRequestMetadata) << '\n';
}
