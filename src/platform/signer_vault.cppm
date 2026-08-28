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
