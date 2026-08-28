module;

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
    std::string baseUrl{"https://haatz.quilibrium.com"};
    provider::model::ProviderDescriptor provider{};
};

}
