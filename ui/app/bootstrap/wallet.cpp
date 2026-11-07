/*!
 * @file        ui/app/bootstrap/wallet.cpp
 * @brief       Wallet approval launch policy and Mini App chain allowlist.
 *
 * @details
 * Validates URL template expansion before any platform side effect. The
 * application provides a shutdown-aware GUI dispatcher, preventing this
 * module from starting a new event loop or retaining controller pointers.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <algorithm>
#include <array>
#include <expected>
#include <functional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module genycaster.ui.bootstrap.wallet;

namespace genycaster::ui::bootstrap {

std::expected<WalletApprovalTarget, WalletLaunchError> prepareWalletApproval(
    const std::string& approvalUrl,
    const std::string& launchTemplate,
    const bool preferCustodyWalletBrowser)
{
    const QString rawUrl = QString::fromStdString(approvalUrl);
    const QString templateUrl = QString::fromStdString(launchTemplate);
    QString target = rawUrl;
    if (!templateUrl.isEmpty()) {
        if (!templateUrl.contains(QStringLiteral("{url}"))
            && !templateUrl.contains(QStringLiteral("{rawUrl}"))) {
            return std::unexpected{WalletLaunchError::InvalidTemplate};
        }
        target = templateUrl;
        target.replace(QStringLiteral("{url}"),
                       QString::fromLatin1(QUrl::toPercentEncoding(rawUrl)));
        target.replace(QStringLiteral("{rawUrl}"), rawUrl);
    }
    const QUrl parsed{target};
    const QString scheme = parsed.scheme().toLower();
    if (!parsed.isValid() || scheme.isEmpty()
        || scheme == QStringLiteral("javascript")
        || scheme == QStringLiteral("data")
        || scheme == QStringLiteral("file")) {
        return std::unexpected{WalletLaunchError::UnsafeUrl};
    }
    return WalletApprovalTarget{
        .url = parsed.toString().toStdString(),
        .preferInstalledWalletBrowser = preferCustodyWalletBrowser
            && templateUrl.isEmpty()
            && scheme == QStringLiteral("http")
            && parsed.host() == QStringLiteral("127.0.0.1"),
    };
}

bool openWalletApproval(
    const WalletApprovalTarget& target,
    const std::function<bool(std::function<bool()>)>& dispatchAppThread)
{
    if (!dispatchAppThread) return false;
    const QUrl parsed{QString::fromStdString(target.url)};
    const QString scheme = parsed.scheme().toLower();
    if (!parsed.isValid() || scheme.isEmpty()
        || scheme == QStringLiteral("javascript")
        || scheme == QStringLiteral("data")
        || scheme == QStringLiteral("file")) return false;

    const auto operation = [parsed, preferInstalledWalletBrowser =
                                       target.preferInstalledWalletBrowser] {
        bool opened{false};
#if defined(Q_OS_MACOS)
        if (preferInstalledWalletBrowser) {
            // Prefer an installed wallet-capable browser for loopback custody
            // authorization; leave ordinary links to the system handler.
            for (const QString& appName : {
                     QStringLiteral("Brave Browser"),
                     QStringLiteral("Google Chrome")}) {
                const QString bundle = appName + QStringLiteral(".app");
                if (QFileInfo::exists(QStringLiteral("/Applications/") + bundle)
                    || QFileInfo::exists(QDir::homePath()
                                        + QStringLiteral("/Applications/") + bundle)) {
                    opened = QProcess::startDetached(
                        QStringLiteral("/usr/bin/open"),
                        {QStringLiteral("-a"), appName, parsed.toString()});
                    if (opened) break;
                }
            }
        }
#else
        Q_UNUSED(preferInstalledWalletBrowser);
#endif
        if (!opened) opened = QDesktopServices::openUrl(parsed);
        return opened;
    };
    return dispatchAppThread(operation);
}

std::vector<std::string> miniAppEthereumChains()
{
    // CAIP-2 EVM identifiers accepted by the Mini App manifest policy.
    const std::array<std::string_view, 16> allowed{
        "eip155:1", "eip155:8453", "eip155:42161", "eip155:421614",
        "eip155:84532", "eip155:666666666", "eip155:100", "eip155:10",
        "eip155:11155420", "eip155:137", "eip155:11155111", "eip155:7777777",
        "eip155:130", "eip155:10143", "eip155:42220", "eip155:999",
    };
    const QString configured = qEnvironmentVariable("GENYCASTER_MINIAPP_EVM_CHAINS").trimmed();
    if (configured.isEmpty())
        return {allowed.begin(), allowed.end()};

    std::vector<std::string> selected{};
    for (const QString& token : configured.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const std::string chain = token.trimmed().toStdString();
        if (std::ranges::find(allowed, std::string_view{chain}) == allowed.end()) {
            qWarning().noquote()
                << "Ignoring unsupported GENYCASTER_MINIAPP_EVM_CHAINS entry:"
                << QString::fromStdString(chain);
            continue;
        }
        if (std::ranges::find(selected, chain) == selected.end())
            selected.push_back(chain);
    }
    return selected;
}

} // namespace genycaster::ui::bootstrap
