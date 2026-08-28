module;

#include <expected>
#include <string>
#include <utility>
#include <vector>

module farcaster.provider.hypersnap.cast;

import farcaster.domain.cast;
import farcaster.foundation.error;
import farcaster.provider.hypersnap.codec;
import farcaster.provider.hypersnap.request;

namespace farcaster::provider::hypersnap::cast {

CastSource::CastSource(transport::http::IHttpTransport& transport, config::Config config)
    : m_transport(transport), m_config(std::move(config))
{
}

foundation::result::Result<domain::cast::Cast>
CastSource::get(const domain::cast::CastId& castId)
{
    if (castId.value.empty()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Cast id must not be empty."));
    }
    if (auto status = request::validateConfig(m_config); !status) {
        return std::unexpected(std::move(status.error()));
    }
    request::Client requests{m_transport, m_config};
    auto body = requests.getJson(
        "/v2/farcaster/cast",
        {{"identifier", castId.value}, {"type", "hash"}});
    if (!body) {
        return std::unexpected(std::move(body.error()));
    }
    return codec::decodeCast(*body);
}

}
