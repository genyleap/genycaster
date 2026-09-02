module;

#include <chrono>
#include <string>

export module farcaster.provider.public_data.holdings;

import farcaster.foundation.result;
import farcaster.transport.http;

export namespace farcaster::provider::public_data::holdings {

/** @brief One ERC-20 wallet holding resolved from a public JSON-RPC endpoint. */
struct TokenHolding final {
    std::string wallet{};
    std::string contract{};
    double balance{};
    unsigned int decimals{18};
};

/** @brief Read-only Base ERC-20 balance provider used by the context rail. */
class WalletAssetSource final {
public:
    explicit WalletAssetSource(
        transport::http::IHttpTransport& transport,
        std::string baseRpcUrl = "https://mainnet.base.org",
        std::chrono::milliseconds timeout = std::chrono::milliseconds{8'000});

    [[nodiscard]] foundation::result::Result<TokenHolding>
    erc20Balance(const std::string& wallet, const std::string& contract);

private:
    transport::http::IHttpTransport& m_transport;
    std::string m_baseRpcUrl;
    std::chrono::milliseconds m_timeout;
};

} // namespace farcaster::provider::public_data::holdings
