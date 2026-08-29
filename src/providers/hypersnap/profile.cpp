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
constexpr std::string_view bannerUserDataTypeName{"USER_DATA_TYPE_BANNER"};
constexpr std::string_view bannerUserDataType{"11"};
constexpr std::string_view legacyBannerUserDataType{"10"};

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
    // Banner is protocol UserData rather than a field guaranteed by the enriched
    // v2 User response. Prefer the symbolic enum name so the adapter does not
    // depend on historical numeric assignments. Current Farcaster assigns Banner
    // to slot 11; slot 10 is retained only as a legacy compatibility fallback.
    const auto assignBanner = [&](const std::string_view type) {
        if (profile->bannerUrl) {
            return;
        }
        if (auto value = userDataValue(type); value && isRemoteBannerUrl(*value)) {
            profile->bannerUrl = std::move(*value);
        }
    };
    assignBanner(bannerUserDataTypeName);
    assignBanner(bannerUserDataType);
    assignBanner(legacyBannerUserDataType);
    return profile;
}

}
