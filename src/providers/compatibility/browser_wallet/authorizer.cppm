/*!
 * @file        src/providers/compatibility/browser_wallet/authorizer.cppm
 * @brief       Browser custody approval compatibility adapter.
 *
 * @details
 * Launches a short-lived external wallet authorization ceremony through
 * an isolated localhost session.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <chrono>
#include <functional>
#include <memory>
#include <string>

export module farcaster.provider.compatibility.browser_wallet.authorizer;

import farcaster.capability.signer;
import farcaster.foundation.result;

export namespace farcaster::provider::compatibility::browser_wallet::authorizer {

/** @brief Cross-platform launch hook used to hand a one-shot approval URL to a wallet/browser. */
using ApprovalLauncher = std::function<foundation::result::Status(const std::string&)>;

/** @brief Configuration for the loopback EIP-1193 custody authorization flow. */
struct Config final {
    ApprovalLauncher launch{};
    std::chrono::milliseconds approvalTimeout{std::chrono::minutes{5}};
    std::chrono::milliseconds requestTimeout{std::chrono::seconds{15}};
    std::size_t maxRequestBytes{32U * 1024U};
};

/**
 * @brief Creates a custody authorizer backed by a local one-shot EIP-1193 approval page.
 *
 * The provider binds only to 127.0.0.1, never stores signatures, and delegates URL opening to
 * the caller so Qt/desktop/mobile launch policy remains outside the protocol/provider layer.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::ICustodyAuthorizer>>
makeAuthorizer(Config config);

}
