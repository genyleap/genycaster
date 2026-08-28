module;

#include <chrono>
#include <memory>
#include <string>

export module farcaster.provider.compatibility.auth;

import farcaster.capability.auth;
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

}
