/*!
 * @file        ui/app/bootstrap/wallet.cppm
 * @brief       Safe wallet approval launch policy and supported EVM chains.
 *
 * @details
 * Validates an external wallet approval target before any browser or wallet
 * is opened. URL template expansion and chain allowlisting are independent
 * from signer keys, provider selection and Qt GUI controller ownership.
 * Platform URL dispatch is supplied by the application shutdown boundary.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <expected>
#include <functional>
#include <string>
#include <vector>

export module genycaster.ui.bootstrap.wallet;

export namespace genycaster::ui::bootstrap {

/** @brief Reasons for refusing an unsafe external wallet launch. */
enum class WalletLaunchError {
    InvalidTemplate,
    UnsafeUrl,
};

/**
 * @brief Validated launch destination, containing no signer or wallet secret.
 *
 * The URL is derived from a trusted approval request and an explicitly
 * configured launch template. Its validation must precede external dispatch.
 */
struct WalletApprovalTarget final {
    std::string url{};
    bool preferInstalledWalletBrowser{false};
};

/**
 * @brief Validate and expand an external wallet approval URL.
 *
 * @param approvalUrl URL supplied by the approval protocol.
 * @param launchTemplate Optional user-configured URL template with {url} or
 *        {rawUrl} placeholder.
 * @param preferCustodyWalletBrowser Prefer an installed wallet-capable browser
 *        for local loopback approval on macOS when no template was configured.
 * @return A validated target, or a classified error with no external effect.
 *
 * Unsafe or malformed targets are rejected before any platform launcher runs.
 */
[[nodiscard]] std::expected<WalletApprovalTarget, WalletLaunchError> prepareWalletApproval(
    const std::string& approvalUrl,
    const std::string& launchTemplate,
    bool preferCustodyWalletBrowser);

/**
 * @brief Open an already validated target on the application GUI thread.
 *
 * @param target Target previously returned by prepareWalletApproval.
 * @param dispatchAppThread Platform dispatcher that invokes its operation
 *        only while the Qt application accepts new work. On browser targets,
 *        the dispatcher may execute immediately on the GUI event loop.
 * @return Whether the operating system accepted a launch request.
 *
 * The function does not grant wallet permissions or authenticate a signer.
 */
[[nodiscard]] bool openWalletApproval(
    const WalletApprovalTarget& target,
    const std::function<bool(std::function<bool()>)>& dispatchAppThread);

/**
 * @brief Determine the bounded EVM chain set exposed to Mini Apps.
 *
 * @return Approved, unique CAIP-2 identifiers in user-configured order, or
 *         the built-in allowlist when no override is present.
 *
 * Unknown and duplicate entries cannot expand the application's declared
 * capabilities or influence signer authorization.
 */
[[nodiscard]] std::vector<std::string> miniAppEthereumChains();

} // namespace genycaster::ui::bootstrap
