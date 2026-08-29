module;

#include <chrono>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.provider.hypersnap.profile;

import farcaster.domain.profile;
import farcaster.domain.identity;
import farcaster.provider.hypersnap.codec;
import farcaster.provider.hypersnap.request;

namespace farcaster::provider::hypersnap::profile {
namespace {

constexpr std::string_view pfpUserDataType{"1"};
constexpr std::string_view displayUserDataType{"2"};
constexpr std::string_view bioUserDataType{"3"};
constexpr std::string_view urlUserDataType{"5"};
constexpr std::string_view locationUserDataType{"7"};
constexpr std::string_view bannerUserDataType{"10"};
constexpr std::string_view compatibilityBannerUserDataType{"11"};

[[nodiscard]] constexpr bool isRemoteBannerUrl(const std::string_view value) noexcept
{
    return value.starts_with("https://") || value.starts_with("http://") || value.starts_with("ipfs://");
}

}

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
    auto profile = codec::decodeUser(*body);
    if (!profile) {
        return std::unexpected(std::move(profile.error()));
    }

    // The enriched v2 User response is the fast path. Protocol-native UserData
    // remains the canonical fallback for fields that a node has not hydrated.
    // Optional enrichment failures never make the base profile unreadable.
    const auto userDataValue = [&](const std::string_view type) -> std::optional<std::string> {
        auto userDataBody = requests.getJson(
            "/v1/userDataByFid",
            {{"fid", std::to_string(fid.value)}, {"user_data_type", std::string{type}}},
            request::GetOptions{.timeout = std::chrono::milliseconds{3'000}});
        if (!userDataBody) {
            return std::nullopt;
        }
        auto value = codec::decodeUserDataValue(*userDataBody);
        if (!value || value->empty()) {
            return std::nullopt;
        }
        return std::move(*value);
    };

    if (!profile->avatarUrl) {
        profile->avatarUrl = userDataValue(pfpUserDataType);
    }
    if (!profile->displayName) {
        if (auto value = userDataValue(displayUserDataType)) {
            profile->displayName = domain::identity::DisplayName{std::move(*value)};
        }
    }
    if (profile->bio.empty()) {
        if (auto value = userDataValue(bioUserDataType)) {
            profile->bio = std::move(*value);
        }
    }
    if (!profile->profileUrl) {
        profile->profileUrl = userDataValue(urlUserDataType);
    }
    if (!profile->location) {
        profile->location = userDataValue(locationUserDataType);
    }
    if (!profile->bannerUrl) {
        profile->bannerUrl = userDataValue(bannerUserDataType);
    }
    // Some newer protocol snapshots assign Banner to slot 11, while deployed
    // Snapchain-compatible APIs still expose Banner as slot 10. Slot 11 is also
    // used for the primary Ethereum address in deployed schemas, so never accept
    // it blindly as an image URL.
    if (!profile->bannerUrl) {
        if (auto compatibility = userDataValue(compatibilityBannerUserDataType);
            compatibility && isRemoteBannerUrl(*compatibility)) {
            profile->bannerUrl = std::move(*compatibility);
        }
    }
    return profile;
}

}
