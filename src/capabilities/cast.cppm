export module farcaster.capability.cast;

import farcaster.domain.cast;
import farcaster.foundation.result;

export namespace farcaster::capability::cast {

/**
 * @brief Blocking provider-neutral cast read port.
 *
 * Implementations may perform network I/O and must not be invoked on the GUI thread.
 */
class ICastSource {
public:
    /** @brief Destroys the cast source. */
    virtual ~ICastSource() = default;

    /** @brief Reads a canonical cast snapshot by canonical cast id. */
    [[nodiscard]] virtual foundation::result::Result<domain::cast::Cast>
    get(const domain::cast::CastId& castId) = 0;
};

}
