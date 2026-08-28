module;

#include <expected>
#include <string>
#include <utility>
#include <vector>

module farcaster.provider.hypersnap.profile;

import farcaster.domain.profile;
import farcaster.domain.identity;
import farcaster.provider.hypersnap.codec;
import farcaster.provider.hypersnap.request;

namespace farcaster::provider::hypersnap::profile {

ProfileSource::ProfileSource(transport::http::IHttpTransport& transport, config::Config config)
    : m_transport(transport), m_config(std::move(config))
{
}

foundation::result::Result<domain::profile::Profile>
ProfileSource::get(const domain::identity::Fid fid)
{
    if (auto status = request::validateConfig(m_config); !status) {
        return std::unexpected(std::move(status.error()));
    }
    request::Client requests{m_transport, m_config};
    auto body = requests.getJson(
        "/v2/farcaster/user",
        {{"fid", std::to_string(fid.value)}});
    if (!body) {
        return std::unexpected(std::move(body.error()));
    }
    return codec::decodeUser(*body);
}

}
