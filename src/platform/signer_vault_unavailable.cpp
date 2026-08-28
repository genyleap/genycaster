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
        "No platform-secure protocol signer storage backend is available in this build."));
}

}
