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
    /**
     * @brief Selected OS installation strategy.
     *
     * Supervised runs a first-party post-exit helper; SystemInstaller lets the
     * operating system enforce user consent and elevation; Unsupported fails
     * closed without launching untrusted executables.
     */
    enum class InstallMode { Supervised, SystemInstaller, Unsupported };

    /**
     * @brief Hand off an already SHA-256-verified compatible release artifact.
     *
     * @param verifiedPackage Absolute local path to the selected OS package.
     * @param expectedSha256 Trusted release digest checked again by helpers.
     * @param errorDetail Optional diagnostic output on failed handoff.
     * @return Installation strategy started, or Unsupported on failure.
     *
     * Signature checks, OS package permissions and installer consent remain
     * authoritative; callers must not bypass them or alter a running process.
     */
    [[nodiscard]] InstallMode launchVerifiedUpdate(
        const QString& verifiedPackage,
        const QString& expectedSha256,
        QString* errorDetail);
}
