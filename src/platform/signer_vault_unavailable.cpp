module;

#include <expected>
#include <memory>
#include <string>

module farcaster.platform.signer_vault;

import farcaster.capability.signer;
import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::platform::signer_vault {

foundation::result::Result<std::unique_ptr<capability::signer::ISignerVault>>
open(std::string)
{
    return std::unexpected(foundation::error::Error::make(
        foundation::error::ErrorCode::Unsupported,
        "Platform-secure protocol signer storage is not implemented on this platform yet."));
}

}
