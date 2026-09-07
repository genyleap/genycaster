module;

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

export module farcaster.provider.compatibility.browser_wallet.bridge;

import farcaster.capability.wallet;
import farcaster.foundation.result;

export namespace farcaster::provider::compatibility::browser_wallet::bridge {

/** @brief Launch hook used to open the isolated loopback wallet bridge in a wallet-capable browser. */
using WalletLauncher = std::function<foundation::result::Status(const std::string&)>;

/** @brief Configuration for the external EIP-1193 Mini App wallet bridge. */
struct Config final {
    WalletLauncher launch{};
    std::vector<std::string> supportedChains{};
    std::chrono::milliseconds requestTimeout{std::chrono::minutes{5}};
    std::size_t maxRequestBytes{64U * 1024U};
};

/**
 * @brief Creates an isolated loopback EIP-1193 bridge backed by the user's real wallet provider.
 *
 * The bridge binds only to 127.0.0.1 and never receives private keys. Sensitive
 * signing, account-access and transaction methods require an explicit approval
 * click on the Genycaster bridge page before the request reaches `window.ethereum`.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<capability::wallet::IEthereumWalletBridge>>
makeBridge(Config config);

} // namespace farcaster::provider::compatibility::browser_wallet::bridge
