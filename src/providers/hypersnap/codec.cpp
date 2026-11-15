module;

#include <boost/json.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

module farcaster.provider.hypersnap.codec;

import farcaster.capability.search;
import farcaster.capability.notification;
import farcaster.domain.cast;
import farcaster.domain.channel;
import farcaster.domain.identity;
import farcaster.domain.feed;
import farcaster.domain.profile;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;

namespace farcaster::provider::hypersnap::codec {
namespace {

using JsonObject = boost::json::object;
using JsonValue = boost::json::value;

[[nodiscard]] foundation::error::Error protocolError(std::string message)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::ProtocolFailure,
        std::move(message));
}

[[nodiscard]] foundation::result::Result<JsonValue> parseJson(const std::string_view body)
{
    std::error_code error{};
    JsonValue document = boost::json::parse(body, error);
    if (error) {
        return std::unexpected(protocolError("Hypersnap returned invalid JSON: " + error.message()));
    }
    return document;
}

[[nodiscard]] const JsonValue* findValue(const JsonObject& object, const std::string_view key)
{
    const auto iterator = object.find(key);
    return iterator == object.end() ? nullptr : &iterator->value();
}

[[nodiscard]] foundation::result::Result<std::string> requireString(
    const JsonObject& object,
    const std::string_view key,
    const std::string_view context)
{
    const JsonValue* value = findValue(object, key);
    if (value == nullptr || !value->is_string()) {
        return std::unexpected(protocolError(
            std::string{context} + "." + std::string{key} + " must be a string."));
    }
    return std::string{value->as_string()};
}

[[nodiscard]] foundation::result::Result<std::uint64_t> requireUnsigned(
    const JsonObject& object,
    const std::string_view key,
    const std::string_view context)
{
    const JsonValue* value = findValue(object, key);
    if (value == nullptr) {
        return std::unexpected(protocolError(
            std::string{context} + "." + std::string{key} + " is required."));
    }
    if (value->is_uint64()) {
        return value->as_uint64();
    }
    if (value->is_int64() && value->as_int64() >= 0) {
        return static_cast<std::uint64_t>(value->as_int64());
    }
    // JSON gateways can serialize uint64 FIDs as decimal strings to avoid
    // JavaScript's 53-bit integer precision limit. Reject signs, whitespace,
    // fractions and overflow instead of silently coercing malformed IDs.
    if (value->is_string()) {
        const std::string_view digits{value->as_string().data(), value->as_string().size()};
        if (!digits.empty() && std::ranges::all_of(digits, [](const char digit) {
                return digit >= '0' && digit <= '9';
            })) {
            std::uint64_t parsed = 0;
            const auto [end, error] = std::from_chars(
                digits.data(), digits.data() + digits.size(), parsed);
            if (error == std::errc{} && end == digits.data() + digits.size()) {
                return parsed;
            }
        }
    }
    return std::unexpected(protocolError(
        std::string{context} + "." + std::string{key} + " must be a non-negative integer."));
}

