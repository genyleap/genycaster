/*!
 * @file        ui/app/update/install.cppm
 * @brief       Verified package installation handoff.
 *
 * @details
 * Routes an already verified update package to the platform installer or a controlled post-exit helper without bypassing operating-system security.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;
#include "qt_fwd.hpp"

export module genycaster.ui.update.install;

export namespace genycaster::ui::update {
    enum class InstallMode { Supervised, SystemInstaller, Unsupported };

    // The detail is optional; a failed handoff returns Unsupported and fills it.
    [[nodiscard]] InstallMode launchVerifiedUpdate(
        const QString& verifiedPackage,
        const QString& expectedSha256,
        QString* errorDetail);
}
