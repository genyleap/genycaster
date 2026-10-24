module;

#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <vector>

module farcaster.provider.hypersnap.continuation;

import farcaster.capability.model;
import farcaster.capability.pagination;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.model;
import farcaster.storage.continuation;

namespace farcaster::provider::hypersnap::continuation {

std::string selectorScope(
    const std::vector<std::pair<std::string, std::string>>& selectors)
{
    // User FID and canonical channel identity define the query dataset;
    // capability identity already distinguishes feed variants. Never bind a
    // cursor to a page size or provider-native cursor/sort parameter.
    for (const auto& [key, value] : selectors) {
        if (key == "fid" || key == "channel_ids") {
            return key + ":" + value;
        }
    }
    return {};
}

Store::Store(
    storage::continuation::ContinuationStore& store,
    provider::model::ProviderId providerId,
    std::string endpointIdentity,
    std::string selectorIdentity)
    : m_store(store), m_providerId(std::move(providerId)),
      m_endpointPrefix("indexed-v3:" + std::move(endpointIdentity) + "\n"
          + std::to_string(selectorIdentity.size()) + ":" + selectorIdentity + "\n")
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
    if (!record->providerCursor.starts_with(m_endpointPrefix)) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Conflict,
            "Indexed read continuation belongs to a different endpoint or query selector; "
            "restart this feed from its first page."));
    }
    return std::optional<std::string>{record->providerCursor.substr(m_endpointPrefix.size())};
}

foundation::result::Result<std::optional<capability::pagination::ContinuationToken>> Store::bind(
    const capability::model::CapabilityId& capabilityId,
    std::optional<std::string> providerCursor)
{
    if (!providerCursor || providerCursor->empty()) {
        return std::optional<capability::pagination::ContinuationToken>{};
    }
    auto token = m_store.bind(capabilityId, m_providerId,
                              m_endpointPrefix + std::move(*providerCursor));
    if (!token) {
        return std::unexpected(std::move(token.error()));
    }
    return std::optional<capability::pagination::ContinuationToken>{std::move(*token)};
}

}