[[nodiscard]] std::optional<std::uint64_t> optionalUnsigned(
    const JsonObject& object,
    const std::string_view key)
{
    const JsonValue* value = findValue(object, key);
    if (value == nullptr || value->is_null()) {
        return std::nullopt;
    }
    if (value->is_uint64()) {
        return value->as_uint64();
    }
    if (value->is_int64() && value->as_int64() >= 0) {
        return static_cast<std::uint64_t>(value->as_int64());
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<double> optionalDouble(
    const JsonObject& object,
    const std::string_view key)
{
    const JsonValue* value = findValue(object, key);
    if (value == nullptr || value->is_null()) {
        return std::nullopt;
    }
    if (value->is_double()) {
        return value->as_double();
    }
    if (value->is_int64()) {
        return static_cast<double>(value->as_int64());
    }
    if (value->is_uint64()) {
        return static_cast<double>(value->as_uint64());
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<bool> optionalBool(
    const JsonObject& object,
    const std::string_view key)
{
    const JsonValue* value = findValue(object, key);
    if (value == nullptr || value->is_null() || !value->is_bool()) {
        return std::nullopt;
    }
    return value->as_bool();
}

[[nodiscard]] std::optional<std::string> optionalString(
    const JsonObject& object,
    const std::string_view key)
{
    const JsonValue* value = findValue(object, key);
    if (value == nullptr || value->is_null() || !value->is_string()) {
        return std::nullopt;
    }
    return std::string{value->as_string()};
}

[[nodiscard]] std::vector<std::string> optionalStringArray(
    const JsonObject& object,
    const std::string_view key)
{
    const JsonValue* value = findValue(object, key);
    if (value == nullptr || value->is_null() || !value->is_array()) {
        return {};
    }

    std::vector<std::string> result{};
    result.reserve(value->as_array().size());
    for (const JsonValue& item : value->as_array()) {
        if (!item.is_string()) {
            continue;
        }
        std::string text{item.as_string()};
        if (!text.empty()) {
            result.push_back(std::move(text));
        }
    }
    return result;
}

void appendUniqueAddress(std::vector<std::string>& addresses, std::string address)
{
    if (address.empty()) {
        return;
    }
    std::string key = address;
    std::ranges::transform(key, key.begin(), [](const char value) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    });
    for (const std::string& existing : addresses) {
        std::string existingKey = existing;
        std::ranges::transform(existingKey, existingKey.begin(), [](const char value) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
        });
        if (existingKey == key) {
            return;
        }
    }
    addresses.push_back(std::move(address));
}

[[nodiscard]] bool readDigits(
    const std::string_view text,
    const std::size_t offset,
    const std::size_t count,
    unsigned& result)
{
    if (offset + count > text.size()) {
        return false;
    }
    result = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const char character = text[offset + index];
        if (character < '0' || character > '9') {
            return false;
        }
        result = result * 10U + static_cast<unsigned>(character - '0');
    }
    return true;
}

[[nodiscard]] foundation::result::Result<foundation::time::Timestamp>
parseTimestamp(const std::string_view text)
{
    if (text.size() < 20 || text[4] != '-' || text[7] != '-' ||
        (text[10] != 'T' && text[10] != 't') || text[13] != ':' || text[16] != ':') {
        return std::unexpected(protocolError("Hypersnap cast timestamp is not valid RFC3339."));
    }

    unsigned yearValue = 0;
    unsigned monthValue = 0;
    unsigned dayValue = 0;
    unsigned hourValue = 0;
    unsigned minuteValue = 0;
    unsigned secondValue = 0;
    if (!readDigits(text, 0, 4, yearValue) || !readDigits(text, 5, 2, monthValue) ||
        !readDigits(text, 8, 2, dayValue) || !readDigits(text, 11, 2, hourValue) ||
        !readDigits(text, 14, 2, minuteValue) || !readDigits(text, 17, 2, secondValue)) {
        return std::unexpected(protocolError("Hypersnap cast timestamp has invalid date/time digits."));
    }

    const std::chrono::year_month_day calendarDate{
        std::chrono::year{static_cast<int>(yearValue)},
        std::chrono::month{monthValue},
        std::chrono::day{dayValue}};
    if (!calendarDate.ok() || hourValue > 23 || minuteValue > 59 || secondValue > 60) {
        return std::unexpected(protocolError("Hypersnap cast timestamp has an invalid calendar value."));
    }

    std::size_t position = 19;
    std::uint32_t millisecondsValue = 0;
    if (position < text.size() && text[position] == '.') {
        ++position;
        std::size_t fractionalDigits = 0;
        while (position < text.size() &&
               std::isdigit(static_cast<unsigned char>(text[position])) != 0) {
            if (fractionalDigits < 3) {
                millisecondsValue = millisecondsValue * 10U +
                    static_cast<std::uint32_t>(text[position] - '0');
            }
            ++fractionalDigits;
            ++position;
        }
        if (fractionalDigits == 0) {
            return std::unexpected(protocolError("Hypersnap timestamp has an empty fractional second."));
        }
        while (fractionalDigits < 3) {
            millisecondsValue *= 10U;
            ++fractionalDigits;
        }
    }

    std::chrono::minutes offset{0};
    if (position >= text.size()) {
        return std::unexpected(protocolError("Hypersnap timestamp is missing a timezone."));
    }
    if (text[position] == 'Z' || text[position] == 'z') {
        ++position;
    } else if (text[position] == '+' || text[position] == '-') {
        const bool isPositive = text[position] == '+';
        ++position;
        unsigned offsetHours = 0;
        unsigned offsetMinutes = 0;
        if (!readDigits(text, position, 2, offsetHours)) {
            return std::unexpected(protocolError("Hypersnap timestamp has an invalid timezone hour."));
        }
        position += 2;
        if (position >= text.size() || text[position] != ':') {
            return std::unexpected(protocolError("Hypersnap timestamp has an invalid timezone separator."));
        }
        ++position;
        if (!readDigits(text, position, 2, offsetMinutes) || offsetHours > 23 || offsetMinutes > 59) {
            return std::unexpected(protocolError("Hypersnap timestamp has an invalid timezone offset."));
        }
        position += 2;
        const auto totalOffset = std::chrono::hours{offsetHours} + std::chrono::minutes{offsetMinutes};
        offset = isPositive
            ? std::chrono::duration_cast<std::chrono::minutes>(totalOffset)
            : -std::chrono::duration_cast<std::chrono::minutes>(totalOffset);
    } else {
        return std::unexpected(protocolError("Hypersnap timestamp has an unsupported timezone."));
    }
    if (position != text.size()) {
        return std::unexpected(protocolError("Hypersnap timestamp has trailing data."));
    }

    const auto localTime = std::chrono::sys_days{calendarDate} +
        std::chrono::hours{hourValue} +
        std::chrono::minutes{minuteValue} +
        std::chrono::seconds{secondValue} +
        std::chrono::milliseconds{millisecondsValue};
    const auto utcTime = localTime - offset;
    return foundation::time::Timestamp{
        std::chrono::duration_cast<std::chrono::milliseconds>(utcTime.time_since_epoch())};
}

[[nodiscard]] foundation::result::Result<domain::profile::Profile>
mapProfile(const JsonObject& user)
{
    auto fid = requireUnsigned(user, "fid", "user");
    if (!fid) {
        return std::unexpected(std::move(fid.error()));
    }

    domain::profile::Profile profile{};
    profile.fid = domain::identity::Fid{*fid};
    if (auto username = optionalString(user, "username"); username && !username->empty()) {
        // HyperSnap's native-registry endpoint uses "fid:<decimal>" as a
        // transport placeholder until a real username is registered. Never
        // promote that placeholder to a user-selected canonical handle.
        const std::string_view handle{*username};
        const bool synthetic = handle.starts_with("fid:") && handle.size() > 4
            && std::ranges::all_of(handle.substr(4), [](const char digit) {
                return digit >= '0' && digit <= '9';
            });
        if (!synthetic) {
            profile.username = domain::identity::Username{std::move(*username)};
        }
    }
    if (auto displayName = optionalString(user, "display_name"); displayName && !displayName->empty()) {
        profile.displayName = domain::identity::DisplayName{std::move(*displayName)};
    }
    profile.avatarUrl = optionalString(user, "pfp_url");
    profile.profileUrl = optionalString(user, "url");
    profile.custodyAddress = optionalString(user, "custody_address");

    if (const JsonValue* verified = findValue(user, "verified_addresses");
        verified != nullptr && verified->is_object()) {
        profile.verifiedEthAddresses = optionalStringArray(verified->as_object(), "eth_addresses");
        profile.verifiedSolAddresses = optionalStringArray(verified->as_object(), "sol_addresses");
    }
    for (std::string verification : optionalStringArray(user, "verifications")) {
        if (verification.size() == 42U && verification.starts_with("0x")) {
            appendUniqueAddress(profile.verifiedEthAddresses, std::move(verification));
        }
    }

    const auto bannerFromObject = [](const JsonObject& object) -> std::optional<std::string> {
        if (auto direct = optionalString(object, "banner_url"); direct && !direct->empty()) {
            return direct;
        }
        const JsonValue* const value = findValue(object, "banner");
        if (value == nullptr) {
            return std::nullopt;
        }
        if (value->is_string()) {
            const std::string banner = value->as_string().c_str();
            return banner.empty() ? std::nullopt : std::optional<std::string>{banner};
        }
        if (value->is_object()) {
            if (auto nested = optionalString(value->as_object(), "url"); nested && !nested->empty()) {
                return nested;
            }
        }
        return std::nullopt;
    };

    profile.bannerUrl = bannerFromObject(user);
    const auto followers = optionalUnsigned(user, "follower_count");
    const auto following = optionalUnsigned(user, "following_count");
    // Absence of a field is not proof of zero. Legacy/partial indexed
    // payloads must not masquerade as authoritative social totals.
    profile.socialCountsKnown = followers.has_value() && following.has_value();
    if (followers) profile.followerCount = *followers;
    if (following) profile.followingCount = *following;

    // Neynar-compatible user payloads expose account quality as either `score`
    // or the legacy experimental.neynar_user_score field. Keep the canonical
    // domain field provider-neutral and accept only the documented [0, 1] range.
    std::optional<double> qualityScore = optionalDouble(user, "score");
    if (!qualityScore) {
        if (const JsonValue* experimental = findValue(user, "experimental");
            experimental != nullptr && experimental->is_object()) {
            qualityScore = optionalDouble(experimental->as_object(), "neynar_user_score");
        }
    }
    if (qualityScore && std::isfinite(*qualityScore) && *qualityScore >= 0.0 && *qualityScore <= 1.0) {
        profile.qualityScore = *qualityScore;
    }

    if (const JsonValue* profileValue = findValue(user, "profile");
        profileValue != nullptr && profileValue->is_object()) {
        const JsonObject& profileObject = profileValue->as_object();
        if (!profile.profileUrl) {
            profile.profileUrl = optionalString(profileObject, "url");
        }
        if (const JsonValue* bioValue = findValue(profileObject, "bio");
            bioValue != nullptr && bioValue->is_object()) {
            if (auto bio = optionalString(bioValue->as_object(), "text")) {
                profile.bio = std::move(*bio);
            }
        }
        if (const JsonValue* locationValue = findValue(profileObject, "location");
            locationValue != nullptr && locationValue->is_object()) {
            if (auto description = optionalString(locationValue->as_object(), "description");
                description && !description->empty()) {
                profile.location = std::move(*description);
            }
        }
        if (!profile.bannerUrl) {
            profile.bannerUrl = bannerFromObject(profileObject);
        }
    }
    return profile;
}

struct MappedCast final {
    domain::cast::Cast cast{};
    domain::profile::Profile author{};
};

// Farcaster CastAdd text omits the display handles: mentions are inserted
// at UTF-8 byte offsets. Neynar may expose a hydrated array of profiles and
// either the canonical mentions_positions offsets or zero-length display
// ranges. Never change the signed protocol text; only build a presentation
// string when the provider supplied an unambiguous username and position.
[[nodiscard]] std::optional<std::string> resolvedMentionText(
    const JsonObject& castObject, const std::string_view original)
{
    const JsonValue* profilesValue = findValue(castObject, "mentioned_profiles");
    const JsonValue* positionsValue = findValue(castObject, "mentions_positions");
    if (profilesValue == nullptr || !profilesValue->is_array()
        || profilesValue->as_array().empty()) {
        // Self-hosted and unhydrated providers may supply canonical CastAdd
        // FIDs/UTF-8 offsets but no Neynar mentioned_profiles. Render an
        // explicitly FID-labelled, navigable marker rather than silently
        // deleting the mention from the user's visible post. Signed text is
        // still untouched and the UI resolves the FID through the active node.
        const JsonValue* fidsValue = findValue(castObject, "mentions");
        if (!fidsValue || !fidsValue->is_array() || !positionsValue
            || !positionsValue->is_array()) return std::nullopt;
        const auto& fids = fidsValue->as_array();
        const auto& positions = positionsValue->as_array();
        if (fids.empty() || fids.size() > 10U || fids.size() != positions.size())
            return std::nullopt;
        struct FidInsert {std::size_t position; std::string handle;};
        std::vector<FidInsert> inserts{};
        inserts.reserve(fids.size());
        for (std::size_t i = 0; i < fids.size(); ++i) {
            const auto readNumber = [](const JsonValue& value) -> std::optional<std::uint64_t> {
                if (value.is_uint64()) return value.as_uint64();
                if (value.is_int64() && value.as_int64() >= 0)
                    return static_cast<std::uint64_t>(value.as_int64());
                return std::nullopt;
            };
            const auto fid = readNumber(fids[i]);
            const auto offset = readNumber(positions[i]);
            if (!fid || *fid == 0 || !offset || *offset > original.size())
                return std::nullopt;
            const auto pos = static_cast<std::size_t>(*offset);
            if (pos < original.size()
                && (static_cast<unsigned char>(original[pos]) & 0xc0U) == 0x80U)
                return std::nullopt;
            inserts.push_back({pos, "@fid:" + std::to_string(*fid)});
        }
        if (!std::is_sorted(inserts.begin(), inserts.end(),
            [](const FidInsert& a, const FidInsert& b) { return a.position < b.position; }))
            return std::nullopt;
        std::string rendered{original};
        for (auto it = inserts.rbegin(); it != inserts.rend(); ++it)
            rendered.insert(it->position, it->handle);
        return rendered;
    }
    const auto& profiles = profilesValue->as_array();
    if (profiles.size() > 10U) return std::nullopt;
    const JsonValue* rangesValue = findValue(castObject, "mentioned_profiles_ranges");
    const bool canonical = positionsValue && positionsValue->is_array()
        && positionsValue->as_array().size() == profiles.size();
    const bool ranges = !canonical && rangesValue && rangesValue->is_array()
        && rangesValue->as_array().size() == profiles.size();
    const auto visibleFallback = [&]() -> std::optional<std::string> {
        std::string display{original};
        bool changed = false;
        for (const auto& profile : profiles) {
            if (!profile.is_object()) continue;
            const auto username = optionalString(profile.as_object(), "username");
            if (!username || username->empty() || username->size() > 80
                || !std::all_of(username->begin(), username->end(), [](unsigned char c) {
                    return std::isalnum(c) || c == '_' || c == '.' || c == '-';
                })) continue;
            const std::string handle = "@" + *username;
            if (display.find(handle) != std::string::npos) continue;
            if (!display.empty() && !std::isspace(static_cast<unsigned char>(display.back())))
                display.push_back(' ');
            display += handle;
            changed = true;
        }
        return changed ? std::optional{std::move(display)} : std::nullopt;
    };
    if (!canonical && !ranges) return visibleFallback();
    // Neynar's hydrated text ranges can be character offsets rather than the
    // canonical Farcaster byte offsets. A byte index falling in a multi-byte
    // code point must be converted, never used to split an emoji. Keep the
    // protocol positions strictly byte-addressed.
    const auto isBoundary = [original](const std::size_t index) {
        return index <= original.size() && (index == original.size()
            || (static_cast<unsigned char>(original[index]) & 0xc0U) != 0x80U);
    };
    const auto characterToByte = [original](std::size_t count, bool utf16)
        -> std::optional<std::size_t> {
        std::size_t index = 0;
        while (count > 0 && index < original.size()) {
            const auto lead = static_cast<unsigned char>(original[index]);
            const std::size_t width = lead < 0x80U ? 1U
                : (lead & 0xe0U) == 0xc0U ? 2U
                : (lead & 0xf0U) == 0xe0U ? 3U
                : (lead & 0xf8U) == 0xf0U ? 4U : 0U;
            if (width == 0 || index + width > original.size()) return std::nullopt;
            const std::size_t units = utf16 && width == 4U ? 2U : 1U;
            if (count < units) return std::nullopt;
            count -= units;
            index += width;
        }
        if (count != 0) return std::nullopt;
        return index;
    };
    struct Insert {std::size_t position; std::string handle;};
    std::vector<Insert> inserts{};
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        if (!profiles[i].is_object()) return std::nullopt;
        const auto username = optionalString(profiles[i].as_object(), "username");
        if (!username || username->empty() || username->size() > 80
            || !std::all_of(username->begin(),username->end(),[](unsigned char c) {
                return std::isalnum(c) || c == '_' || c == '.' || c == '-';
            })) return std::nullopt;
        const auto& entries = canonical ? positionsValue->as_array() : rangesValue->as_array();
        const auto& item = entries[i];
        std::optional<std::uint64_t> offset{};
        if (canonical) {
            if (item.is_uint64()) offset = item.as_uint64();
            else if (item.is_int64() && item.as_int64() >= 0)
                offset = static_cast<std::uint64_t>(item.as_int64());
        } else if (item.is_object()) {
            const auto start = optionalUnsigned(item.as_object(),"start");
            const auto end = optionalUnsigned(item.as_object(),"end");
            if (!start || !end || *start > *end || *end > original.size())
                return std::nullopt;
            if (*start != *end) {
                const auto begin = static_cast<std::size_t>(*start);
                const auto finish = static_cast<std::size_t>(*end);
                const std::string_view span = original.substr(begin,finish-begin);
                if (!span.starts_with("@")) return visibleFallback();
                continue;
            }
            offset = start;
        }
        if (!offset) return std::nullopt;
        std::size_t pos = static_cast<std::size_t>(*offset);
        if (!isBoundary(pos)) {
            if (canonical) return std::nullopt;
            // Prefer character indices for hydrated ranges (code points),
            // then UTF-16 offsets used by some Web clients.
            const auto decoded = characterToByte(pos, false);
            const auto utf16 = characterToByte(pos, true);
            if (decoded && isBoundary(*decoded)) pos = *decoded;
            else if (utf16 && isBoundary(*utf16)) pos = *utf16;
            else return std::nullopt;
        }
        inserts.push_back({pos, "@" + *username});
    }
    if (inserts.empty() || !std::is_sorted(inserts.begin(),inserts.end(),
        [](const Insert& a,const Insert& b){return a.position < b.position;}))
        return std::nullopt;
    std::string rendered{original};
    for (auto it=inserts.rbegin();it!=inserts.rend();++it)
        rendered.insert(it->position,it->handle);
    return rendered;
}


