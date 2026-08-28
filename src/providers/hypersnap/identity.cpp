module;

#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.provider.hypersnap.identity;

import farcaster.domain.identity;
import farcaster.foundation.error;
import farcaster.provider.hypersnap.codec;
import farcaster.provider.hypersnap.request;

namespace farcaster::provider::hypersnap::identity {

IdentityResolver::IdentityResolver(transport::http::IHttpTransport& transport, config::Config config)
    : m_transport(transport), m_config(std::move(config))
{
}

foundation::result::Result<domain::identity::Fid>
IdentityResolver::resolveUsername(const std::string_view username)
{
    if (username.empty()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Username must not be empty."));
    }
    if (auto status = request::validateConfig(m_config); !status) {
        return std::unexpected(std::move(status.error()));
    }
    request::Client requests{m_transport, m_config};
    auto body = requests.getJson(
        "/v2/farcaster/user/by-username",
        {{"username", std::string{username}}});
    if (!body) {
        return std::unexpected(std::move(body.error()));
    }
    auto profile = codec::decodeUser(*body);
    if (!profile) {
        return std::unexpected(std::move(profile.error()));
    }
    return profile->fid;
}

}
