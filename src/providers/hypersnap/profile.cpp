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
constexpr std::string_view usernameUserDataType{"6"};
constexpr std::string_view locationUserDataType{"7"};
constexpr std::string_view bannerUserDataType{"10"};

[[nodiscard]] constexpr bool isRemoteBannerUrl(const std::string_view value) noexcept
{
    return value.starts_with("https://") || value.starts_with("http://") || value.starts_with("ipfs://");
}

[[nodiscard]] bool isType(
    const codec::UserDataEntry& entry,
    const std::string_view numericType,
    const std::string_view symbolicType = {})
{
    return entry.type == numericType || (!symbolicType.empty() && entry.type == symbolicType);
}

[[nodiscard]] std::optional<std::string> findValue(
    const std::vector<codec::UserDataEntry>& entries,
    const std::string_view numericType,
    const std::string_view symbolicType = {})
{
    for (const auto& entry : entries) {
        if (isType(entry, numericType, symbolicType) && !entry.value.empty()) {
            return entry.value;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> findBanner(
    const std::vector<codec::UserDataEntry>& entries)
{
    // Farcaster UserDataType 10 is the canonical banner slot. Keep numeric and
    // symbolic decoding equivalent, but never reinterpret newer UserData fields
    // as media merely because their value happens to look like a URL.
    for (const auto& entry : entries) {
        const bool bannerType = isType(entry, bannerUserDataType, "USER_DATA_TYPE_BANNER");
        if (bannerType && isRemoteBannerUrl(entry.value)) {
            return entry.value;
        }
    }
    return std::nullopt;
}

} // namespace

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

    // The enriched v2 user is the immediate fast path. A single protocol-native
    // UserDataByFid request then fills any missing metadata, including banners.
    // This deliberately avoids the old N sequential requests (one per field),
    // which made profile navigation feel network-bound even when the base user
    // had already arrived.
    auto userDataBody = requests.getJson(
        "/v1/userDataByFid",
        {{"fid", std::to_string(fid.value)}},
        request::GetOptions{.timeout = std::chrono::milliseconds{2'500}});
    if (!userDataBody) {
        return profile;
    }

    auto entries = codec::decodeUserDataEntries(*userDataBody);
    if (!entries) {
        return profile;
    }

    if (!profile->avatarUrl) {
        profile->avatarUrl = findValue(*entries, pfpUserDataType, "USER_DATA_TYPE_PFP");
    }
    if (!profile->displayName) {
        if (auto value = findValue(*entries, displayUserDataType, "USER_DATA_TYPE_DISPLAY")) {
            profile->displayName = domain::identity::DisplayName{std::move(*value)};
        }
    }
    if (profile->bio.empty()) {
        if (auto value = findValue(*entries, bioUserDataType, "USER_DATA_TYPE_BIO")) {
            profile->bio = std::move(*value);
        }
    }
    if (!profile->profileUrl) {
        profile->profileUrl = findValue(*entries, urlUserDataType, "USER_DATA_TYPE_URL");
    }
    if (!profile->username || profile->username->value.starts_with("fid:")) {
        auto username = findValue(*entries, usernameUserDataType, "USER_DATA_TYPE_USERNAME");
        if (!username) {
            username = findValue(*entries, usernameUserDataType, "USER_DATA_TYPE_FNAME");
        }
        if (username && !username->empty()) {
            profile->username = domain::identity::Username{std::move(*username)};
        }
    }
    if (!profile->location) {
        profile->location = findValue(*entries, locationUserDataType, "USER_DATA_TYPE_LOCATION");
    }
    if (!profile->bannerUrl) {
        profile->bannerUrl = findBanner(*entries);
    }
    return profile;
}

} // namespace farcaster::provider::hypersnap::profile