[[nodiscard]] foundation::result::Result<MappedCast> mapCast(
    const JsonObject& castObject,
    const bool retainAuthorProfiles = false)
{
    auto hash = requireString(castObject, "hash", "cast");
    auto text = requireString(castObject, "text", "cast");
    auto timestampText = requireString(castObject, "timestamp", "cast");
    if (!hash) {
        return std::unexpected(std::move(hash.error()));
    }
    if (!text) {
        return std::unexpected(std::move(text.error()));
    }
    if (!timestampText) {
        return std::unexpected(std::move(timestampText.error()));
    }
    auto timestamp = parseTimestamp(*timestampText);
    if (!timestamp) {
        return std::unexpected(std::move(timestamp.error()));
    }

    const JsonValue* authorValue = findValue(castObject, "author");
    if (authorValue == nullptr || !authorValue->is_object()) {
        return std::unexpected(protocolError("cast.author must be a JSON object."));
    }
    auto author = mapProfile(authorValue->as_object());
    if (!author) {
        return std::unexpected(std::move(author.error()));
    }

    domain::cast::Cast cast{};
    cast.id = domain::cast::CastId{std::move(*hash)};
    cast.author = author->fid;
    cast.text = std::move(*text);
    cast.displayText = resolvedMentionText(castObject,cast.text);
    // App identity is optional and may be deliberately withheld by privacy
    // clients. Attribute only explicitly named application metadata.
    if (const JsonValue* app = findValue(castObject, "app");
        app != nullptr && app->is_object()) {
        auto name = optionalString(app->as_object(), "display_name");
        if (!name || name->empty()) name = optionalString(app->as_object(), "name");
        if (name && !name->empty() && name->size() <= 48U) {
            cast.sourceClient = std::move(*name);
        }
    }
    cast.createdAt = *timestamp;
    if (auto parentHash = optionalString(castObject, "parent_hash"); parentHash && !parentHash->empty()) {
        cast.parent = domain::cast::CastId{std::move(*parentHash)};
    }
    if (auto parentUrl = optionalString(castObject, "parent_url"); parentUrl && !parentUrl->empty()) {
        cast.parentUrl = std::move(*parentUrl);
    }
    if (const auto* value = findValue(castObject, "channel"); value && value->is_object()) {
        const auto& channel = value->as_object();
        cast.channelId = optionalString(channel,"id");
        cast.channelName = optionalString(channel,"name");
        cast.channelImageUrl = optionalString(channel,"image_url");
        if (!cast.channelImageUrl || cast.channelImageUrl->empty())
            cast.channelImageUrl = optionalString(channel,"imageUrl");
    }
    // Some Farcaster v2-compatible nodes preserve both the direct parent hash
    // and the channel/root parent URL as contextual metadata. The protocol's
    // canonical parent identity remains the hash when present; retaining the
    // URL is harmless context and must not invalidate an otherwise usable cast.

    if (const JsonValue* reactionsValue = findValue(castObject, "reactions");
        reactionsValue != nullptr && reactionsValue->is_object()) {
        // Engagement enrichment is derived data. A partially-populated index row
        // must not invalidate the canonical cast itself or blank an entire feed.
        cast.engagement.likes = optionalUnsigned(reactionsValue->as_object(), "likes_count").value_or(0);
        cast.engagement.recasts = optionalUnsigned(reactionsValue->as_object(), "recasts_count").value_or(0);
    }
    if (const JsonValue* repliesValue = findValue(castObject, "replies");
        repliesValue != nullptr && repliesValue->is_object()) {
        cast.engagement.replies = optionalUnsigned(repliesValue->as_object(), "count").value_or(0);
    }

    if (const JsonValue* embedsValue = findValue(castObject, "embeds");
        embedsValue != nullptr && embedsValue->is_array()) {
        for (const JsonValue& embedValue : embedsValue->as_array()) {
            if (!embedValue.is_object()) {
                continue;
            }
            const JsonObject& embed = embedValue.as_object();
            if (auto url = optionalString(embed, "url"); url && !url->empty()) {
                cast.embeds.push_back(domain::cast::Embed{
                    .kind = domain::cast::EmbedKind::Url,
                    .value = std::move(*url),
                });
                continue;
            }
            const JsonValue* castIdValue = findValue(embed, "cast_id");
            if (castIdValue != nullptr && castIdValue->is_object()) {
                const JsonObject& castId = castIdValue->as_object();
                auto castHash = requireString(castId, "hash", "cast.embed.cast_id");
                auto castFid = requireUnsigned(castId, "fid", "cast.embed.cast_id");
                if (!castHash) {
                    return std::unexpected(std::move(castHash.error()));
                }
                if (!castFid) {
                    return std::unexpected(std::move(castFid.error()));
                }
                if (castHash->empty()) {
                    return std::unexpected(protocolError(
                        "cast.embed.cast_id.hash must not be empty."));
                }
                cast.embeds.push_back(domain::cast::Embed{
                    .kind = domain::cast::EmbedKind::CastReference,
                    .value = std::move(*castHash),
                    .referencedAuthor = domain::identity::Fid{*castFid},
                });
            }
        }
    }

    if (const JsonValue* repliesValue = findValue(castObject, "direct_replies");
        repliesValue != nullptr && repliesValue->is_array()) {
        const auto& replyValues = repliesValue->as_array();
        cast.directReplies.reserve(replyValues.size());
        for (const JsonValue& replyValue : replyValues) {
            if (!replyValue.is_object()) {
                return std::unexpected(protocolError(
                    "Hypersnap conversation reply is not a cast object."));
            }
            // Never silently lose a reply while presenting the resulting
            // conversation as complete. The caller can retain a usable warm
            // snapshot and report that the network conversation was malformed.
            auto reply = mapCast(replyValue.as_object(), retainAuthorProfiles);
            if (!reply) {
                return std::unexpected(std::move(reply.error()));
            }
            if (retainAuthorProfiles) {
                // The v2 conversation schema embeds the full author object on
                // every nested cast. Preserve it on the canonical snapshot so
                // thread consumers can hydrate reply authors from the payload.
                reply->cast.authorProfile.emplace(std::move(reply->author));
            }
            cast.directReplies.push_back(std::move(reply->cast));
        }
        cast.engagement.replies = std::max<std::uint64_t>(
            cast.engagement.replies, static_cast<std::uint64_t>(replyValues.size()));
    }

    return MappedCast{.cast = std::move(cast), .author = std::move(*author)};
}

