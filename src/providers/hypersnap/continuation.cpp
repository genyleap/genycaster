module;

#include <expected>
#include <optional>
#include <string>
#include <utility>

module farcaster.provider.hypersnap.continuation;

import farcaster.capability.model;
import farcaster.capability.pagination;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.model;
import farcaster.storage.continuation;

namespace farcaster::provider::hypersnap::continuation {

Store::Store(
    storage::continuation::ContinuationStore& store,
    provider::model::ProviderId providerId)
    : m_store(store), m_providerId(std::move(providerId))
{
}

foundation::result::Result<std::optional<std::string>> Store::resolve(
    const capability::model::CapabilityId& capabilityId,
    const capability::pagination::PageRequest& pageRequest)
{
    if (!pageRequest.continuation) {
        return std::optional<std::string>{};
    }
    auto record = m_store.resolve(*pageRequest.continuation);
    if (!record) {
        return std::unexpected(std::move(record.error()));
    }
    if (record->capabilityId.value != capabilityId.value || record->providerId.value != m_providerId.value) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Conflict,
            "Continuation token does not belong to the requested capability/provider route."));
    }
    return std::optional<std::string>{record->providerCursor};
}

foundation::result::Result<std::optional<capability::pagination::ContinuationToken>> Store::bind(
    const capability::model::CapabilityId& capabilityId,
    std::optional<std::string> providerCursor)
{
    if (!providerCursor || providerCursor->empty()) {
        return std::optional<capability::pagination::ContinuationToken>{};
    }
    auto token = m_store.bind(capabilityId, m_providerId, std::move(*providerCursor));
    if (!token) {
        return std::unexpected(std::move(token.error()));
    }
    return std::optional<capability::pagination::ContinuationToken>{std::move(*token)};
}

}
