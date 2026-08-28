module;

#include <string_view>

export module farcaster.capability.identity;

import farcaster.domain.identity;
import farcaster.foundation.result;

export namespace farcaster::capability::identity {

/**
 * @brief Blocking provider-neutral identity resolution port.
 *
 * The initial contract resolves an existing Farcaster username to its FID.
 */
class IIdentityResolver {
public:
    /** @brief Destroys the identity resolver. */
    virtual ~IIdentityResolver() = default;

    /** @brief Resolves a canonical Farcaster username to a Farcaster identity. */
    [[nodiscard]] virtual foundation::result::Result<domain::identity::Fid>
    resolveUsername(std::string_view username) = 0;
};

}