[[nodiscard]] foundation::result::Result<std::optional<std::string>> readNextCursor(const JsonObject& root)
{
    const JsonValue* nextValue = findValue(root, "next");
    if (nextValue == nullptr || nextValue->is_null()) {
        return std::optional<std::string>{};
    }
    if (!nextValue->is_object()) {
        return std::unexpected(protocolError("response.next must be a JSON object."));
    }
    const JsonValue* cursorValue = findValue(nextValue->as_object(), "cursor");
    if (cursorValue == nullptr || cursorValue->is_null()) {
        return std::optional<std::string>{};
    }
    if (!cursorValue->is_string()) {
        return std::unexpected(protocolError("response.next.cursor must be a string or null."));
    }
    std::string cursor{cursorValue->as_string()};
    if (cursor.empty()) {
        return std::optional<std::string>{};
    }
    return std::optional<std::string>{std::move(cursor)};
}

}

foundation::result::Result<std::vector<RecentShardHeight>>
decodeRecentShardInfo(const std::string_view body)
{
    auto document = parseJson(body);
    if (!document) { return std::unexpected(std::move(document.error())); }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Snapchain shard information must be an object."));
    }
    const JsonValue* shards = findValue(document->as_object(), "shardInfos");
    if (shards == nullptr || !shards->is_array()) {
        return std::unexpected(protocolError("Snapchain shardInfos array is required."));
    }
    std::vector<RecentShardHeight> result{};
    for (const auto& shard : shards->as_array()) {
        if (!shard.is_object()) { continue; }
        const auto id = optionalUnsigned(shard.as_object(), "shardId");
        const auto height = optionalUnsigned(shard.as_object(), "maxHeight");
        if (id && height && *id != 0U && *height > 0U) {
            result.push_back(RecentShardHeight{.shard = *id, .height = *height});
        }
    }
    std::ranges::sort(result, {}, &RecentShardHeight::shard);
    if (result.size() != 2U || result[0].shard != 1U || result[1].shard != 2U) {
        return std::unexpected(protocolError(
            "Snapchain recent discovery requires both data shards (1, 2)."));
    }
    return result;
}

foundation::result::Result<std::vector<RecentCastEvent>>
decodeRecentCastEvents(const std::string_view body, const std::uint64_t startBlock,
                       const std::uint64_t endBlock, const bool repliesOnly)
{
    auto document = parseJson(body);
    if (!document) { return std::unexpected(std::move(document.error())); }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Snapchain events response must be an object."));
    }
    const JsonValue* events = findValue(document->as_object(), "events");
    if (events == nullptr || !events->is_array()) {
        return std::unexpected(protocolError("Snapchain events array is required."));
    }
    // Snapchain returns a bounded event batch. If the window did not fit, never
    // claim chronological completeness, as that would silently skip new casts.
    if (events->as_array().size() >= 2000U) {
        return std::unexpected(protocolError("Snapchain recent event window exceeded 2000 events."));
    }
    std::vector<RecentCastEvent> result{};
    for (const auto& event : events->as_array()) {
        if (!event.is_object()) { continue; }
        const auto& object = event.as_object();
        const auto block = optionalUnsigned(object, "blockNumber");
        if (!block || *block < startBlock || *block >= endBlock) { continue; }
        if (optionalString(object, "type") != "HUB_EVENT_TYPE_MERGE_MESSAGE") { continue; }
        const JsonValue* merged = findValue(object, "mergeMessageBody");
        if (merged == nullptr || !merged->is_object()) { continue; }
        const JsonValue* message = findValue(merged->as_object(), "message");
        if (message == nullptr || !message->is_object()) { continue; }
        const JsonValue* data = findValue(message->as_object(), "data");
        if (data == nullptr || !data->is_object()) { continue; }
        if (optionalString(data->as_object(), "type") != "MESSAGE_TYPE_CAST_ADD") { continue; }
        const JsonValue* castBody = findValue(data->as_object(), "castAddBody");
        if (castBody == nullptr || !castBody->is_object()) { continue; }
        const JsonValue* parent = findValue(castBody->as_object(), "parentCastId");
        const auto parentHash = parent != nullptr && parent->is_object()
            ? optionalString(parent->as_object(), "hash") : std::nullopt;
        const bool isReply = parentHash && !parentHash->empty();
        if (isReply != repliesOnly) { continue; }
        const auto hash = optionalString(message->as_object(), "hash");
        const auto timestamp = optionalUnsigned(data->as_object(), "timestamp");
        const auto id = optionalUnsigned(object, "id");
        if (!hash || hash->size() != 42U || !hash->starts_with("0x")
            || !timestamp || !id) { continue; }
        result.push_back(RecentCastEvent{
            .hash = *hash, .parentHash = parentHash,
            .protocolTimestamp = *timestamp, .eventId = *id});
    }
    return result;
}

