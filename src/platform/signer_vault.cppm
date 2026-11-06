/*!
 * @file        src/platform/signer_vault.cppm
 * @brief       Native secure signer vault factory.
 *
 * @details
 * Opens a platform-backed Ed25519 vault and fails with structured errors
 * when a secure backend is unavailable.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <memory>
#include <string>

export module farcaster.platform.signer_vault;

import farcaster.capability.signer;
import farcaster.foundation.result;

export namespace farcaster::platform::signer_vault {

/**
 * @brief Opens the platform-secure Ed25519 signer vault.
 * @param serviceName Stable application service identifier used by the OS credential store.
 * @return Secure vault or a structured error when no secure backend is available.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::ISignerVault>>
open(std::string serviceName);

}
