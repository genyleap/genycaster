/*!
 * @file        ui/mobile/miniapp_runtime.cppm
 * @brief       Executable Mini App runtime ownership.
 *
 * @details
 * Composes isolated app sessions, manifests, host SDK routing and
 * platform-specific permission boundaries.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include "miniapp_catalog_controller.hpp"
#include "miniapp_host_controller.hpp"
#include "miniapp_task_manager_controller.hpp"
#include <memory>
#include <string>

export module farcaster.runtime.miniapp;

import farcaster.capability.wallet;

export namespace farcaster::runtime::miniapp {

/**
 * @brief Owns isolated Mini App sessions, manifest resolution and host SDK routing.
 *
 * The runtime keeps up to four native Mini App documents alive concurrently.
 * Window presentation keeps independent documents visible in parallel while
 * exactly one session is active/foreground for host UI and EIP-1193 wallet
 * authority. Drawer presentations remain single-visible by design. Discovery
 * and local collection state remain
 * owned by the catalogue controller, while task telemetry is projected through
 * the task-manager controller.
 */
class MiniAppRuntime final {
public:
    explicit MiniAppRuntime(
        MiniAppHostController& controller,
        MiniAppCatalogController& catalog,
        MiniAppTaskManagerController& taskManager,
        bool useDirectProxy,
        qulonglong clientFid,
        capability::wallet::IEthereumWalletBridge* walletBridge = nullptr,
        std::string optimismRpcUrl = "https://mainnet.optimism.io");
    ~MiniAppRuntime();
    /** Bounded by the verified account's Geny Pro entitlement (1 or 4). */
    void setSessionLimit(int limit);
    MiniAppRuntime(const MiniAppRuntime&) = delete;
    auto operator=(const MiniAppRuntime&) -> MiniAppRuntime& = delete;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}