foundation::result::Result<FeedPayload> decodeFeed(const std::string_view body)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Hypersnap feed response must be a JSON object."));
    }
    const JsonObject& root = document->as_object();

    // The documented v2 shape is {casts,next}. During API rollout skew some
    // compatible deployments wrapped that payload in result/data, so accept
    // those envelopes without weakening the cast-level validation contract.
    const JsonObject* feedRoot = &root;
    if (findValue(*feedRoot, "casts") == nullptr) {
        for (const std::string_view envelope : {std::string_view{"result"}, std::string_view{"data"}}) {
            const JsonValue* value = findValue(root, envelope);
            if (value != nullptr && value->is_object() && findValue(value->as_object(), "casts") != nullptr) {
                feedRoot = &value->as_object();
                break;
            }
        }
    }

    const JsonValue* castsValue = findValue(*feedRoot, "casts");
    const JsonValue* itemsValue = findValue(*feedRoot, "items");
    if ((castsValue == nullptr || !castsValue->is_array()) &&
        (itemsValue == nullptr || !itemsValue->is_array())) {
        return std::unexpected(protocolError(
            "Hypersnap feed response must contain a casts or items array."));
    }

    FeedPayload payload{};
    std::unordered_set<std::uint64_t> seenProfiles{};
    std::optional<foundation::error::Error> firstCastError{};

    auto appendMapped = [&](const JsonObject& castObject,
                           const JsonObject* itemObject = nullptr) {
        auto mapped = mapCast(castObject);
        if (!mapped) {
            if (!firstCastError) {
                firstCastError = std::move(mapped.error());
            }
            return;
        }
        if (itemObject != nullptr) {
            mapped->cast.engagement.likes = std::max(
                mapped->cast.engagement.likes,
                optionalUnsigned(*itemObject, "likes").value_or(0));
            mapped->cast.engagement.recasts = std::max(
                mapped->cast.engagement.recasts,
                optionalUnsigned(*itemObject, "recasts").value_or(0));
            mapped->cast.engagement.replies = std::max(
                mapped->cast.engagement.replies,
                optionalUnsigned(*itemObject, "replies").value_or(0));
        }
        if (seenProfiles.insert(mapped->author.fid.value).second) {
            payload.profiles.push_back(mapped->author);
        }
        payload.casts.push_back(std::move(mapped->cast));
    };

    std::size_t sourceCount = 0;
    if (castsValue != nullptr && castsValue->is_array()) {
        sourceCount = castsValue->as_array().size();
        payload.casts.reserve(sourceCount);
        payload.profiles.reserve(sourceCount);
        for (const JsonValue& castValue : castsValue->as_array()) {
            if (!castValue.is_object()) {
                if (!firstCastError) {
                    firstCastError = protocolError("Hypersnap feed contains a non-object cast.");
                }
                continue;
            }
            appendMapped(castValue.as_object());
        }
    } else if (itemsValue != nullptr && itemsValue->is_array()) {
        // Older/internal FeedService payloads expose [{cast,likes,recasts,replies}].
        // Accept the already-v2-shaped cast object when present. Raw protobuf
        // messages remain rejected by mapCast rather than being guessed.
        sourceCount = itemsValue->as_array().size();
        payload.casts.reserve(sourceCount);
        payload.profiles.reserve(sourceCount);
        for (const JsonValue& itemValue : itemsValue->as_array()) {
            if (!itemValue.is_object()) {
                if (!firstCastError) {
                    firstCastError = protocolError("Hypersnap feed contains a non-object item.");
                }
                continue;
            }
            const JsonObject& item = itemValue.as_object();
            const JsonValue* castValue = findValue(item, "cast");
            if (castValue == nullptr || !castValue->is_object()) {
                if (!firstCastError) {
                    firstCastError = protocolError("Hypersnap feed item.cast must be an object.");
                }
                continue;
            }
            appendMapped(castValue->as_object(), &item);
        }
    }

    if (sourceCount != 0 && payload.casts.empty() && firstCastError) {
        return std::unexpected(std::move(*firstCastError));
    }

    auto cursor = readNextCursor(*feedRoot);
    if (!cursor) {
        return std::unexpected(std::move(cursor.error()));
    }
    if (!*cursor) {
        if (auto direct = optionalString(*feedRoot, "next_cursor"); direct && !direct->empty()) {
            *cursor = std::move(*direct);
        } else if (feedRoot != &root) {
            if (auto rootCursor = optionalString(root, "next_cursor"); rootCursor && !rootCursor->empty()) {
                *cursor = std::move(*rootCursor);
            }
        }
    }
    payload.providerCursor = std::move(*cursor);
    return payload;
}

foundation::result::Result<domain::profile::Profile> decodeUser(const std::string_view body)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Hypersnap user response must be a JSON object."));
    }
    const JsonValue* userValue = findValue(document->as_object(), "user");
    if (userValue == nullptr || !userValue->is_object()) {
        return std::unexpected(protocolError("Hypersnap user response.user must be a JSON object."));
    }
    return mapProfile(userValue->as_object());
}

foundation::result::Result<std::vector<domain::profile::Profile>> decodeUsers(
    const std::string_view body)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Hypersnap user-search response must be a JSON object."));
    }
    const JsonValue* usersValue = findValue(document->as_object(), "users");
    if (usersValue == nullptr || !usersValue->is_array()) {
        return std::unexpected(protocolError("Hypersnap user-search response.users must be an array."));
    }

    std::vector<domain::profile::Profile> profiles{};
    profiles.reserve(usersValue->as_array().size());
    for (const JsonValue& userValue : usersValue->as_array()) {
        if (!userValue.is_object()) {
            return std::unexpected(protocolError("Hypersnap user-search response contains a non-object user."));
        }
        auto profile = mapProfile(userValue.as_object());
        if (!profile) {
            return std::unexpected(std::move(profile.error()));
        }
        profiles.push_back(std::move(*profile));
    }
    return profiles;
}

foundation::result::Result<std::vector<capability::search::ChannelSuggestion>>
decodeSearchChannels(const std::string_view body)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Hypersnap channel-search response must be a JSON object."));
    }
    const JsonValue* channelsValue = findValue(document->as_object(), "channels");
    if (channelsValue == nullptr || !channelsValue->is_array()) {
        return std::unexpected(protocolError("Hypersnap channel-search response.channels must be an array."));
    }

    std::vector<capability::search::ChannelSuggestion> channels{};
    channels.reserve(channelsValue->as_array().size());
    for (const JsonValue& channelValue : channelsValue->as_array()) {
        if (!channelValue.is_object()) {
            return std::unexpected(protocolError("Hypersnap channel-search response contains a non-object channel."));
        }
        const JsonObject& channel = channelValue.as_object();
        auto id = requireString(channel, "id", "channel");
        auto name = requireString(channel, "name", "channel");
        if (!id) {
            return std::unexpected(std::move(id.error()));
        }
        if (!name) {
            return std::unexpected(std::move(name.error()));
        }
        capability::search::ChannelSuggestion suggestion{};
        suggestion.id = domain::channel::ChannelId{std::move(*id)};
        suggestion.name = std::move(*name);
        suggestion.parentUrl = optionalString(channel, "parent_url");
        if ((!suggestion.parentUrl || suggestion.parentUrl->empty()))
            suggestion.parentUrl = optionalString(channel, "url");
        suggestion.imageUrl = optionalString(channel, "image_url");
        suggestion.description = optionalString(channel, "description");
        if (auto followers = optionalUnsigned(channel, "follower_count")) {
            suggestion.followerCount = *followers;
        }
        channels.push_back(std::move(suggestion));
    }
    return channels;
}

