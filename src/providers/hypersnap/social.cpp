module;

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.provider.hypersnap.social;

import farcaster.capability.catalog;
import farcaster.capability.model;
import farcaster.domain.social;
import farcaster.foundation.error;
import farcaster.provider.hypersnap.codec;
import farcaster.provider.hypersnap.continuation;
import farcaster.provider.hypersnap.request;

namespace farcaster::provider::hypersnap::social {
namespace {

[[nodiscard]] foundation::result::Result<std::size_t> validatedLimit(
    const capability::pagination::PageRequest& pageRequest)
{
    if (pageRequest.limit == 0 || pageRequest.limit > 100) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Hypersnap v2 page limit must be between 1 and 100."));
    }
    return pageRequest.limit;
}

[[nodiscard]] foundation::result::Result<capability::social::SocialGraphPage> readGraph(
    transport::http::IHttpTransport& transport,
    storage::continuation::ContinuationStore& continuationStore,
    const config::Config& config,
    const domain::identity::Fid fid,
    const bool isFollowers,
    const capability::model::CapabilityId& capabilityId,
    const std::string_view path,
    const capability::pagination::PageRequest& pageRequest)
{
    if (auto status = request::validateConfig(config); !status) {
        return std::unexpected(std::move(status.error()));
    }
    auto limit = validatedLimit(pageRequest);
    if (!limit) {
        return std::unexpected(std::move(limit.error()));
    }

    continuation::Store continuations{continuationStore, config.provider.id};
    auto cursor = continuations.resolve(capabilityId, pageRequest);
    if (!cursor) {
        return std::unexpected(std::move(cursor.error()));
    }
    std::vector<std::pair<std::string, std::string>> query{
        {"fid", std::to_string(fid.value)},
        {"limit", std::to_string(*limit)},
    };
    if (*cursor) {
        query.emplace_back("cursor", **cursor);
    }

    request::Client requests{transport, config};
    auto body = requests.getJson(path, query);
    if (!body) {
        return std::unexpected(std::move(body.error()));
    }
    auto payload = codec::decodeSocial(*body);
    if (!payload) {
        return std::unexpected(std::move(payload.error()));
    }

    capability::social::SocialGraphPage page{};
    page.profiles = std::move(payload->profiles);
    page.relations.reserve(page.profiles.size());
    for (const auto& profile : page.profiles) {
        page.relations.push_back(domain::social::SocialRelation{
            .source = isFollowers ? profile.fid : fid,
            .target = isFollowers ? fid : profile.fid,
            .kind = domain::social::SocialRelationKind::Follows,
        });
    }
    auto next = continuations.bind(capabilityId, std::move(payload->providerCursor));
    if (!next) {
        return std::unexpected(std::move(next.error()));
    }
    page.next = std::move(*next);
    return page;
}

}

SocialGraphSource::SocialGraphSource(
    transport::http::IHttpTransport& transport,
    storage::continuation::ContinuationStore& continuations,
    config::Config config)
    : m_transport(transport), m_continuations(continuations), m_config(std::move(config))
{
}

foundation::result::Result<capability::social::SocialGraphPage> SocialGraphSource::followers(
    const domain::identity::Fid fid,
    const capability::pagination::PageRequest& pageRequest)
{
    return readGraph(
        m_transport,
        m_continuations,
        m_config,
        fid,
        true,
        capability::catalog::graphFollowersRead(),
        "/v2/farcaster/user/followers",
        pageRequest);
}

foundation::result::Result<capability::social::SocialGraphPage> SocialGraphSource::following(
    const domain::identity::Fid fid,
    const capability::pagination::PageRequest& pageRequest)
{
    return readGraph(
        m_transport,
        m_continuations,
        m_config,
        fid,
        false,
        capability::catalog::graphFollowingRead(),
        "/v2/farcaster/user/following",
        pageRequest);
}

}
