module;

#include <memory>
#include <string>

export module farcaster.platform.signer_vault;

import farcaster.capability.signer;
import farcaster.foundation.result;

export namespace farcaster::platform::signer_vault {

/**
 * @brief Opens the platform-native secure Ed25519 signer vault.
 * @param serviceName Stable application service identifier used by the OS credential store.
 * @return Secure vault or Unsupported when this platform adapter is not implemented.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::ISignerVault>>
open(std::string serviceName);

}
