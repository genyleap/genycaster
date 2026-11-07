/*!
 * @file        ui/app/bootstrap/monitoring.cppm
 * @brief       Notification badge and network recovery lifecycle wiring.
 *
 * @details
 * Subscribes the existing Qt presentation facade to authenticated-account
 * notification state, application focus changes and network reachability.
 * All timers and signal delivery remain GUI-thread-bound and stop with the
 * presentation facade. No protocol state or network credentials are owned by
 * this module.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include "qt_fwd.hpp"

export module genycaster.ui.bootstrap.monitoring;

export namespace genycaster::ui::bootstrap {

/**
 * @brief Connect per-FID seen-state and lifecycle badge refresh.
 *
 * @param application GUI application for foreground detection.
 * @param settings Borrowed preferences, which outlive the presentation facade.
 * @param bridge Existing C++-owned presentation facade.
 *
 * The observed account changes reset only this facade's badge startup state.
 * Asynchronous timers use bridge as their QObject context. Stored notification
 * cursors are always scoped to the authenticated FID.
 */
void connectNotificationBadge(
    QGuiApplication& application, QSettings& settings, PresentationBridge& bridge);

/**
 * @brief Enable automatic read recovery on network reconnection.
 *
 * @param bridge Existing C++-owned presentation facade.
 * @param enabled False for UI screenshot and smoke fixtures.
 *
 * Never retries an active or busy request. Recovery timers are children of the
 * bridge and cannot outlive it or restart the application during shutdown.
 */
void connectNetworkRecovery(PresentationBridge& bridge, bool enabled);

} // namespace genycaster::ui::bootstrap
