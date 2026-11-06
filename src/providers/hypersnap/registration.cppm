/*!
 * @file        src/providers/hypersnap/registration.cppm
 * @brief       HyperSnap capability registration.
 *
 * @details
 * Advertises the exact read and derived operations supported by the
 * current adapter revision.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <vector>

export module farcaster.provider.hypersnap.registration;

import farcaster.capability.model;

export namespace farcaster::provider::hypersnap::registration {

/** @brief Returns the exact operation-level capabilities implemented by this adapter revision. */
[[nodiscard]] std::vector<capability::model::CapabilityDescriptor> capabilities();

}