[[nodiscard]] std::optional<UserDataEntry> decodeUserDataEntry(const JsonValue& messageValue)
{
    if (!messageValue.is_object()) {
        return std::nullopt;
    }
    const JsonValue* dataValue = findValue(messageValue.as_object(), "data");
    if (dataValue == nullptr || !dataValue->is_object()) {
        return std::nullopt;
    }
    const JsonValue* bodyValue = findValue(dataValue->as_object(), "userDataBody");
    if (bodyValue == nullptr || !bodyValue->is_object()) {
        return std::nullopt;
    }
    const JsonObject& bodyObject = bodyValue->as_object();
    const auto value = optionalString(bodyObject, "value");
    if (!value || value->empty()) {
        return std::nullopt;
    }

    std::string type{};
    if (const JsonValue* typeValue = findValue(bodyObject, "type"); typeValue != nullptr) {
        if (typeValue->is_string()) {
            type = std::string{typeValue->as_string()};
        } else if (typeValue->is_uint64()) {
            type = std::to_string(typeValue->as_uint64());
        } else if (typeValue->is_int64() && typeValue->as_int64() >= 0) {
            type = std::to_string(typeValue->as_int64());
        }
    }
    if (type.empty()) {
        return std::nullopt;
    }
    return UserDataEntry{.type = std::move(type), .value = *value};
}

foundation::result::Result<std::string> decodeUserDataValue(const std::string_view body)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Snapchain user data response must be a JSON object."));
    }

    const JsonObject& root = document->as_object();
    const JsonValue* messageValue = &*document;
    if (const JsonValue* messages = findValue(root, "messages");
        messages != nullptr && messages->is_array() && !messages->as_array().empty()) {
        messageValue = &messages->as_array().front();
    }
    if (!messageValue->is_object()) {
        return std::unexpected(protocolError("Snapchain user data message must be an object."));
    }
    const JsonValue* dataValue = findValue(messageValue->as_object(), "data");
    if (dataValue == nullptr || !dataValue->is_object()) {
        return std::unexpected(protocolError("Snapchain user data message.data must be an object."));
    }
    const JsonValue* bodyValue = findValue(dataValue->as_object(), "userDataBody");
    if (bodyValue == nullptr || !bodyValue->is_object()) {
        return std::unexpected(protocolError("Snapchain user data body is missing."));
    }
    return requireString(bodyValue->as_object(), "value", "userDataBody");
}

foundation::result::Result<std::vector<UserDataEntry>> decodeUserDataEntries(
    const std::string_view body)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Snapchain user data response must be a JSON object."));
    }

    std::vector<UserDataEntry> entries{};
    const JsonObject& root = document->as_object();
    if (const JsonValue* messages = findValue(root, "messages"); messages != nullptr) {
        if (!messages->is_array()) {
            return std::unexpected(protocolError("Snapchain user data response.messages must be an array."));
        }
        entries.reserve(messages->as_array().size());
        for (const JsonValue& message : messages->as_array()) {
            if (auto entry = decodeUserDataEntry(message)) {
                entries.push_back(std::move(*entry));
            }
        }
        return entries;
    }

    if (auto entry = decodeUserDataEntry(*document)) {
        entries.push_back(std::move(*entry));
        return entries;
    }

    return std::unexpected(protocolError("Snapchain user data response contains no UserData message."));
}

foundation::result::Result<CastPayload> decodeCastPayload(const std::string_view body)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Hypersnap cast response must be a JSON object."));
    }
    const JsonValue* castValue = findValue(document->as_object(), "cast");
    if (castValue == nullptr || !castValue->is_object()) {
        return std::unexpected(protocolError("Hypersnap cast response.cast must be a JSON object."));
    }
    auto mapped = mapCast(castValue->as_object());
    if (!mapped) {
        return std::unexpected(std::move(mapped.error()));
    }
    return CastPayload{.cast = std::move(mapped->cast), .author = std::move(mapped->author)};
}

foundation::result::Result<domain::cast::Cast> decodeCast(const std::string_view body)
{
    auto payload = decodeCastPayload(body);
    if (!payload) {
        return std::unexpected(std::move(payload.error()));
    }
    return std::move(payload->cast);
}

foundation::result::Result<ConversationPayload> decodeConversationPayload(
    const std::string_view body)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Hypersnap conversation response must be a JSON object."));
    }

    const JsonValue* conversationValue = findValue(document->as_object(), "conversation");
    if (conversationValue == nullptr || !conversationValue->is_object()) {
        return std::unexpected(protocolError(
            "Hypersnap conversation response.conversation must be a JSON object."));
    }
    const JsonValue* castValue = findValue(conversationValue->as_object(), "cast");
    if (castValue == nullptr || !castValue->is_object()) {
        return std::unexpected(protocolError(
            "Hypersnap conversation response.conversation.cast must be a JSON object."));
    }

    // Bound both width and depth before recursive canonical decoding. A
    // four-level provider hint does not limit the number of direct replies,
    // and invalid/oversized trees must not silently disappear from a thread.
    constexpr std::size_t maximumConversationNodes{2'048};
    constexpr std::size_t maximumConversationDepth{32};
    std::vector<std::pair<const JsonValue*, std::size_t>> pending{
        {castValue, 0U}};
    std::size_t nodes{0U};
    while (!pending.empty()) {
        const auto [candidate, depth] = pending.back();
        pending.pop_back();
        if (!candidate->is_object() || depth > maximumConversationDepth
            || ++nodes > maximumConversationNodes) {
            return std::unexpected(protocolError(
                "Hypersnap conversation exceeds the supported node/depth budget."));
        }
        if (const JsonValue* replies = findValue(candidate->as_object(), "direct_replies");
            replies != nullptr) {
            if (!replies->is_array()) {
                return std::unexpected(protocolError(
                    "Hypersnap conversation direct_replies must be an array."));
            }
            for (const auto& child : replies->as_array()) {
                if (child.is_object()) {
                    pending.emplace_back(&child, depth + 1);
                }
            }
        }
    }
    auto mapped = mapCast(castValue->as_object(), true);
    if (!mapped) {
        return std::unexpected(std::move(mapped.error()));
    }

    std::size_t directReplyCount{mapped->cast.directReplies.size()};
    if (const JsonValue* replies = findValue(castValue->as_object(), "direct_replies");
        replies != nullptr) {
        if (!replies->is_array()) {
            return std::unexpected(protocolError(
                "Hypersnap conversation cast.direct_replies must be an array."));
        }
        directReplyCount = replies->as_array().size();
    }
    mapped->cast.engagement.replies = std::max<std::uint64_t>(
        mapped->cast.engagement.replies,
        static_cast<std::uint64_t>(directReplyCount));

    return ConversationPayload{
        .cast = std::move(mapped->cast),
        .author = std::move(mapped->author),
        .directReplyCount = directReplyCount,
    };
}

