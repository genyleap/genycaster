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
    // Optional upstream hosted-service credential. Passed only as an HTTP
    // header, never embedded in endpoint URLs, logs or persistent settings.
    std::string apiKey{};
    provider::model::ProviderDescriptor provider{};
};

}
