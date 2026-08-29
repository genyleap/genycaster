module;

#include <boost/json.hpp>

#include <chrono>
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

import farcaster.domain.cast;
import farcaster.domain.identity;
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
        profile.username = domain::identity::Username{std::move(*username)};
    }
    if (auto displayName = optionalString(user, "display_name"); displayName && !displayName->empty()) {
        profile.displayName = domain::identity::DisplayName{std::move(*displayName)};
    }
    profile.avatarUrl = optionalString(user, "pfp_url");
    profile.profileUrl = optionalString(user, "url");
    profile.custodyAddress = optionalString(user, "custody_address");

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
    if (auto followers = optionalUnsigned(user, "follower_count")) {
        profile.followerCount = *followers;
    }
    if (auto following = optionalUnsigned(user, "following_count")) {
        profile.followingCount = *following;
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

[[nodiscard]] foundation::result::Result<MappedCast> mapCast(const JsonObject& castObject)
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
    cast.createdAt = *timestamp;
    if (auto parentHash = optionalString(castObject, "parent_hash"); parentHash && !parentHash->empty()) {
        cast.parent = domain::cast::CastId{std::move(*parentHash)};
    }
    if (auto parentUrl = optionalString(castObject, "parent_url"); parentUrl && !parentUrl->empty()) {
        cast.parentUrl = std::move(*parentUrl);
    }
    if (cast.parent && cast.parentUrl) {
        return std::unexpected(protocolError(
            "Hypersnap cast cannot contain both parent_hash and parent_url."));
    }

    if (const JsonValue* reactionsValue = findValue(castObject, "reactions");
        reactionsValue != nullptr && reactionsValue->is_object()) {
        auto likes = requireUnsigned(reactionsValue->as_object(), "likes_count", "cast.reactions");
        auto recasts = requireUnsigned(reactionsValue->as_object(), "recasts_count", "cast.reactions");
        if (!likes) { return std::unexpected(std::move(likes.error())); }
        if (!recasts) { return std::unexpected(std::move(recasts.error())); }
        cast.engagement.likes = *likes;
        cast.engagement.recasts = *recasts;
    }
    if (const JsonValue* repliesValue = findValue(castObject, "replies");
        repliesValue != nullptr && repliesValue->is_object()) {
        auto replies = requireUnsigned(repliesValue->as_object(), "count", "cast.replies");
        if (!replies) { return std::unexpected(std::move(replies.error())); }
        cast.engagement.replies = *replies;
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
    const JsonValue* castsValue = findValue(root, "casts");
    if (castsValue == nullptr || !castsValue->is_array()) {
        return std::unexpected(protocolError("Hypersnap feed response.casts must be an array."));
    }

    FeedPayload payload{};
    std::unordered_set<std::uint64_t> seenProfiles{};
    payload.casts.reserve(castsValue->as_array().size());
    payload.profiles.reserve(castsValue->as_array().size());
    for (const JsonValue& castValue : castsValue->as_array()) {
        if (!castValue.is_object()) {
            return std::unexpected(protocolError("Hypersnap feed contains a non-object cast."));
        }
        auto mapped = mapCast(castValue.as_object());
        if (!mapped) {
            return std::unexpected(std::move(mapped.error()));
        }
        if (seenProfiles.insert(mapped->author.fid.value).second) {
            payload.profiles.push_back(mapped->author);
        }
        payload.casts.push_back(std::move(mapped->cast));
    }
    auto cursor = readNextCursor(root);
    if (!cursor) {
        return std::unexpected(std::move(cursor.error()));
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

foundation::result::Result<ReactionTargetsPayload> decodeReactionTargets(
    const std::string_view body)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Snapchain reactions response must be a JSON object."));
    }
    const JsonObject& root = document->as_object();
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
