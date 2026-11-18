/*!
 * @file        src/providers/compatibility/browser_wallet/bridge.cppm
 * @brief       External browser Mini App wallet bridge.
 *
 * @details
 * Mediates origin-scoped EIP-1193 wallet sessions without exposing the
 * host's signing keys or UI internals.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

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
 * The bridge binds only to 127.0.0.1 and never receives private keys. The first
 * account-access request for each Mini App origin requires explicit host consent;
 * that grant is remembered only for this bridge process and is revoked on
 * permission revoke, chain/principal change and wallet disconnect. Origins are
 * independently checked at beginSession: public HTTPS authority only (no
 * credentials, path, query, fragment, private host, or non-default port), then
 * lowercased and normalized for a single consent principal. An invalid new
 * origin leaves any existing session untouched. Signing and transaction methods
 * always require an explicit approval click before reaching `window.ethereum`.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<capability::wallet::IEthereumWalletBridge>>
makeBridge(Config config);

} // namespace farcaster::provider::compatibility::browser_wallet::bridge
