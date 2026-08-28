module;

#include <boost/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

module farcaster.provider.snapchain.signer;

import farcaster.capability.signer;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.protocol.message;
import farcaster.transport.http;

namespace farcaster::provider::snapchain::signer {
namespace {

using JsonObject = boost::json::object;
using JsonValue = boost::json::value;

inline constexpr std::size_t publicKeyHexLength = 64U;
inline constexpr std::size_t maximumPages = 64U;
inline constexpr std::uint32_t requestedPageSize = 1'000U;

[[nodiscard]] foundation::error::Error makeError(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] foundation::error::Error protocolError(std::string message)
{
    return makeError(foundation::error::ErrorCode::ProtocolFailure, std::move(message));
}

[[nodiscard]] std::string trimTrailingSlashes(std::string value)
{
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

[[nodiscard]] bool isUnreserved(const unsigned char character)
{
    return std::isalnum(character) != 0 || character == '-' || character == '_' ||
        character == '.' || character == '~';
}

[[nodiscard]] std::string percentEncode(const std::string_view value)
{
    constexpr std::array<char, 16> hexadecimal{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    std::string encoded{};
    encoded.reserve(value.size());
    for (const char rawCharacter : value) {
        const auto character = static_cast<unsigned char>(rawCharacter);
        if (isUnreserved(character)) {
            encoded.push_back(rawCharacter);
            continue;
        }
        encoded.push_back('%');
        encoded.push_back(hexadecimal[(character >> 4U) & 0x0FU]);
        encoded.push_back(hexadecimal[character & 0x0FU]);
    }
    return encoded;
}

[[nodiscard]] foundation::result::Status validateConfig(const Config& config)
{
    if (config.baseUrl.empty()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Snapchain base URL must not be empty."));
    }
    if (!config.baseUrl.starts_with("https://") && !config.baseUrl.starts_with("http://")) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Snapchain base URL must use http or https."));
    }
    if (config.baseUrl.find_first_of("?#\r\n") != std::string::npos) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Snapchain base URL must be an origin/root URL without query, fragment, or control characters."));
    }
    if (config.timeout <= std::chrono::milliseconds::zero()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Snapchain request timeout must be positive."));
    }
    if (config.maxBodyBytes == 0U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Snapchain response-size limit must be positive."));
    }
    return {};
}

[[nodiscard]] bool isHexDigit(const char character)
{
    return (character >= '0' && character <= '9') ||
        (character >= 'a' && character <= 'f') ||
        (character >= 'A' && character <= 'F');
}

[[nodiscard]] foundation::result::Result<std::string> normalizePublicKeyHex(
    const std::string_view value,
    const std::string_view context)
{
    std::string_view hex = value;
    if (hex.starts_with("0x") || hex.starts_with("0X")) {
        hex.remove_prefix(2);
    }
    if (hex.size() != publicKeyHexLength ||
        !std::all_of(hex.begin(), hex.end(), isHexDigit)) {
        return std::unexpected(protocolError(
            std::string{context} + " must be a 32-byte hexadecimal Ed25519 public key."));
    }

    std::string normalized{hex};
    std::ranges::transform(normalized, normalized.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return normalized;
}

[[nodiscard]] const JsonValue* findValue(const JsonObject& object, const std::string_view key)
{
    const auto iterator = object.find(key);
    return iterator == object.end() ? nullptr : &iterator->value();
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

[[nodiscard]] foundation::result::Result<std::uint32_t> requireUint32(
    const JsonObject& object,
    const std::string_view key,
    const std::string_view context)
{
    auto value = requireUnsigned(object, key, context);
    if (!value) {
        return std::unexpected(std::move(value.error()));
    }
    if (*value > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(protocolError(
            std::string{context} + "." + std::string{key} + " exceeds uint32."));
    }
    return static_cast<std::uint32_t>(*value);
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

[[nodiscard]] foundation::result::Result<std::optional<std::uint64_t>> optionalUnsigned(
    const JsonObject& object,
    const std::string_view key,
    const std::string_view context)
{
    const JsonValue* value = findValue(object, key);
    if (value == nullptr || value->is_null()) {
        return std::optional<std::uint64_t>{};
    }
    auto parsed = requireUnsigned(object, key, context);
    if (!parsed) {
        return std::unexpected(std::move(parsed.error()));
    }
    return std::optional<std::uint64_t>{*parsed};
}

[[nodiscard]] foundation::result::Result<std::optional<std::string>> optionalString(
    const JsonObject& object,
    const std::string_view key,
    const std::string_view context)
{
    const JsonValue* value = findValue(object, key);
    if (value == nullptr || value->is_null()) {
        return std::optional<std::string>{};
    }
    if (!value->is_string()) {
        return std::unexpected(protocolError(
            std::string{context} + "." + std::string{key} + " must be a string when present."));
    }
    return std::optional<std::string>{std::string{value->as_string()}};
}

[[nodiscard]] foundation::result::Result<JsonValue> parseJson(const std::string_view body)
{
    std::error_code error{};
    JsonValue document = boost::json::parse(body, error);
    if (error) {
        return std::unexpected(protocolError("Snapchain returned invalid JSON: " + error.message()));
    }
    return document;
}

[[nodiscard]] foundation::time::Timestamp unixSecondsToTimestamp(const std::uint64_t seconds)
{
    return foundation::time::Timestamp{
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds{seconds})};
}

[[nodiscard]] foundation::result::Result<domain::signer::Scope> mapScope(
    const std::string_view value)
{
    using domain::signer::Scope;
    if (value == "MESSAGE_TYPE_CAST_ADD") { return Scope::CastAdd; }
    if (value == "MESSAGE_TYPE_CAST_REMOVE") { return Scope::CastRemove; }
    if (value == "MESSAGE_TYPE_REACTION_ADD") { return Scope::ReactionAdd; }
    if (value == "MESSAGE_TYPE_REACTION_REMOVE") { return Scope::ReactionRemove; }
    if (value == "MESSAGE_TYPE_LINK_ADD") { return Scope::LinkAdd; }
    if (value == "MESSAGE_TYPE_LINK_REMOVE") { return Scope::LinkRemove; }
    if (value == "MESSAGE_TYPE_VERIFICATION_ADD_ETH_ADDRESS") { return Scope::VerificationAddEthAddress; }
    if (value == "MESSAGE_TYPE_VERIFICATION_REMOVE") { return Scope::VerificationRemove; }
    if (value == "MESSAGE_TYPE_USER_DATA_ADD") { return Scope::UserDataAdd; }
    if (value == "MESSAGE_TYPE_USERNAME_PROOF") { return Scope::UsernameProof; }
    if (value == "MESSAGE_TYPE_FRAME_ACTION") { return Scope::FrameAction; }
    if (value == "MESSAGE_TYPE_LINK_COMPACT_STATE") { return Scope::LinkCompactState; }
    if (value == "MESSAGE_TYPE_LEND_STORAGE") { return Scope::LendStorage; }

    return std::unexpected(protocolError(
        "Snapchain signer response contains an unsupported scope: " + std::string{value} + "."));
}

[[nodiscard]] foundation::result::Result<std::vector<domain::signer::Scope>> mapScopes(
    const JsonObject& object)
{
    const JsonValue* value = findValue(object, "scopes");
    if (value == nullptr || !value->is_array()) {
        return std::unexpected(protocolError("signer.scopes must be an array."));
    }
    std::vector<domain::signer::Scope> scopes{};
    scopes.reserve(value->as_array().size());
    for (const JsonValue& item : value->as_array()) {
        if (!item.is_string()) {
            return std::unexpected(protocolError("signer.scopes entries must be strings."));
        }
        auto mapped = mapScope(item.as_string());
        if (!mapped) {
            return std::unexpected(std::move(mapped.error()));
        }
        if (std::ranges::find(scopes, *mapped) != scopes.end()) {
            return std::unexpected(protocolError("Snapchain signer response contains duplicate scopes."));
        }
        scopes.push_back(*mapped);
    }
    return scopes;
}

[[nodiscard]] foundation::result::Result<domain::signer::Registration> mapRegistration(
    const JsonObject& object,
    const domain::identity::Fid expectedFid,
    const std::string_view normalizedKey)
{
    auto source = requireString(object, "source", "signer");
    auto key = requireString(object, "key", "signer");
    auto keyType = requireUint32(object, "keyType", "signer");
    auto fid = requireUnsigned(object, "fid", "signer");
    if (!source) { return std::unexpected(std::move(source.error())); }
    if (!key) { return std::unexpected(std::move(key.error())); }
    if (!keyType) { return std::unexpected(std::move(keyType.error())); }
    if (!fid) { return std::unexpected(std::move(fid.error())); }

    auto responseKey = normalizePublicKeyHex(*key, "signer.key");
    if (!responseKey) {
        return std::unexpected(std::move(responseKey.error()));
    }
    if (*responseKey != normalizedKey) {
        return std::unexpected(protocolError("Internal signer mapper received a non-matching key."));
    }
    if (*fid != expectedFid.value) {
        return std::unexpected(protocolError("Snapchain signer response contains a mismatched FID."));
    }
    if (*keyType != 1U) {
        return std::unexpected(protocolError("Snapchain signer response uses an unsupported key type."));
    }

    domain::signer::Registration registration{
        .fid = expectedFid,
        .publicKey = domain::signer::PublicKey{*responseKey},
        .verifiedAt = foundation::time::now(),
    };

    if (*source == "SIGNER_SOURCE_ONCHAIN") {
        registration.kind = domain::signer::RegistrationKind::LegacyOnchain;
        registration.state = domain::signer::RegistrationState::Active;
        return registration;
    }
    if (*source != "SIGNER_SOURCE_OFFCHAIN") {
        return std::unexpected(protocolError("Snapchain signer response uses an unsupported source."));
    }

    auto ttl = requireUint32(object, "ttl", "signer");
    auto lastUsedAt = optionalUnsigned(object, "lastUsedAt", "signer");
    auto expiresAt = optionalUnsigned(object, "expiresAt", "signer");
    auto requestFid = optionalUnsigned(object, "requestFid", "signer");
    auto scopes = mapScopes(object);
    if (!ttl) { return std::unexpected(std::move(ttl.error())); }
    if (!lastUsedAt) { return std::unexpected(std::move(lastUsedAt.error())); }
    if (!expiresAt) { return std::unexpected(std::move(expiresAt.error())); }
    if (!requestFid) { return std::unexpected(std::move(requestFid.error())); }
    if (!scopes) { return std::unexpected(std::move(scopes.error())); }

    if (*ttl == 0U || *ttl > domain::signer::MaxGaslessTtlSeconds) {
        return std::unexpected(protocolError("Snapchain gasless signer TTL is outside the supported protocol range."));
    }
    if (!*lastUsedAt) {
        return std::unexpected(protocolError("Snapchain gasless signer is missing lastUsedAt."));
    }
    if (!*requestFid || **requestFid == 0U) {
        return std::unexpected(protocolError("Snapchain gasless signer is missing requestFid."));
    }
    if (scopes->empty()) {
        return std::unexpected(protocolError("Snapchain gasless signer has an empty scope set."));
    }

    const std::uint64_t computedExpiry = **lastUsedAt + static_cast<std::uint64_t>(*ttl);
    if (*expiresAt && **expiresAt != computedExpiry) {
        return std::unexpected(protocolError("Snapchain gasless signer expiry is inconsistent with lastUsedAt + ttl."));
    }

    registration.kind = domain::signer::RegistrationKind::Gasless;
    registration.ttlSeconds = *ttl;
    registration.lastUsedAt = unixSecondsToTimestamp(**lastUsedAt);
    registration.requestFid = domain::identity::Fid{**requestFid};
    registration.scopes = std::move(*scopes);
    registration.state = domain::signer::isExpired(registration, registration.verifiedAt)
        ? domain::signer::RegistrationState::Expired
        : domain::signer::RegistrationState::Active;
    return registration;
}

[[nodiscard]] std::string responseExcerpt(const std::string_view body)
{
    constexpr std::size_t maxLength{320U};
    std::string excerpt{};
    excerpt.reserve(std::min(body.size(), maxLength));
    bool previousWhitespace{false};
    for (const char rawCharacter : body) {
        if (excerpt.size() >= maxLength) {
            break;
        }
        const auto character = static_cast<unsigned char>(rawCharacter);
        const bool whitespace = std::isspace(character) != 0;
        if (whitespace) {
            if (!excerpt.empty() && !previousWhitespace) {
                excerpt.push_back(' ');
            }
            previousWhitespace = true;
            continue;
        }
        if (std::iscntrl(character) != 0) {
            continue;
        }
        excerpt.push_back(rawCharacter);
        previousWhitespace = false;
    }
    while (!excerpt.empty() && excerpt.back() == ' ') {
        excerpt.pop_back();
    }
    if (body.size() > maxLength) {
        excerpt.append("...");
    }
    return excerpt;
}

[[nodiscard]] foundation::error::Error httpStatusError(
    const std::uint16_t statusCode,
    const std::string_view responseBody)
{
    foundation::error::ErrorCode code = foundation::error::ErrorCode::ProtocolFailure;
    switch (statusCode) {
    case 401:
    case 403:
        code = foundation::error::ErrorCode::Unauthorized;
        break;
    case 404:
        code = foundation::error::ErrorCode::NotFound;
        break;
    case 408:
        code = foundation::error::ErrorCode::Timeout;
        break;
    case 409:
        code = foundation::error::ErrorCode::Conflict;
        break;
    case 429:
        code = foundation::error::ErrorCode::RateLimited;
        break;
    default:
        if (statusCode >= 500U) {
            code = foundation::error::ErrorCode::Unavailable;
        }
        break;
    }

    std::string message = "Snapchain HTTP request failed with status " + std::to_string(statusCode) + ".";
    const std::string excerpt = responseExcerpt(responseBody);
    if (!excerpt.empty()) {
        message.append(" Node response: ");
        message.append(excerpt);
    }
    return makeError(code, std::move(message));
}

[[nodiscard]] std::string makeSignersUrl(
    const Config& config,
    const domain::identity::Fid fid,
    const std::optional<domain::identity::Fid> requestFid,
    const std::optional<std::string_view> pageToken)
{
    std::string url = trimTrailingSlashes(config.baseUrl);
    url.append("/v1/signersByFid?fid=");
    url.append(std::to_string(fid.value));
    url.append("&pageSize=");
    url.append(std::to_string(requestedPageSize));
    if (requestFid) {
        // serde_qs requires explicit sequence syntax even for one requester.
        // A scalar requesterFids value is rejected by Snapchain with HTTP 400.
        url.append("&requesterFids%5B0%5D=");
        url.append(std::to_string(requestFid->value));
    }
    if (pageToken && !pageToken->empty()) {
        url.append("&pageToken=");
        url.append(percentEncode(*pageToken));
    }
    return url;
}

struct Page final {
    std::vector<JsonObject> signers{};
    std::optional<std::string> nextPageToken{};
    std::uint32_t currentUserNonce{};
    std::optional<std::uint32_t> currentRequesterNonce{};
};

[[nodiscard]] foundation::result::Result<Page> decodePage(
    const std::string_view body,
    const std::optional<domain::identity::Fid> requestFid)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Snapchain signers response must be a JSON object."));
    }
    const JsonObject& object = document->as_object();

    const JsonValue* signersValue = findValue(object, "signers");
    if (signersValue == nullptr || !signersValue->is_array()) {
        return std::unexpected(protocolError("Snapchain signers response.signers must be an array."));
    }

    Page page{};
    page.signers.reserve(signersValue->as_array().size());
    for (const JsonValue& value : signersValue->as_array()) {
        if (!value.is_object()) {
            return std::unexpected(protocolError("Snapchain signers response entries must be objects."));
        }
        page.signers.push_back(value.as_object());
    }

    auto nextPageToken = optionalString(object, "nextPageToken", "response");
    auto currentUserNonce = requireUint32(object, "currentUserNonce", "response");
    if (!nextPageToken) { return std::unexpected(std::move(nextPageToken.error())); }
    if (!currentUserNonce) { return std::unexpected(std::move(currentUserNonce.error())); }
    page.nextPageToken = std::move(*nextPageToken);
    page.currentUserNonce = *currentUserNonce;

    if (requestFid) {
        const JsonValue* noncesValue = findValue(object, "requesterFidNonces");
        if (noncesValue == nullptr || !noncesValue->is_object()) {
            return std::unexpected(protocolError(
                "Snapchain signers response is missing requesterFidNonces for the requested FID."));
        }
        const std::string key = std::to_string(requestFid->value);
        auto requesterNonce = requireUint32(noncesValue->as_object(), key, "response.requesterFidNonces");
        if (!requesterNonce) {
            return std::unexpected(std::move(requesterNonce.error()));
        }
        page.currentRequesterNonce = *requesterNonce;
    }
    return page;
}

[[nodiscard]] foundation::result::Result<std::string> performGet(
    transport::http::IHttpTransport& transport,
    const Config& config,
    const std::string& url)
{
    auto response = transport.get(transport::http::HttpRequest{
        .url = url,
        .headers = {"Accept: application/json"},
        .timeout = config.timeout,
        .maxBodyBytes = config.maxBodyBytes,
    });
    if (!response) {
        return std::unexpected(std::move(response.error()));
    }
    if (response->statusCode < 200U || response->statusCode >= 300U) {
        return std::unexpected(httpStatusError(response->statusCode, response->body));
    }
    return std::move(response->body);
}

[[nodiscard]] foundation::result::Result<std::string> performSubmit(
    transport::http::IHttpTransport& transport,
    const Config& config,
    std::string body)
{
    std::string url = trimTrailingSlashes(config.baseUrl);
    url.append("/v1/submitMessage");
    auto response = transport.post(transport::http::HttpPostRequest{
        .url = std::move(url),
        .headers = {
            "Accept: application/json",
            "Content-Type: application/octet-stream",
        },
        .body = std::move(body),
        .timeout = config.timeout,
        .maxBodyBytes = config.maxBodyBytes,
    });
    if (!response) {
        return std::unexpected(std::move(response.error()));
    }
    if (response->statusCode < 200U || response->statusCode >= 300U) {
        return std::unexpected(httpStatusError(response->statusCode, response->body));
    }
    return std::move(response->body);
}

[[nodiscard]] foundation::result::Result<std::uint32_t> currentFarcasterTimestamp()
{
    return protocol::message::toFarcasterTimestamp(foundation::time::now());
}

}

class Registry final : public capability::signer::ISignerRegistry {
public:
    Registry(transport::http::IHttpTransport& transport, Config config)
        : m_transport(transport), m_config(std::move(config))
    {
    }

