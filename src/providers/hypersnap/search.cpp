module;

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <chrono>
#include <optional>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

module farcaster.provider.hypersnap.search;

import farcaster.capability.search;
import farcaster.foundation.error;
import farcaster.provider.hypersnap.codec;
import farcaster.provider.hypersnap.request;

namespace farcaster::provider::hypersnap::search {
namespace {

[[nodiscard]] std::string trim(std::string_view value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](const char character) {
        return std::isspace(static_cast<unsigned char>(character)) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](const char character) {
        return std::isspace(static_cast<unsigned char>(character)) != 0;
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string{first, last};
}

} // namespace

SearchSource::SearchSource(transport::http::IHttpTransport& transport, config::Config config)
    : m_transport(transport), m_config(std::move(config))
{
}

foundation::result::Result<capability::search::SearchPage>
SearchSource::search(const std::string_view query, const std::size_t limit)
{
    if (auto status = request::validateConfig(m_config); !status) {
        return std::unexpected(std::move(status.error()));
    }
    const std::string canonical = trim(query);
    if (canonical.empty() || limit == 0 || limit > 30) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Search query must be non-empty and limit must be between 1 and 30."));
    }

    request::Client requests{m_transport, m_config};
    const std::string limitText = std::to_string(limit);
    capability::search::SearchPage page{};
    std::optional<foundation::error::Error> firstFailure{};

    auto usersBody = requests.getJson(
        "/v2/farcaster/user/search",
        {{"q", canonical}, {"limit", limitText}},
        request::GetOptions{.timeout = std::chrono::milliseconds{4'000}});
    if (usersBody) {
        auto profiles = codec::decodeUsers(*usersBody);
        if (!profiles) {
            firstFailure = std::move(profiles.error());
        } else {
            page.profiles = std::move(*profiles);
        }
    } else {
        firstFailure = std::move(usersBody.error());
    }

    auto channelsBody = requests.getJson(
        "/v2/farcaster/channel/search",
        {{"q", canonical}, {"limit", limitText}},
        request::GetOptions{.timeout = std::chrono::milliseconds{4'000}});
    if (channelsBody) {
        auto channels = codec::decodeSearchChannels(*channelsBody);
        if (!channels) {
            if (!firstFailure) {
                firstFailure = std::move(channels.error());
            }
        } else {
            page.channels = std::move(*channels);
        }
    } else if (!firstFailure) {
        firstFailure = std::move(channelsBody.error());
    }

    if (page.profiles.empty() && page.channels.empty() && firstFailure) {
        return std::unexpected(std::move(*firstFailure));
    }
    return page;
}

} // namespace farcaster::provider::hypersnap::search
