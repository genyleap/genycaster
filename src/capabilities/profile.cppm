export module farcaster.capability.profile;

import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.result;

export namespace farcaster::capability::profile {

/**
 * @brief Blocking provider-neutral profile read port.
 *
 * Implementations may perform network I/O and must not be invoked on the GUI thread.
 */
class IProfileSource {
public:
    /** @brief Destroys the profile source. */
    virtual ~IProfileSource() = default;

    /** @brief Reads a canonical profile snapshot for a Farcaster identity. */
    [[nodiscard]] virtual foundation::result::Result<domain::profile::Profile>
    get(domain::identity::Fid fid) = 0;
};

}
