module;

#include <optional>
#include <string_view>

export module farcaster.local.entity;

import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.result;

export namespace farcaster::local::entity {

/**
 * @brief Stable port for canonical local entity persistence.
 *
 * Implementations may block and therefore belong on a storage executor rather
 * than the GUI thread.
 */
class IEntityStore {
public:
    /** @brief Destroys the persistence port. */
    virtual ~IEntityStore() = default;

    /**
     * @brief Upserts a canonical profile snapshot.
     * @param profile Profile value copied by the implementation before return.
     * @return Success or a structured storage failure.
     */
    [[nodiscard]] virtual foundation::result::Status
    putProfile(const domain::profile::Profile& profile) = 0;

    /**
     * @brief Loads a profile if it exists locally.
     * @param fid Canonical Farcaster identity to query.
     * @return Optional owned profile or a structured storage failure.
     */
    [[nodiscard]] virtual foundation::result::Result<std::optional<domain::profile::Profile>>
    findProfile(domain::identity::Fid fid) = 0;

    /**
     * @brief Loads a profile by canonical username if it exists locally.
     * @param username Case-insensitive canonical username lookup value.
     * @return Optional owned profile or a structured storage failure.
     */
    [[nodiscard]] virtual foundation::result::Result<std::optional<domain::profile::Profile>>
    findProfileByUsername(std::string_view username) = 0;

    /**
     * @brief Upserts a canonical cast snapshot.
     * @param cast Cast value copied by the implementation before return.
     * @return Success or a structured storage failure.
     */
    [[nodiscard]] virtual foundation::result::Status
    putCast(const domain::cast::Cast& cast) = 0;

    /**
     * @brief Loads a cast if it exists locally.
     * @param castId Canonical cast identifier to query.
     * @return Optional owned cast or a structured storage failure.
     */
    [[nodiscard]] virtual foundation::result::Result<std::optional<domain::cast::Cast>>
    findCast(const domain::cast::CastId& castId) = 0;

    /**
     * @brief Loads one cast without expanding locally persisted reply edges.
     *
     * Feed/list callers need the canonical cast and its embeds, not a complete
     * conversation tree for every visible row. Implementations should avoid
     * traversing reply relationships; this default keeps other stores source-
     * compatible while suppressing descendants in the returned value.
     *
     * @param castId Canonical cast identifier to query.
     * @return Optional cast with empty directReplies or a storage failure.
     */
    [[nodiscard]] virtual foundation::result::Result<std::optional<domain::cast::Cast>>
    findCastShallow(const domain::cast::CastId& castId)
    {
        auto cast = findCast(castId);
        if (cast && *cast) {
            (*cast)->directReplies.clear();
        }
        return cast;
    }
};

}
