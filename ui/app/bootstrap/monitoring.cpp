/*!
 * @file        ui/app/bootstrap/monitoring.cpp
 * @brief       Runtime notification and network recovery GUI wiring.
 *
 * @details
 * Connects notification cursors and network-lifecycle retries with Qt-owned
 * callback contexts. Per-account badge state is held in a shared capture
 * rather than referencing temporary entrypoint stack variables.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include "presentation_bridge.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QNetworkInformation>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QTimer>

#include <memory>

module genycaster.ui.bootstrap.monitoring;

namespace genycaster::ui::bootstrap {

void connectNotificationBadge(
    QGuiApplication& application, QSettings& settings, PresentationBridge& presentationBridge)
{
    struct AccountBadgeState final {
        qulonglong fid{0};
        bool bootstrapped{false};
    };
    const auto state = std::make_shared<AccountBadgeState>();
    QObject::connect(&presentationBridge, &PresentationBridge::notificationSeenAtChanged,
        &presentationBridge, [&settings](const qulonglong fid, const qlonglong observedAtMs) {
            if (fid == 0) {
                return;
            }
            settings.setValue(
                QStringLiteral("notifications/seen_at/%1").arg(fid),
                observedAtMs);
            settings.sync();
        });
    QObject::connect(&application, &QGuiApplication::applicationStateChanged,
        &presentationBridge, [&presentationBridge](const Qt::ApplicationState state) {
            if (state != Qt::ApplicationActive || !presentationBridge.isAuthenticated()) {
                return;
            }
            // Refresh after focus settles so returning from sleep/background does not
            // leave a stale navigation badge for another polling interval.
            QTimer::singleShot(250, &presentationBridge, [&presentationBridge] {
                presentationBridge.refreshNotificationBadge();
            });
        });
    QObject::connect(&presentationBridge, &PresentationBridge::stateChanged,
        &presentationBridge,
        [&presentationBridge, &settings, state] {
            const qulonglong fid = presentationBridge.isAuthenticated()
                ? presentationBridge.accountFid() : 0;
            if (fid != state->fid) {
                state->fid = fid;
                state->bootstrapped = false;
                if (fid != 0) {
                    presentationBridge.restoreNotificationSeenAt(
                        fid,
                        settings.value(
                            QStringLiteral("notifications/seen_at/%1").arg(fid),
                            0).toLongLong());
                }
            }
            if (fid != 0 && presentationBridge.runtimeReady() && !state->bootstrapped) {
                state->bootstrapped = true;
                QTimer::singleShot(750, &presentationBridge, [&presentationBridge] {
                    presentationBridge.refreshNotificationBadge();
                });
            }
        });
}

void connectNetworkRecovery(PresentationBridge& presentationBridge, const bool enabled)
{
    if (enabled) {
        QNetworkInformation::loadDefaultBackend();
        if (auto* networkInformation = QNetworkInformation::instance()) {
            const auto initialReachability = networkInformation->reachability();
            const auto applyReachability = [&presentationBridge](const QNetworkInformation::Reachability reachability) {
                const bool known = reachability != QNetworkInformation::Reachability::Unknown;
                const bool online = reachability != QNetworkInformation::Reachability::Disconnected;
                presentationBridge.applyNetworkReachability(known, online);
            };
            applyReachability(initialReachability);
            QObject::connect(networkInformation, &QNetworkInformation::reachabilityChanged,
                &presentationBridge,
                [&presentationBridge, previousReachability = initialReachability, applyReachability]
                (const QNetworkInformation::Reachability reachability) mutable {
                    const bool reconnected = previousReachability == QNetworkInformation::Reachability::Disconnected
                        && reachability != QNetworkInformation::Reachability::Disconnected
                        && reachability != QNetworkInformation::Reachability::Unknown;
                    previousReachability = reachability;
                    applyReachability(reachability);
                    if (reconnected && presentationBridge.runtimeReady() && !presentationBridge.isBusy()) {
                        QTimer::singleShot(350, &presentationBridge, [&presentationBridge] {
                            if (!presentationBridge.isBusy() && !presentationBridge.isOffline()) {
                                presentationBridge.retryAutomatically();
                            }
                        });
                    }
                });
        } else {
            presentationBridge.applyNetworkReachability(false, true);
        }

        auto* const sourceRecoveryTimer = new QTimer(&presentationBridge);
        sourceRecoveryTimer->setInterval(35'000);
        QObject::connect(sourceRecoveryTimer, &QTimer::timeout, &presentationBridge,
            [&presentationBridge] {
                if (!presentationBridge.runtimeReady() || presentationBridge.isBusy() ||
                    presentationBridge.isOffline() || !presentationBridge.sourceUnavailable()) {
                    return;
                }
                presentationBridge.retryAutomatically();
            });
        sourceRecoveryTimer->start();
    }
}

} // namespace genycaster::ui::bootstrap