foundation::result::Result<ReactionTargetsPayload> decodeReactionTargets(
    const std::string_view body)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Hypersnap reactions response must be a JSON object."));
    }
    const JsonObject& root = document->as_object();

    // Preferred Hypersnap v2 API shape.
    if (const JsonValue* reactions = findValue(root, "reactions");
        reactions != nullptr && reactions->is_array()) {
        ReactionTargetsPayload payload{};
        payload.targets.reserve(reactions->as_array().size());
        for (const JsonValue& reactionValue : reactions->as_array()) {
            if (!reactionValue.is_object()) {
                continue;
            }
            const JsonObject& reaction = reactionValue.as_object();
            const JsonValue* castValue = findValue(reaction, "cast");
            if (castValue == nullptr || !castValue->is_object()) {
                continue;
            }
            const JsonObject& cast = castValue->as_object();
            auto targetHash = requireString(cast, "hash", "reaction.cast");
            if (!targetHash || targetHash->empty()) {
                continue;
            }

            domain::identity::Fid targetAuthor{};
            if (const auto directFid = optionalUnsigned(cast, "fid")) {
                targetAuthor = domain::identity::Fid{*directFid};
            } else if (const JsonValue* authorValue = findValue(cast, "author");
                       authorValue != nullptr && authorValue->is_object()) {
                if (const auto authorFid = optionalUnsigned(authorValue->as_object(), "fid")) {
                    targetAuthor = domain::identity::Fid{*authorFid};
                }
            }

            foundation::time::Timestamp reactedAt{};
            if (const auto timestampText = optionalString(reaction, "reaction_timestamp")) {
                if (auto parsed = parseTimestamp(*timestampText); parsed) {
                    reactedAt = *parsed;
                }
            } else if (const auto unixSeconds = optionalUnsigned(reaction, "timestamp")) {
                reactedAt = foundation::time::Timestamp{
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::seconds{*unixSeconds})};
            }
            payload.targets.push_back(ReactionTarget{
                .author = targetAuthor,
                .castId = domain::cast::CastId{std::move(*targetHash)},
                .reactedAt = reactedAt,
            });
        }
        auto cursor = readNextCursor(root);
        if (!cursor) {
            return std::unexpected(std::move(cursor.error()));
        }
        payload.providerCursor = std::move(*cursor);
        return payload;
    }

    // Legacy Snapchain-compatible shape retained only for older self-hosted
    // nodes. New reads use the documented /v2/farcaster/reaction/* endpoints.
    const JsonValue* messages = findValue(root, "messages");
    if (messages == nullptr || !messages->is_array()) {
        return std::unexpected(protocolError("Snapchain reactions response.messages must be an array."));
    }

    ReactionTargetsPayload payload{};
    payload.targets.reserve(messages->as_array().size());
    constexpr std::uint64_t farcasterEpochUnixSeconds = 1'609'459'200ULL;
    for (const JsonValue& messageValue : messages->as_array()) {
        if (!messageValue.is_object()) {
            return std::unexpected(protocolError("Snapchain reactions response contains a non-object message."));
        }
        const JsonValue* dataValue = findValue(messageValue.as_object(), "data");
        if (dataValue == nullptr || !dataValue->is_object()) {
            return std::unexpected(protocolError("Snapchain reaction message.data must be an object."));
        }
        const JsonObject& data = dataValue->as_object();
        auto timestamp = requireUnsigned(data, "timestamp", "reaction.data");
        if (!timestamp) {
            return std::unexpected(std::move(timestamp.error()));
        }
        if (*timestamp > std::numeric_limits<std::uint32_t>::max()) {
            return std::unexpected(protocolError("reaction.data.timestamp exceeds uint32."));
        }
        const JsonValue* bodyValue = findValue(data, "reactionBody");
        if (bodyValue == nullptr || !bodyValue->is_object()) {
            return std::unexpected(protocolError("Snapchain reaction message.reactionBody must be an object."));
        }
        const JsonValue* targetValue = findValue(bodyValue->as_object(), "targetCastId");
        if (targetValue == nullptr || !targetValue->is_object()) {
            continue;
        }
        auto targetFid = requireUnsigned(targetValue->as_object(), "fid", "reaction.targetCastId");
        auto targetHash = requireString(targetValue->as_object(), "hash", "reaction.targetCastId");
        if (!targetFid) { return std::unexpected(std::move(targetFid.error())); }
        if (!targetHash) { return std::unexpected(std::move(targetHash.error())); }
        const auto unixSeconds = farcasterEpochUnixSeconds + *timestamp;
        payload.targets.push_back(ReactionTarget{
            .author = domain::identity::Fid{*targetFid},
            .castId = domain::cast::CastId{std::move(*targetHash)},
            .reactedAt = foundation::time::Timestamp{std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::seconds{unixSeconds})},
        });
    }
    if (const JsonValue* token = findValue(root, "nextPageToken");
        token != nullptr && token->is_string() && !token->as_string().empty()) {
        payload.providerCursor = std::string{token->as_string()};
    }
    return payload;
}


foundation::result::Result<domain::feed::ViewerReactionSnapshot> decodeCastInteractions(
    const std::string_view body,
    const domain::identity::Fid viewer)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object() || viewer.value == 0) {
        return std::unexpected(protocolError("Hypersnap batch cast-interactions response is invalid."));
    }
    const JsonObject& root = document->as_object();
    const std::string key = std::to_string(viewer.value);
    const JsonValue* interactions = findValue(root, key);
    if (interactions == nullptr || !interactions->is_array()) {
        return std::unexpected(protocolError(
            "Hypersnap batch cast-interactions response is missing the viewer entry."));
    }

    const auto& entries = interactions->as_array();
    domain::feed::ViewerReactionSnapshot snapshot{};
    // A compliant implementation returns one per-cast entry carrying at least
    // the target hash (liked/recasted may be false). Some compatible deployments
    // instead report authored-cast interaction metadata (mentions, parent_fid,
    // timestamp) with no hash at all. Silently treating that incompatible shape
    // as "the viewer reacted to nothing" would blank every reaction state, so
    // reject it and let the caller fall back to the canonical /reaction/user
    // source of truth.
    bool sawHashEntry{false};
    for (const JsonValue& value : entries) {
        if (!value.is_object()) {
            continue;
        }
        const JsonObject& interaction = value.as_object();
        const auto hash = optionalString(interaction, "hash");
        if (!hash || hash->empty()) {
            continue;
        }
        sawHashEntry = true;
        // Only a per-target record is evidence that an absent reaction is
        // actually false. The provider may omit unrelated account casts.
        snapshot.examined.push_back(domain::cast::CastId{*hash});
        snapshot.likeExamined.push_back(domain::cast::CastId{*hash});
        snapshot.recastExamined.push_back(domain::cast::CastId{*hash});
        if (optionalBool(interaction, "liked").value_or(false)) {
            snapshot.liked.push_back(domain::cast::CastId{*hash});
        }
        if (optionalBool(interaction, "recasted").value_or(false)) {
            snapshot.recasted.push_back(domain::cast::CastId{*hash});
        }
    }
    if (!entries.empty() && !sawHashEntry) {
        return std::unexpected(protocolError(
            "Hypersnap batch cast-interactions response does not match the "
            "documented per-cast interactions shape (missing hash)."));
    }
    return snapshot;
}

