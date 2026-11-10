/*!
 * @file        src/providers/hypersnap/config.cppm
 * @brief       HyperSnap provider configuration.
 *
 * @details
 * Owns endpoint settings and adapter-instance identity at composition
 * time rather than in domain models.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include "genycaster_deployment.hpp"
#include <string>

export module farcaster.provider.hypersnap.config;

import farcaster.provider.model;

export namespace farcaster::provider::hypersnap::config {

/**
 * @brief Hypersnap endpoint and provider-instance metadata owned by composition code.
 *
 * The adapter can target either a public or self-hosted node. Deployment and
 * privacy classification therefore belong to provider rather than to the
 * Hypersnap implementation itself.
 */
struct Config final {
    std::string baseUrl{genycaster::deployment::hypersnapOrigin};
    // Optional upstream hosted-service credential. Passed only as an HTTP
    // header, never embedded in endpoint URLs, logs or persistent settings.
    std::string apiKey{};
    provider::model::ProviderDescriptor provider{};
};

}
