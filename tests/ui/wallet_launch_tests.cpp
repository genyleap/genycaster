/*!
 * @file        tests/ui/wallet_launch_tests.cpp
 * @brief       Wallet approval URL and Mini App EVM chain policy regressions.
 *
 * @details
 * Verifies pure launch preparation without opening a browser or invoking a
 * wallet. Unknown chain IDs and unsafe schemes must fail closed.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#include <QCoreApplication>
#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QUrl>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

import genycaster.ui.bootstrap.wallet;

namespace {
bool check(const bool success, const char* label)
{
    if (!success) qCritical().noquote() << "Wallet launch policy:" << label;
    return success;
}
}

int main(int argc, char** argv)
{
    QCoreApplication application{argc, argv};
    using genycaster::ui::bootstrap::WalletLaunchError;
    using genycaster::ui::bootstrap::WalletApprovalTarget;
    using genycaster::ui::bootstrap::prepareWalletApproval;
    using genycaster::ui::bootstrap::openWalletApproval;
    using genycaster::ui::bootstrap::miniAppEthereumChains;
    bool passed = true;

    {
        const auto target = prepareWalletApproval(
            "https://wallet.example/approve?request=abc", "", false);
        passed &= check(target.has_value(), "normal HTTPS approval allowed");
        if (target) {
            passed &= check(target->url.find("https://wallet.example") == 0,
                            "external approval target retained");
            passed &= check(!target->preferInstalledWalletBrowser,
                            "normal HTTPS does not force installed browser");
        }
    }
    {
        const auto target = prepareWalletApproval(
            "http://127.0.0.1:4455/approve", "", true);
        passed &= check(target && target->preferInstalledWalletBrowser,
                        "loopback native custody route prefers installed wallet browser");
    }
    {
        const auto target = prepareWalletApproval(
            "http://127.0.0.1:4455/approve", "wallet://request?link={url}", true);
        passed &= check(target && !target->preferInstalledWalletBrowser,
                        "explicit template disables automatic browser preference");
        passed &= check(target && target->url.find("wallet://request?link=") == 0,
                        "template URL expanded");
        if (target) {
            passed &= check(target->url.find("http%3A%2F%2F127.0.0.1") != std::string::npos,
                            "approval URL is percent-encoded inside template");
        }
    }
    {
        const auto target = prepareWalletApproval(
            "https://example.test/a", "wallet://approval?raw={rawUrl}", false);
        passed &= check(target && target->url.find("https://example.test/a") != std::string::npos,
                        "rawUrl placeholder matches existing policy");
    }
    {
        const auto target = prepareWalletApproval(
            "https://example.test/a", "wallet://approval?no-placeholder", false);
        passed &= check(!target && target.error() == WalletLaunchError::InvalidTemplate,
                        "template without placeholder rejected");
    }
    for (const std::string& unsafe : {
             "javascript:alert(1)", "data:text/html,hello",
             "file:///tmp/wallet-key", "not a url"}) {
        const auto target = prepareWalletApproval(unsafe, "", false);
        passed &= check(!target && target.error() == WalletLaunchError::UnsafeUrl,
                        "unsafe external scheme rejected");
    }
    {
        // A caller cannot bypass the launch validator by constructing a
        // malicious target and handing it to the launcher.
        bool dispatched = false;
        const WalletApprovalTarget forged{"file:///tmp/private", false};
        const bool opened = openWalletApproval(
            forged, [&dispatched](std::function<bool()>) {
                dispatched = true;
                return true;
            });
        passed &= check(!opened && !dispatched, "dispatch never sees an unsafe forged URL");
    }

    const bool oldExists = qEnvironmentVariableIsSet("GENYCASTER_MINIAPP_EVM_CHAINS");
    const QByteArray oldValue = qgetenv("GENYCASTER_MINIAPP_EVM_CHAINS");
    qunsetenv("GENYCASTER_MINIAPP_EVM_CHAINS");
    {
        const auto defaults = miniAppEthereumChains();
        passed &= check(defaults.size() == 16, "default EVM chain allowlist count");
        passed &= check(std::ranges::find(defaults, "eip155:8453") != defaults.end(),
                        "Base Mainnet remains supported by default");
    }
    qputenv("GENYCASTER_MINIAPP_EVM_CHAINS",
            " eip155:10, eip155:8453, fake:9000, eip155:10 ");
    {
        const auto chains = miniAppEthereumChains();
        passed &= check(chains.size() == 2, "unknown and duplicate chain entries removed");
        passed &= check(chains.size() >= 2 && chains[0] == "eip155:10"
                        && chains[1] == "eip155:8453",
                        "explicit allowed chain ordering retained");
    }
    if (oldExists)
        qputenv("GENYCASTER_MINIAPP_EVM_CHAINS", oldValue);
    else
        qunsetenv("GENYCASTER_MINIAPP_EVM_CHAINS");
    return passed ? 0 : 1;
}
