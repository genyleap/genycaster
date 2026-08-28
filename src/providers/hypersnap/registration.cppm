module;

#include <vector>

export module farcaster.provider.hypersnap.registration;

import farcaster.capability.model;

export namespace farcaster::provider::hypersnap::registration {

/** @brief Returns the exact operation-level capabilities implemented by this adapter revision. */
[[nodiscard]] std::vector<capability::model::CapabilityDescriptor> capabilities();

}
