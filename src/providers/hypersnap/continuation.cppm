module;

#include <optional>
#include <string>

export module farcaster.provider.hypersnap.continuation;

import farcaster.capability.model;
import farcaster.capability.pagination;
import farcaster.foundation.result;
import farcaster.provider.model;
import farcaster.storage.continuation;

export namespace farcaster::provider::hypersnap::continuation {

/**
 * @brief Provider-private continuation service over a borrowed SQLite mapping store.
 *
 * The backing store must outlive this object. Raw provider cursors never cross
 * this module into application-facing pagination tokens.
 */
class Store final {
public:
    /** @brief Creates a continuation service for one stable provider instance. */
    Store(
        storage::continuation::ContinuationStore& store,
        provider::model::ProviderId providerId);

    /** @brief Resolves an application token and enforces capability/provider affinity. */
    [[nodiscard]] foundation::result::Result<std::optional<std::string>> resolve(
        const capability::model::CapabilityId& capabilityId,
        const capability::pagination::PageRequest& pageRequest);

    /** @brief Binds a raw provider cursor to an application-safe token. */
    [[nodiscard]] foundation::result::Result<std::optional<capability::pagination::ContinuationToken>> bind(
        const capability::model::CapabilityId& capabilityId,
        std::optional<std::string> providerCursor);

private:
    storage::continuation::ContinuationStore& m_store;
    provider::model::ProviderId m_providerId{};
};

}
