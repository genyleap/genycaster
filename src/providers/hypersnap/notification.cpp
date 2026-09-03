module;

#include <chrono>
#include <expected>
#include <string>
#include <utility>
#include <vector>

module farcaster.provider.hypersnap.notification;

import farcaster.capability.catalog;
import farcaster.foundation.error;
import farcaster.provider.hypersnap.codec;
import farcaster.provider.hypersnap.continuation;
import farcaster.provider.hypersnap.request;

namespace farcaster::provider::hypersnap::notification {

NotificationSource::NotificationSource(
    transport::http::IHttpTransport& transport,
    storage::continuation::ContinuationStore& continuations,
    config::Config config)
    : m_transport(transport), m_continuations(continuations), m_config(std::move(config))
{
}

foundation::result::Result<capability::notification::NotificationPage>
NotificationSource::notifications(
    const domain::identity::Fid viewer,
    const capability::pagination::PageRequest& pageRequest)
{
    if (viewer.value == 0 || pageRequest.limit == 0 || pageRequest.limit > 100) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Notification viewer and page limit must be valid."));
    }
    if (auto status = request::validateConfig(m_config); !status) {
        return std::unexpected(std::move(status.error()));
    }

    const auto capabilityId = capability::catalog::notificationsRead();
    continuation::Store continuations{m_continuations, m_config.provider.id};
    auto cursor = continuations.resolve(capabilityId, pageRequest);
    if (!cursor) {
        return std::unexpected(std::move(cursor.error()));
    }

    std::vector<std::pair<std::string, std::string>> query{
        {"fid", std::to_string(viewer.value)},
        {"limit", std::to_string(pageRequest.limit)},
    };
    if (*cursor) {
        query.emplace_back("cursor", **cursor);
    }

    request::Client requests{m_transport, m_config};
    auto body = requests.getJson(
        "/v2/farcaster/notifications",
        query,
        request::GetOptions{.timeout = std::chrono::milliseconds{10'000}});
    if (!body) {
        return std::unexpected(std::move(body.error()));
    }
    auto decoded = codec::decodeNotifications(*body);
    if (!decoded) {
        return std::unexpected(std::move(decoded.error()));
    }

    capability::notification::NotificationPage page{};
    page.items = std::move(decoded->items);
    auto next = continuations.bind(capabilityId, std::move(decoded->providerCursor));
    if (!next) {
        return std::unexpected(std::move(next.error()));
    }
    page.next = std::move(*next);
    return page;
}

}