foundation::result::Result<NotificationPayload> decodeNotifications(const std::string_view body)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Hypersnap notifications response must be a JSON object."));
    }
    const JsonObject& root = document->as_object();
    const JsonValue* notifications = findValue(root, "notifications");
    if (notifications == nullptr || !notifications->is_array()) {
        return std::unexpected(protocolError("Hypersnap notifications response.notifications must be an array."));
    }

    NotificationPayload payload{};
    payload.items.reserve(notifications->as_array().size());
    for (const JsonValue& value : notifications->as_array()) {
        if (!value.is_object()) {
            continue;
        }
        const JsonObject& object = value.as_object();
        const std::string type = optionalString(object, "type").value_or("unknown");

        capability::notification::NotificationKind kind = capability::notification::NotificationKind::Unknown;
        if (type == "cast-mention" || type == "mention" || type == "mentions") {
            kind = capability::notification::NotificationKind::Mention;
        } else if (type == "cast-reply" || type == "reply" || type == "replies") {
            kind = capability::notification::NotificationKind::Reply;
        } else if (type == "recast" || type == "recasts" || type == "cast-recast") {
            kind = capability::notification::NotificationKind::Recast;
        } else if (type == "quote" || type == "quotes" || type == "cast-quote"
                   || type == "cast-quoted" || type == "quote-cast") {
            kind = capability::notification::NotificationKind::Quote;
        } else if (type == "reaction" || type == "like" || type == "likes") {
            kind = capability::notification::NotificationKind::Reaction;
        } else if (type == "follow" || type == "follows") {
            kind = capability::notification::NotificationKind::Follow;
        }

        foundation::time::Timestamp observedAt{};
        std::uint64_t unixSeconds{0};
        if (const auto timestamp = optionalUnsigned(object, "most_recent_timestamp")) {
            unixSeconds = *timestamp;
            observedAt = foundation::time::Timestamp{
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds{*timestamp})};
        } else if (const auto timestampText = optionalString(object, "timestamp")) {
            if (auto parsed = parseTimestamp(*timestampText); parsed) {
                observedAt = *parsed;
                unixSeconds = static_cast<std::uint64_t>(
                    std::max<std::int64_t>(0, observedAt.time_since_epoch().count()) / 1000);
            }
        } else if (const auto numericTimestamp = optionalUnsigned(object, "timestamp")) {
            unixSeconds = *numericTimestamp;
            observedAt = foundation::time::Timestamp{
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds{*numericTimestamp})};
        }

        capability::notification::NotificationItem item{
            .kind = kind,
            .observedAt = observedAt,
        };

        const auto appendActor = [&](const JsonValue& actorValue) {
            const JsonObject* profileObject = nullptr;
            if (actorValue.is_object()) {
                const JsonObject& candidate = actorValue.as_object();
                if (const JsonValue* user = findValue(candidate, "user"); user != nullptr && user->is_object()) {
                    profileObject = &user->as_object();
                } else if (findValue(candidate, "fid") != nullptr) {
                    profileObject = &candidate;
                }
            }
            if (profileObject == nullptr) {
                return;
            }
            auto profile = mapProfile(*profileObject);
            if (!profile) {
                return;
            }
            const auto duplicate = std::ranges::any_of(item.actors, [&](const auto& actor) {
                return actor.fid.value == profile->fid.value;
            });
            if (!duplicate) {
                item.actors.push_back(std::move(*profile));
            }
        };

        // Current Hypersnap nodes emit a compact {type, cast, user, timestamp}
        // notification shape while older/documented compatibility responses may
        // provide aggregated reactions/follows and most_recent_timestamp. Support
        // both without leaking either provider DTO into the capability model.
        if (const JsonValue* user = findValue(object, "user"); user != nullptr) {
            appendActor(*user);
        }

        const JsonValue* castValue = findValue(object, "cast");
        if (castValue != nullptr && castValue->is_object()) {
            const JsonObject& castObject = castValue->as_object();
            auto mapped = mapCast(castObject);
            // Some notification endpoints send a *partial* mentioned/replied
            // cast: hash and text are present, while author/timestamp live on
            // the notification envelope. Without this enrichment mapCast
            // rejects the cast and the UI silently loses the @mention text.
            // Only fill fields actually supplied by the provider, never
            // synthesize a hash or a message body.
            if (!mapped && (kind == capability::notification::NotificationKind::Mention
                            || kind == capability::notification::NotificationKind::Reply
                            || kind == capability::notification::NotificationKind::Quote)) {
                JsonObject enriched = castObject;
                if (findValue(enriched, "author") == nullptr) {
                    if (const JsonValue* actor = findValue(object, "user");
                        actor != nullptr && actor->is_object())
                        enriched["author"] = *actor;
                }
                if (findValue(enriched, "timestamp") == nullptr) {
                    if (const JsonValue* time = findValue(object, "timestamp");
                        time != nullptr && time->is_string())
                        enriched["timestamp"] = *time;
                }
                if (findValue(enriched, "text") == nullptr) {
                    if (const auto notificationText = optionalString(object, "text"))
                        enriched["text"] = *notificationText;
                }
                mapped = mapCast(enriched);
            }
            if (mapped) {
                item.cast = std::move(mapped->cast);
                if (item.actors.empty() &&
                    (kind == capability::notification::NotificationKind::Reply ||
                     kind == capability::notification::NotificationKind::Mention ||
                     kind == capability::notification::NotificationKind::Quote)) {
                    item.actors.push_back(std::move(mapped->author));
                }
            }
        }

        // Reply/mention context: the cast this notification answers and its
        // author. Current nodes emit a compact {type, cast, user, timestamp}
        // shape whose cast may carry a Neynar-compatible parent_author object;
        // richer responses may add an explicit notification-level parent_author
        // and parent_cast. Capture whatever is present so the UI can render the
        // conversation hierarchy without leaking provider DTOs. Absent fields
        // leave the notification fully usable without this enrichment.
        if (kind == capability::notification::NotificationKind::Reply ||
            kind == capability::notification::NotificationKind::Mention ||
            kind == capability::notification::NotificationKind::Quote) {
            const auto mapParentAuthor = [&](const JsonObject& profileObject) {
                auto profile = mapProfile(profileObject);
                if (profile && !item.parentAuthor) {
                    item.parentAuthor = std::move(*profile);
                }
            };
            if (castValue != nullptr && castValue->is_object()) {
                if (const JsonValue* parentAuthor = findValue(castValue->as_object(), "parent_author");
                    parentAuthor != nullptr && parentAuthor->is_object()) {
                    mapParentAuthor(parentAuthor->as_object());
                }
            }
            if (const JsonValue* parentAuthor = findValue(object, "parent_author");
                parentAuthor != nullptr && parentAuthor->is_object()) {
                mapParentAuthor(parentAuthor->as_object());
            }
            if (const JsonValue* parentCast = findValue(object, "parent_cast");
                parentCast != nullptr && parentCast->is_object()) {
                if (auto parent = mapCast(parentCast->as_object()); parent) {
                    item.parentCast = std::move(parent->cast);
                    if (!item.parentAuthor) {
                        item.parentAuthor = std::move(parent->author);
                    }
                }
            }
        }

        std::size_t aggregateCount = item.actors.empty() ? 0U : item.actors.size();
        for (const std::string_view key : {std::string_view{"reactions"}, std::string_view{"follows"}}) {
            if (const JsonValue* actors = findValue(object, key); actors != nullptr && actors->is_array()) {
                aggregateCount = std::max(aggregateCount, actors->as_array().size());
                for (const JsonValue& actor : actors->as_array()) {
                    appendActor(actor);
                }
            }
        }
        aggregateCount = std::max(aggregateCount, item.actors.size());
        item.aggregateCount = std::max<std::size_t>(1U, aggregateCount);

        const std::string castKey = item.cast ? item.cast->id.value : std::string{};
        const std::string actorKey = item.actors.empty()
            ? std::string{}
            : std::to_string(item.actors.front().fid.value);
        item.stableId = type + ":" + std::to_string(unixSeconds) + ":" + castKey + ":" + actorKey;
        payload.items.push_back(std::move(item));
    }

    auto cursor = readNextCursor(root);
    if (!cursor) {
        return std::unexpected(std::move(cursor.error()));
    }
    payload.providerCursor = std::move(*cursor);
    return payload;
}

foundation::result::Result<SocialPayload> decodeSocial(const std::string_view body)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Hypersnap social response must be a JSON object."));
    }
    const JsonObject& root = document->as_object();
    const JsonValue* usersValue = findValue(root, "users");
    if (usersValue == nullptr || !usersValue->is_array()) {
        return std::unexpected(protocolError("Hypersnap social response.users must be an array."));
    }

    SocialPayload payload{};
    payload.profiles.reserve(usersValue->as_array().size());
    for (const JsonValue& userValue : usersValue->as_array()) {
        if (!userValue.is_object()) {
            return std::unexpected(protocolError("Hypersnap social response contains a non-object user."));
        }
        auto profile = mapProfile(userValue.as_object());
        if (!profile) {
            return std::unexpected(std::move(profile.error()));
        }
        payload.profiles.push_back(std::move(*profile));
    }
    auto cursor = readNextCursor(root);
    if (!cursor) {
        return std::unexpected(std::move(cursor.error()));
    }
    payload.providerCursor = std::move(*cursor);
    return payload;
}

}
