module;

#include <chrono>
#include <memory>
#include <string>

export module farcaster.provider.compatibility.auth;

import farcaster.capability.auth;
import farcaster.domain.identity;
import farcaster.foundation.result;
import farcaster.transport.http;

export namespace farcaster::provider::compatibility::auth {

/** @brief Infrastructure configuration for the Farcaster SIWF compatibility adapter. */
struct Config final {
    std::string relayBaseUrl{"https://relay.farcaster.xyz"};
    std::string relayVersion{"v1"};
    std::string optimismRpcUrl{"https://mainnet.optimism.io"};
    std::chrono::milliseconds requestTimeout{10'000};
    std::chrono::minutes attemptLifetime{5};
    std::chrono::hours sessionLifetime{24};
};

/**
 * @brief Creates the current Farcaster relay-backed SIWF compatibility adapter.
 * @param transport Blocking provider-neutral HTTP transport.
 * @param config Relay/RPC infrastructure configuration.
 * @return Owned authentication adapter or a validation failure.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<capability::auth::ICompatibilityAuthentication>>
makeAuthentication(transport::http::IHttpTransport& transport, Config config = {});

/**
 * Resolve an already locally derived PUBLIC custody address on Optimism.
 * Requires chain 10, a nonzero idOf(address) and the inverse custodyOf(fid).
 * Does not accept or transmit a mnemonic, sign a challenge, or start a session.
 * This check is mandatory before native custody-recovery authorization.
 */
[[nodiscard]] foundation::result::Result<domain::identity::Fid>
resolveRecoveryCustodyFid(
    transport::http::IHttpTransport& transport,
    std::string custodyAddress,
    Config config = {});

/**
 * A one-use OpenProof challenge. Contains only public identity and a short-
 * lived opaque ticket. The custody phrase and key are NEVER passed here.
 */
struct RecoveryChallenge final {
    domain::identity::Fid fid{};
    std::string ticket{};
    std::string message{};
};

struct RecoveryIdentity final {
    domain::identity::Fid fid{};
    std::string issuer{};
    std::string subject{};
    // Populated by trusted native composition after IdRegistry verification.
    // The gateway does not return custody keys or recovery words.
    std::string custodyAddress{};
    std::string accountTicket{}; // C++-only ephemeral account grant
    int expiresSeconds{};
};

/** After IdRegistry verification, request a fresh gateway-bound proof. */
[[nodiscard]] foundation::result::Result<RecoveryChallenge>
startRecoveryChallenge(transport::http::IHttpTransport& transport,
                       std::string gatewayOrigin, domain::identity::Fid fid,
                       std::string custodyAddress);

/** Submit only the EIP-191 signature for the exact, previously issued ticket. */
[[nodiscard]] foundation::result::Result<RecoveryIdentity>
completeRecoveryChallenge(transport::http::IHttpTransport& transport,
                          std::string gatewayOrigin,
                          const RecoveryChallenge& challenge,
                          std::string signature);

}