    [[nodiscard]] foundation::result::Result<capability::signer::RegistrySnapshot> inspect(
        const domain::identity::Fid fid,
        const domain::signer::PublicKey& publicKey,
        const std::optional<domain::identity::Fid> requestFid) override
    {
        if (fid.value == 0U) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Signer inspection requires a non-zero FID."));
        }
        if (requestFid && requestFid->value == 0U) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Signer inspection requester FID must be non-zero when provided."));
        }
        auto configStatus = validateConfig(m_config);
        if (!configStatus) {
            return std::unexpected(std::move(configStatus.error()));
        }
        auto normalizedKey = normalizePublicKeyHex(publicKey.hex, "publicKey");
        if (!normalizedKey) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                std::move(normalizedKey.error().message)));
        }

        capability::signer::RegistrySnapshot snapshot{};
        std::optional<domain::signer::Registration> gaslessMatch{};
        std::optional<domain::signer::Registration> legacyMatch{};
        std::optional<std::string> pageToken{};
        std::unordered_set<std::string> observedTokens{};

        for (std::size_t pageIndex = 0U; pageIndex < maximumPages; ++pageIndex) {
            auto body = performGet(
                m_transport,
                m_config,
                makeSignersUrl(
                    m_config,
                    fid,
                    requestFid,
                    pageToken ? std::optional<std::string_view>{*pageToken} : std::nullopt));
            if (!body) {
                return std::unexpected(std::move(body.error()));
            }
            auto page = decodePage(*body, requestFid);
            if (!page) {
                return std::unexpected(std::move(page.error()));
            }

            snapshot.nonces.currentUserNonce = std::max(
                snapshot.nonces.currentUserNonce,
                page->currentUserNonce);
            if (page->currentRequesterNonce) {
                const auto current = snapshot.nonces.currentRequesterNonce.value_or(0U);
                snapshot.nonces.currentRequesterNonce = std::max(current, *page->currentRequesterNonce);
            }

            for (const JsonObject& signerObject : page->signers) {
                auto responseKeyText = requireString(signerObject, "key", "signer");
                if (!responseKeyText) {
                    return std::unexpected(std::move(responseKeyText.error()));
                }
                auto responseKey = normalizePublicKeyHex(*responseKeyText, "signer.key");
                if (!responseKey) {
                    return std::unexpected(std::move(responseKey.error()));
                }
                if (*responseKey != *normalizedKey) {
                    continue;
                }

                auto registration = mapRegistration(signerObject, fid, *normalizedKey);
                if (!registration) {
                    return std::unexpected(std::move(registration.error()));
                }
                if (registration->kind == domain::signer::RegistrationKind::Gasless) {
                    if (gaslessMatch) {
                        return std::unexpected(protocolError(
                            "Snapchain returned duplicate gasless records for the same signer key."));
                    }
                    gaslessMatch = std::move(*registration);
                } else {
                    if (legacyMatch) {
                        return std::unexpected(protocolError(
                            "Snapchain returned duplicate on-chain records for the same signer key."));
                    }
                    legacyMatch = std::move(*registration);
                }
            }

            if (!page->nextPageToken || page->nextPageToken->empty()) {
                break;
            }
            if (!observedTokens.insert(*page->nextPageToken).second) {
                return std::unexpected(protocolError("Snapchain returned a repeating pagination token."));
            }
            pageToken = std::move(page->nextPageToken);

            if (pageIndex + 1U == maximumPages) {
                return std::unexpected(protocolError("Snapchain signer pagination exceeded the safety bound."));
            }
        }

        snapshot.registration = gaslessMatch ? std::move(gaslessMatch) : std::move(legacyMatch);
        return snapshot;
    }

    [[nodiscard]] foundation::result::Result<domain::signer::Registration> registerGasless(
        const capability::signer::KeyAddRequest& request,
        const capability::signer::KeyAddAuthorization& authorization,
        capability::signer::INativeSigner& proofSigner) override
    {
        auto configStatus = validateConfig(m_config);
        if (!configStatus) {
            return std::unexpected(std::move(configStatus.error()));
        }
        if (request.requestFid.value == 0U) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "KEY_ADD requires a non-zero request FID."));
        }

        auto expectedKey = normalizePublicKeyHex(request.publicKey.hex, "request.publicKey");
        if (!expectedKey) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                std::move(expectedKey.error().message)));
        }
        auto signerKey = proofSigner.publicKey();
        if (!signerKey) {
            return std::unexpected(std::move(signerKey.error()));
        }
        auto normalizedSignerKey = normalizePublicKeyHex(signerKey->hex, "proofSigner.publicKey");
        if (!normalizedSignerKey) {
            return std::unexpected(std::move(normalizedSignerKey.error()));
        }
        if (*normalizedSignerKey != *expectedKey) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Conflict,
                "KEY_ADD proof signer public key does not match the authorized key."));
        }

        auto deadline = protocol::message::toFarcasterTimestamp(request.deadline);
        auto timestamp = currentFarcasterTimestamp();
        if (!deadline) {
            return std::unexpected(std::move(deadline.error()));
        }
        if (!timestamp) {
            return std::unexpected(std::move(timestamp.error()));
        }

        auto unsignedMessage = protocol::message::encodeKeyAdd(protocol::message::KeyAddInput{
            .fid = request.fid,
            .publicKey = domain::signer::PublicKey{*expectedKey},
            .scopes = request.scopes,
            .ttlSeconds = request.ttlSeconds,
            .nonce = request.nonce,
            .deadline = *deadline,
            .timestamp = *timestamp,
            .network = protocol::message::Network::Mainnet,
            .custodySignature = authorization.custodySignature,
            .signedKeyRequestMetadata = authorization.signedKeyRequestMetadata,
        });
        if (!unsignedMessage) {
            return std::unexpected(std::move(unsignedMessage.error()));
        }

        auto signature = proofSigner.sign(unsignedMessage->hashBytes);
        if (!signature) {
            return std::unexpected(std::move(signature.error()));
        }
        auto encodedMessage = protocol::message::seal(*unsignedMessage, *signature);
        if (!encodedMessage) {
            return std::unexpected(std::move(encodedMessage.error()));
        }
        auto submitted = performSubmit(m_transport, m_config, std::move(*encodedMessage));
        if (!submitted) {
            return std::unexpected(std::move(submitted.error()));
        }

        auto observed = inspect(
            request.fid,
            domain::signer::PublicKey{*expectedKey},
            request.requestFid);
        if (!observed) {
            return std::unexpected(std::move(observed.error()));
        }
        if (!observed->registration) {
            return std::unexpected(protocolError(
                "Snapchain accepted KEY_ADD but the signer was not observable afterwards."));
        }

        const auto& registration = *observed->registration;
        if (registration.kind != domain::signer::RegistrationKind::Gasless ||
            registration.state != domain::signer::RegistrationState::Active ||
            registration.publicKey.hex != *expectedKey ||
            registration.ttlSeconds != request.ttlSeconds ||
            registration.scopes != request.scopes ||
            !registration.requestFid ||
            registration.requestFid->value != request.requestFid.value ||
            observed->nonces.currentUserNonce < request.nonce) {
            return std::unexpected(protocolError(
                "Snapchain post-submit signer state does not match the authorized KEY_ADD request."));
        }
        return registration;
    }

    [[nodiscard]] foundation::result::Status selfRevoke(
        const domain::identity::Fid fid,
        const domain::identity::Fid requestFid,
        const std::uint32_t nonce,
        const foundation::time::Timestamp deadline,
        capability::signer::INativeSigner& signer) override
    {
        auto configStatus = validateConfig(m_config);
        if (!configStatus) {
            return std::unexpected(std::move(configStatus.error()));
        }
        if (fid.value == 0U || requestFid.value == 0U) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "KEY_REMOVE self-revocation requires non-zero user and request FIDs."));
        }

        auto publicKey = signer.publicKey();
        if (!publicKey) {
            return std::unexpected(std::move(publicKey.error()));
        }
        auto normalizedKey = normalizePublicKeyHex(publicKey->hex, "signer.publicKey");
        if (!normalizedKey) {
            return std::unexpected(std::move(normalizedKey.error()));
        }
        auto farcasterDeadline = protocol::message::toFarcasterTimestamp(deadline);
        auto timestamp = currentFarcasterTimestamp();
        if (!farcasterDeadline) {
            return std::unexpected(std::move(farcasterDeadline.error()));
        }
        if (!timestamp) {
            return std::unexpected(std::move(timestamp.error()));
        }

        auto unsignedMessage = protocol::message::encodeKeyRemoveSelfRevoke(
            protocol::message::KeyRemoveSelfRevokeInput{
                .fid = fid,
                .publicKey = domain::signer::PublicKey{*normalizedKey},
                .nonce = nonce,
                .deadline = *farcasterDeadline,
                .timestamp = *timestamp,
                .network = protocol::message::Network::Mainnet,
            });
        if (!unsignedMessage) {
            return std::unexpected(std::move(unsignedMessage.error()));
        }
        auto signature = signer.sign(unsignedMessage->hashBytes);
        if (!signature) {
            return std::unexpected(std::move(signature.error()));
        }
        auto encodedMessage = protocol::message::seal(*unsignedMessage, *signature);
        if (!encodedMessage) {
            return std::unexpected(std::move(encodedMessage.error()));
        }
        auto submitted = performSubmit(m_transport, m_config, std::move(*encodedMessage));
        if (!submitted) {
            return std::unexpected(std::move(submitted.error()));
        }

        auto observed = inspect(
            fid,
            domain::signer::PublicKey{*normalizedKey},
            requestFid);
        if (!observed) {
            return std::unexpected(std::move(observed.error()));
        }
        if (observed->registration &&
            observed->registration->state == domain::signer::RegistrationState::Active) {
            return std::unexpected(protocolError(
                "Snapchain accepted KEY_REMOVE but the signer remains active afterwards."));
        }
        if (!observed->nonces.currentRequesterNonce ||
            *observed->nonces.currentRequesterNonce < nonce) {
            return std::unexpected(protocolError(
                "Snapchain post-revocation requester nonce did not advance to the submitted value."));
        }
        return {};
    }

private:
    transport::http::IHttpTransport& m_transport;
    Config m_config{};
};

std::unique_ptr<capability::signer::ISignerRegistry> makeRegistry(
    transport::http::IHttpTransport& transport,
    Config config)
{
    return std::make_unique<Registry>(transport, std::move(config));
}

}
