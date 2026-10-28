module;

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <expected>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module farcaster.provider.identity.openproof;

import farcaster.capability.auth;
import farcaster.domain.identity;
import farcaster.domain.session;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.transport.http;

namespace farcaster::provider::identity::openproof {
namespace {

using Json = boost::property_tree::ptree;

[[nodiscard]] foundation::error::Error makeError(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] std::string trim(std::string value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](const unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    if (first == value.end()) {
        return {};
    }
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](const unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    return std::string{first, last};
}

[[nodiscard]] std::string lower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

[[nodiscard]] bool validHttpsUrl(const std::string_view value)
{
    if (!value.starts_with("https://") || value.size() <= 8U) {
        return false;
    }
    for (const char raw : value) {
        const auto ch = static_cast<unsigned char>(raw);
        if (std::isspace(ch) != 0 || ch == '\\' || ch == '"' || ch == '\'') {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string withoutTrailingSlash(std::string value)
{
    value = trim(std::move(value));
    while (value.size() > std::string_view{"https://"}.size() && value.ends_with('/')) {
        value.pop_back();
    }
    return value;
}

[[nodiscard]] std::string joinUrl(const std::string& base, const std::string_view path)
{
    if (path.empty()) {
        return base;
    }
    return base + (path.front() == '/' ? std::string{path} : "/" + std::string{path});
}

[[nodiscard]] bool unreserved(const unsigned char ch) noexcept
{
    return std::isalnum(ch) != 0 || ch == '-' || ch == '.' || ch == '_' || ch == '~';
}

[[nodiscard]] std::string percentEncode(const std::string_view value)
{
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result{};
    result.reserve(value.size() * 3U);
    for (const char raw : value) {
        const auto ch = static_cast<unsigned char>(raw);
        if (unreserved(ch)) {
            result.push_back(static_cast<char>(ch));
            continue;
        }
        result.push_back('%');
        result.push_back(hex[(ch >> 4U) & 0x0FU]);
        result.push_back(hex[ch & 0x0FU]);
    }
    return result;
}

[[nodiscard]] std::string formBody(
    const std::vector<std::pair<std::string_view, std::string>>& values)
{
    std::string result{};
    for (const auto& [key, value] : values) {
        if (!result.empty()) {
            result.push_back('&');
        }
        result.append(percentEncode(key));
        result.push_back('=');
        result.append(percentEncode(value));
    }
    return result;
}

[[nodiscard]] std::string jsonEscape(const std::string_view value)
{
    std::string result{};
    result.reserve(value.size() + 8U);
    for (const char raw : value) {
        switch (raw) {
        case '\\': result.append("\\\\"); break;
        case '"': result.append("\\\""); break;
        case '\b': result.append("\\b"); break;
        case '\f': result.append("\\f"); break;
        case '\n': result.append("\\n"); break;
        case '\r': result.append("\\r"); break;
        case '\t': result.append("\\t"); break;
        default:
            if (static_cast<unsigned char>(raw) < 0x20U) {
                result.push_back(' ');
            } else {
                result.push_back(raw);
            }
            break;
        }
    }
    return result;
}

[[nodiscard]] foundation::result::Result<Json> parseJson(
    const std::string& body,
    const std::string_view context)
{
    try {
        std::istringstream stream{body};
        Json json{};
        boost::property_tree::read_json(stream, json);
        return json;
    } catch (const std::exception&) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            std::string{context} + " returned malformed JSON."));
    }
}

[[nodiscard]] foundation::result::Status requireStatus(
    const transport::http::HttpResponse& response,
    const std::uint16_t expected,
    const std::string_view context)
{
    if (response.statusCode == expected) {
        return {};
    }
    // OpenProof deliberately hides verification internals from the public API.
    // Only surface its opaque, non-secret correlation id for support; never
    // append the response body, signed message, cookies or bearer credentials.
    std::string requestId{};
    if (!response.body.empty() && response.body.size() <= 8'192U) {
        if (auto parsed = parseJson(response.body, "OpenProof error"); parsed) {
            requestId = parsed->get<std::string>("error.request_id", "");
        }
    }
    const bool safeId = !requestId.empty() && requestId.size() <= 96U
        && std::all_of(requestId.begin(), requestId.end(), [](const unsigned char ch) {
            return std::isalnum(ch) != 0 || ch == '-' || ch == '_';
        });
    std::string message = std::string{context} + " returned HTTP "
        + std::to_string(response.statusCode) + ".";
    if (safeId) {
        message.append(" Request ID: ").append(requestId).append(".");
    }
    return std::unexpected(makeError(
        response.statusCode == 401U || response.statusCode == 403U
            ? foundation::error::ErrorCode::Unauthorized
            : foundation::error::ErrorCode::ProtocolFailure,
        std::move(message)));
}

[[nodiscard]] foundation::result::Status requireSuccessStatus(
    const transport::http::HttpResponse& response,
    const std::string_view context)
{
    if (response.statusCode >= 200U && response.statusCode < 300U) {
        return {};
    }
    return requireStatus(response, 200U, context);
}

class CookieJar final {
public:
    void absorb(const transport::http::HttpResponse& response)
    {
        for (const auto& header : response.headers) {
            const auto separator = header.find(':');
            if (separator == std::string::npos
                || lower(trim(header.substr(0, separator))) != "set-cookie") {
                continue;
            }
            const std::string cookie = trim(header.substr(separator + 1U));
            const auto semicolon = cookie.find(';');
            const std::string pair = trim(cookie.substr(0, semicolon));
            const auto equals = pair.find('=');
            if (equals == std::string::npos || equals == 0U) {
                continue;
            }
            const std::string key = trim(pair.substr(0, equals));
            const std::string value = pair.substr(equals + 1U);
            if (value.empty()) {
                m_values.erase(key);
            } else {
                m_values[key] = value;
            }
        }
    }

    [[nodiscard]] std::string requestHeader() const
    {
        std::string result{};
        for (const auto& [name, value] : m_values) {
            if (!result.empty()) {
                result.append("; ");
            }
            result.append(name);
            result.push_back('=');
            result.append(value);
        }
        return result;
    }

    [[nodiscard]] bool empty() const noexcept { return m_values.empty(); }

private:
    std::map<std::string, std::string> m_values{};
};

[[nodiscard]] foundation::result::Result<std::uint64_t> parseFid(
    const std::string_view text)
{
    std::uint64_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value == 0U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "OpenProof returned an invalid Farcaster FID."));
    }
    return value;
}

struct AccountSnapshot final {
    std::string subject{};
    std::optional<std::string> username{};
    std::optional<std::string> displayName{};
    std::optional<std::string> avatarUrl{};
    std::optional<std::uint64_t> farcasterFid{};
};

[[nodiscard]] std::optional<std::string> optionalJsonText(
    const Json& json,
    const std::string& key)
{
    const std::string value = trim(json.get(key, std::string{}));
    return value.empty() ? std::nullopt : std::optional<std::string>{value};
}

struct FarcasterConnectionSnapshot final {
    std::uint64_t fid{};
    std::optional<std::string> username{};
    std::optional<std::string> displayName{};
    std::optional<std::string> avatarUrl{};
};

[[nodiscard]] foundation::result::Result<std::optional<FarcasterConnectionSnapshot>>
farcasterConnectionFromConnections(const Json& json)
{
    const auto connections = json.get_child_optional("connections");
    if (!connections) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "OpenProof account response omitted the connections list."));
    }

    std::optional<FarcasterConnectionSnapshot> farcaster{};
    for (const auto& entry : *connections) {
        const Json& connection = entry.second;
        if (lower(connection.get("provider", std::string{})) != "farcaster") {
            continue;
        }
        if (farcaster) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Conflict,
                "Genycaster requires exactly one linked Farcaster identity."));
        }

        const std::string fidText = trim(connection.get("fid", std::string{}));
        if (fidText.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "The linked OpenProof Farcaster identity omitted its FID."));
        }
        auto fid = parseFid(fidText);
        if (!fid) {
            return std::unexpected(std::move(fid.error()));
        }
        farcaster = FarcasterConnectionSnapshot{
            .fid = *fid,
            .username = optionalJsonText(connection, "preferred_username"),
            .displayName = optionalJsonText(connection, "display_name"),
            .avatarUrl = optionalJsonText(connection, "picture_url"),
        };
    }
    return farcaster;
}

[[nodiscard]] std::vector<std::string> bearerHeaders(const std::string& accessToken)
{
    return {
        "Authorization: Bearer " + accessToken,
        "Accept: application/json",
    };
}

[[nodiscard]] foundation::result::Result<AccountSnapshot> loadBearerAccount(
    transport::http::IHttpTransport& transport,
    const Config& config,
    const std::string& accessToken)
{
    auto userInfo = transport.get(transport::http::HttpRequest{
        .url = joinUrl(config.issuer, "/oauth/userinfo"),
        .headers = bearerHeaders(accessToken),
        .timeout = config.requestTimeout,
        .maxBodyBytes = 256U * 1024U,
    });
    if (!userInfo) {
        return std::unexpected(std::move(userInfo.error()));
    }
    if (auto status = requireStatus(*userInfo, 200U, "OpenProof userinfo"); !status) {
        return std::unexpected(std::move(status.error()));
    }
    auto claims = parseJson(userInfo->body, "OpenProof userinfo");
    if (!claims) {
        return std::unexpected(std::move(claims.error()));
    }
    const std::string subject = trim(claims->get("sub", std::string{}));
    if (subject.empty()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "OpenProof userinfo omitted the canonical subject."));
    }

    auto connections = transport.get(transport::http::HttpRequest{
        .url = joinUrl(config.issuer, "/account/connections"),
        .headers = bearerHeaders(accessToken),
        .timeout = config.requestTimeout,
        .maxBodyBytes = 512U * 1024U,
    });
    if (!connections) {
        return std::unexpected(std::move(connections.error()));
    }
    if (auto status = requireStatus(*connections, 200U, "OpenProof account connections"); !status) {
        return std::unexpected(std::move(status.error()));
    }
    auto connectionJson = parseJson(connections->body, "OpenProof account connections");
    if (!connectionJson) {
        return std::unexpected(std::move(connectionJson.error()));
    }
    auto farcaster = farcasterConnectionFromConnections(*connectionJson);
    if (!farcaster) {
        return std::unexpected(std::move(farcaster.error()));
    }
    if (!*farcaster) {
        return AccountSnapshot{.subject = subject};
    }
    const auto& connection = **farcaster;
    return AccountSnapshot{
        .subject = subject,
        .username = connection.username,
        .displayName = connection.displayName,
        .avatarUrl = connection.avatarUrl,
        .farcasterFid = connection.fid,
    };
}

[[nodiscard]] foundation::result::Result<AccountSnapshot> loadCookieAccount(
    transport::http::IHttpTransport& transport,
    const Config& config,
    const CookieJar& cookies,
    const std::optional<std::uint64_t> expectedFid,
    std::optional<std::string> relayUsername,
    std::optional<std::string> relayDisplayName,
    std::optional<std::string> relayAvatar)
{
    if (cookies.empty()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "OpenProof did not issue an authenticated session cookie."));
    }
    const std::vector<std::string> headers{
        "Cookie: " + cookies.requestHeader(),
        "Accept: application/json",
    };
    auto profile = transport.get(transport::http::HttpRequest{
        .url = joinUrl(config.issuer, "/account/profile"),
        .headers = headers,
        .timeout = config.requestTimeout,
        .maxBodyBytes = 256U * 1024U,
    });
    if (!profile) {
        return std::unexpected(std::move(profile.error()));
    }
    if (auto status = requireStatus(*profile, 200U, "OpenProof account profile"); !status) {
        return std::unexpected(std::move(status.error()));
    }
    auto profileJson = parseJson(profile->body, "OpenProof account profile");
    if (!profileJson) {
        return std::unexpected(std::move(profileJson.error()));
    }
    const std::string subject = trim(profileJson->get("identity_id", std::string{}));
    if (subject.empty()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "OpenProof account profile omitted the canonical identity."));
    }

    auto connections = transport.get(transport::http::HttpRequest{
        .url = joinUrl(config.issuer, "/account/connections"),
        .headers = headers,
        .timeout = config.requestTimeout,
        .maxBodyBytes = 512U * 1024U,
    });
    if (!connections) {
        return std::unexpected(std::move(connections.error()));
    }
    if (auto status = requireStatus(*connections, 200U, "OpenProof account connections"); !status) {
        return std::unexpected(std::move(status.error()));
    }
    auto connectionJson = parseJson(connections->body, "OpenProof account connections");
    if (!connectionJson) {
        return std::unexpected(std::move(connectionJson.error()));
    }
    auto farcaster = farcasterConnectionFromConnections(*connectionJson);
    if (!farcaster) {
        return std::unexpected(std::move(farcaster.error()));
    }
    if (!*farcaster) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "OpenProof completed Farcaster authentication without linking a Farcaster identity."));
    }
    const auto& connection = **farcaster;
    if (expectedFid && connection.fid != *expectedFid) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "The OpenProof Farcaster link does not match the approved Farcaster identity."));
    }

    auto username = connection.username ? connection.username : std::move(relayUsername);
    auto displayName = connection.displayName ? connection.displayName : std::move(relayDisplayName);
    auto avatar = connection.avatarUrl ? connection.avatarUrl : std::move(relayAvatar);

    return AccountSnapshot{
        .subject = subject,
        .username = std::move(username),
        .displayName = std::move(displayName),
        .avatarUrl = std::move(avatar),
        .farcasterFid = connection.fid,
    };
}

[[nodiscard]] domain::session::ClientSession sessionFrom(
    const AccountSnapshot& account,
    const Config& config)
{
    const auto authenticatedAt = foundation::time::now();
    return domain::session::ClientSession{
        .identityIssuer = config.issuer,
        .identitySubject = account.subject,
        .fid = domain::identity::Fid{*account.farcasterFid},
        .username = account.username,
        .displayName = account.displayName,
        .avatarUrl = account.avatarUrl,
        .custodyAddress = std::nullopt,
        .method = domain::session::AuthenticationMethod::Custody,
        .authenticatedAt = authenticatedAt,
        .expiresAt = authenticatedAt +
            std::chrono::duration_cast<std::chrono::milliseconds>(config.sessionLifetime),
    };
}

class Attempt final : public capability::auth::IAuthenticationAttempt {
public:
    enum class Mode : std::uint8_t {
        Device,
        DirectFarcaster,
    };

    Attempt(
        transport::http::IHttpTransport& transport,
        Config config,
        std::string deviceCode,
        std::chrono::seconds interval)
        : m_transport(transport),
          m_config(std::move(config)),
          m_mode(Mode::Device),
          m_deviceCode(std::move(deviceCode)),
          m_pollInterval(std::max(std::chrono::seconds{1}, interval)),
          m_deadline(std::chrono::steady_clock::now() + m_config.attemptLifetime),
          m_nextPoll(std::chrono::steady_clock::now())
    {
    }

    Attempt(
        transport::http::IHttpTransport& transport,
        Config config,
        CookieJar cookies,
        std::string relayToken,
        std::string relayNonce)
        : m_transport(transport),
          m_config(std::move(config)),
          m_mode(Mode::DirectFarcaster),
          m_cookies(std::move(cookies)),
          m_relayToken(std::move(relayToken)),
          m_relayNonce(std::move(relayNonce)),
          m_deadline(std::chrono::steady_clock::now() + m_config.attemptLifetime),
          m_nextPoll(std::chrono::steady_clock::now())
    {
    }

    [[nodiscard]] foundation::result::Result<capability::auth::PollResult> poll() override
    {
        if (std::chrono::steady_clock::now() >= m_deadline) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "The authentication attempt expired before completion."));
        }
        return m_mode == Mode::Device ? pollDevice() : pollDirectFarcaster();
    }

private:
    [[nodiscard]] foundation::result::Result<capability::auth::PollResult> pending() const
    {
        return capability::auth::PollResult{.state = capability::auth::PollState::Pending};
    }

    [[nodiscard]] foundation::result::Status refreshAccessToken()
    {
        if (m_refreshToken.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "The OpenProof access token expired before Farcaster linking completed."));
        }
        auto response = m_transport.post(transport::http::HttpPostRequest{
            .url = joinUrl(m_config.issuer, "/oauth/token"),
            .headers = {"Content-Type: application/x-www-form-urlencoded",
                        "Accept: application/json"},
            .body = formBody({
                {"grant_type", "refresh_token"},
                {"client_id", m_config.clientId},
                {"refresh_token", m_refreshToken},
            }),
            .timeout = m_config.requestTimeout,
            .maxBodyBytes = 256U * 1024U,
        });
        if (!response) {
            return std::unexpected(std::move(response.error()));
        }
        if (auto status = requireStatus(*response, 200U, "OpenProof token refresh"); !status) {
            return status;
        }
        auto json = parseJson(response->body, "OpenProof token refresh");
        if (!json) {
            return std::unexpected(std::move(json.error()));
        }
        const std::string accessToken = trim(json->get("access_token", std::string{}));
        if (accessToken.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "OpenProof token refresh omitted an access token."));
        }
        m_accessToken = accessToken;
        const std::string rotatedRefresh = trim(json->get("refresh_token", std::string{}));
        if (!rotatedRefresh.empty()) {
            m_refreshToken = rotatedRefresh;
        }
        const auto expiresIn = json->get("expires_in", std::int64_t{300});
        m_accessExpiresAt = std::chrono::steady_clock::now()
            + std::chrono::seconds{std::max<std::int64_t>(30, expiresIn - 10)};
        return {};
    }

    [[nodiscard]] foundation::result::Status ensureAccessToken()
    {
        if (m_accessToken.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "OpenProof authentication did not yield an access token."));
        }
        if (std::chrono::steady_clock::now() < m_accessExpiresAt) {
            return {};
        }
        return refreshAccessToken();
    }

    [[nodiscard]] foundation::result::Result<AccountSnapshot> bearerAccount()
    {
        if (auto ready = ensureAccessToken(); !ready) {
            return std::unexpected(std::move(ready.error()));
        }
        return loadBearerAccount(m_transport, m_config, m_accessToken);
    }

    [[nodiscard]] foundation::result::Result<std::string> beginFarcasterLink()
    {
        if (auto ready = ensureAccessToken(); !ready) {
            return std::unexpected(std::move(ready.error()));
        }

        auto challengeResponse = m_transport.post(transport::http::HttpPostRequest{
            .url = joinUrl(m_config.issuer, "/account/connections/web3/start"),
            .headers = {
                "Authorization: Bearer " + m_accessToken,
                "Content-Type: application/json",
                "Accept: application/json",
            },
            .body = "{\"provider\":\"farcaster\"}",
            .timeout = m_config.requestTimeout,
            .maxBodyBytes = 256U * 1024U,
        });
        if (!challengeResponse) {
            return std::unexpected(std::move(challengeResponse.error()));
        }
        if (auto status = requireStatus(
                *challengeResponse, 200U, "OpenProof Farcaster-link challenge"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        m_cookies.absorb(*challengeResponse);

        auto challenge = parseJson(
            challengeResponse->body, "OpenProof Farcaster-link challenge");
        if (!challenge) {
            return std::unexpected(std::move(challenge.error()));
        }
        const std::string nonce = trim(challenge->get("nonce", std::string{}));
        const std::string domain = trim(challenge->get("domain", std::string{}));
        const std::string uri = trim(challenge->get("uri", std::string{}));
        const std::string expiresAt = trim(challenge->get("expires_at", std::string{}));
        if (nonce.empty() || domain.empty() || uri.empty() || expiresAt.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "OpenProof Farcaster-link challenge is incomplete."));
        }

        const std::string relayUrl = joinUrl(
            withoutTrailingSlash(m_config.farcasterRelayBaseUrl),
            "/" + m_config.farcasterRelayVersion + "/channel");
        const std::string relayBody =
            "{\"siweUri\":\"" + jsonEscape(uri)
            + "\",\"domain\":\"" + jsonEscape(domain)
            + "\",\"nonce\":\"" + jsonEscape(nonce)
            + "\",\"expirationTime\":\"" + jsonEscape(expiresAt)
            + "\",\"acceptAuthAddress\":true}";
        auto relay = m_transport.post(transport::http::HttpPostRequest{
            .url = relayUrl,
            .headers = {"Content-Type: application/json", "Accept: application/json"},
            .body = relayBody,
            .timeout = m_config.requestTimeout,
            .maxBodyBytes = 512U * 1024U,
        });
        if (!relay) {
            return std::unexpected(std::move(relay.error()));
        }
        if (auto status = requireSuccessStatus(*relay, "Farcaster authentication relay"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        auto relayJson = parseJson(relay->body, "Farcaster authentication relay");
        if (!relayJson) {
            return std::unexpected(std::move(relayJson.error()));
        }
        const std::string relayToken = trim(relayJson->get("channelToken", std::string{}));
        const std::string relayNonce = trim(relayJson->get("nonce", std::string{}));
        const std::string approvalUrl = trim(relayJson->get("url", std::string{}));
        if (relayToken.empty() || relayNonce != nonce || !validHttpsUrl(approvalUrl)) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Farcaster relay returned an invalid OpenProof-bound link channel."));
        }

        m_relayToken = relayToken;
        m_relayNonce = relayNonce;
        m_farcasterApprovalUrl = approvalUrl;
        m_waitingForFarcasterLink = true;
        m_nextPoll = std::chrono::steady_clock::now();
        return approvalUrl;
    }

    [[nodiscard]] foundation::result::Result<capability::auth::PollResult>
    pollFarcasterLink()
    {
        const auto now = std::chrono::steady_clock::now();
        if (now < m_nextPoll) {
            return capability::auth::PollResult{
                .state = capability::auth::PollState::FarcasterLinkRequired,
                .approvalUrl = m_farcasterApprovalUrl,
            };
        }
        m_nextPoll = now + std::chrono::seconds{1};

        auto relay = m_transport.get(transport::http::HttpRequest{
            .url = joinUrl(
                withoutTrailingSlash(m_config.farcasterRelayBaseUrl),
                "/" + m_config.farcasterRelayVersion + "/channel/status"),
            .headers = {
                "Authorization: Bearer " + m_relayToken,
                "Accept: application/json",
            },
            .timeout = m_config.requestTimeout,
            .maxBodyBytes = 512U * 1024U,
        });
        if (!relay) {
            return std::unexpected(std::move(relay.error()));
        }
        auto relayJson = parseJson(relay->body, "Farcaster authentication relay");
        if (!relayJson) {
            return std::unexpected(std::move(relayJson.error()));
        }
        const std::string state = lower(trim(relayJson->get("state", std::string{})));
        if (relay->statusCode == 202U || state == "pending") {
            return capability::auth::PollResult{
                .state = capability::auth::PollState::FarcasterLinkRequired,
                .approvalUrl = m_farcasterApprovalUrl,
            };
        }
        if (relay->statusCode != 200U || (state != "completed" && state != "complete")) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Farcaster linking relay returned an invalid state."));
        }
        if (trim(relayJson->get("nonce", std::string{})) != m_relayNonce) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "Farcaster linking relay returned a mismatched nonce."));
        }

        const std::string message = relayJson->get("message", std::string{});
        const std::string signature = trim(relayJson->get("signature", std::string{}));
        const std::string fidText = trim(relayJson->get("fid", std::string{}));
        if (message.empty() || signature.empty() || fidText.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Farcaster linking relay returned an incomplete proof."));
        }
        auto expectedFid = parseFid(fidText);
        if (!expectedFid) {
            return std::unexpected(std::move(expectedFid.error()));
        }
        if (auto ready = ensureAccessToken(); !ready) {
            return std::unexpected(std::move(ready.error()));
        }

        std::vector<std::string> headers{
            "Authorization: Bearer " + m_accessToken,
            "Content-Type: application/json",
            "Accept: application/json",
        };
        const std::string cookieHeader = m_cookies.requestHeader();
        if (!cookieHeader.empty()) {
            headers.push_back("Cookie: " + cookieHeader);
        }
        const std::string body = "{\"message\":\"" + jsonEscape(message)
            + "\",\"signature\":\"" + jsonEscape(signature) + "\"}";
        auto completed = m_transport.post(transport::http::HttpPostRequest{
            .url = joinUrl(m_config.issuer, "/account/connections/web3/complete"),
            .headers = std::move(headers),
            .body = body,
            .timeout = m_config.requestTimeout,
            .maxBodyBytes = 512U * 1024U,
        });
        if (!completed) {
            return std::unexpected(std::move(completed.error()));
        }
        if (auto status = requireStatus(
                *completed, 200U, "OpenProof Farcaster-link verification"); !status) {
            return std::unexpected(std::move(status.error()));
        }

        auto account = bearerAccount();
        if (!account) {
            return std::unexpected(std::move(account.error()));
        }
        if (!account->farcasterFid || *account->farcasterFid != *expectedFid) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "OpenProof did not persist the approved Farcaster identity on this account."));
        }
        m_waitingForFarcasterLink = false;
        return capability::auth::PollResult{
            .state = capability::auth::PollState::Completed,
            .session = sessionFrom(*account, m_config),
            .accountGrant = verifiedAccountGrant(*account),
        };
    }

    [[nodiscard]] foundation::result::Result<capability::auth::PollResult> completeOrLink()
    {
        if (m_waitingForFarcasterLink) {
            return pollFarcasterLink();
        }
        auto account = bearerAccount();
        if (!account) {
            return std::unexpected(std::move(account.error()));
        }
        if (!account->farcasterFid) {
            auto approvalUrl = beginFarcasterLink();
            if (!approvalUrl) {
                return std::unexpected(std::move(approvalUrl.error()));
            }
            return capability::auth::PollResult{
                .state = capability::auth::PollState::FarcasterLinkRequired,
                .approvalUrl = std::move(*approvalUrl),
            };
        }
        return capability::auth::PollResult{
            .state = capability::auth::PollState::Completed,
            .session = sessionFrom(*account, m_config),
            .accountGrant = verifiedAccountGrant(*account),
        };
    }

    [[nodiscard]] std::optional<capability::auth::VerifiedAccountGrant>
    verifiedAccountGrant(const AccountSnapshot& account) const
    {
        if (!account.farcasterFid || m_accessToken.empty() || account.subject.empty())
            return std::nullopt;
        const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
            m_accessExpiresAt - std::chrono::steady_clock::now()).count();
        if (remaining < 30 || remaining > 3600) return std::nullopt;
        return capability::auth::VerifiedAccountGrant{
            .bearer = m_accessToken,
            .subject = account.subject,
            .fid = *account.farcasterFid,
            .expiresSeconds = static_cast<int>(remaining),
        };
    }

    [[nodiscard]] foundation::result::Result<capability::auth::PollResult> pollDevice()
    {
        if (!m_accessToken.empty()) {
            return completeOrLink();
        }
        const auto now = std::chrono::steady_clock::now();
        if (now < m_nextPoll) {
            return pending();
        }
        m_nextPoll = now + m_pollInterval;

        auto response = m_transport.post(transport::http::HttpPostRequest{
            .url = joinUrl(m_config.issuer, "/oauth/token"),
            .headers = {"Content-Type: application/x-www-form-urlencoded",
                        "Accept: application/json"},
            .body = formBody({
                {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"},
                {"client_id", m_config.clientId},
                {"device_code", m_deviceCode},
            }),
            .timeout = m_config.requestTimeout,
            .maxBodyBytes = 256U * 1024U,
        });
        if (!response) {
            return std::unexpected(std::move(response.error()));
        }
        auto json = parseJson(response->body, "OpenProof device token endpoint");
        if (!json) {
            return std::unexpected(std::move(json.error()));
        }
        if (response->statusCode != 200U) {
            const std::string code = trim(json->get("error", std::string{}));
            if (code == "authorization_pending") {
                return pending();
            }
            if (code == "slow_down") {
                m_pollInterval += std::chrono::seconds{5};
                m_nextPoll = std::chrono::steady_clock::now() + m_pollInterval;
                return pending();
            }
            if (code == "access_denied") {
                return std::unexpected(makeError(
                    foundation::error::ErrorCode::Unauthorized,
                    "OpenProof device authorization was denied."));
            }
            if (code == "expired_token") {
                return std::unexpected(makeError(
                    foundation::error::ErrorCode::Unauthorized,
                    "OpenProof device authorization expired."));
            }
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "OpenProof device token exchange failed."));
        }

        m_accessToken = trim(json->get("access_token", std::string{}));
        m_refreshToken = trim(json->get("refresh_token", std::string{}));
        const auto expiresIn = json->get("expires_in", std::int64_t{300});
        if (m_accessToken.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "OpenProof device token exchange omitted an access token."));
        }
        m_accessExpiresAt = std::chrono::steady_clock::now()
            + std::chrono::seconds{std::max<std::int64_t>(30, expiresIn - 10)};
        return completeOrLink();
    }

    [[nodiscard]] foundation::result::Result<capability::auth::PollResult> pollDirectFarcaster()
    {
        const auto now = std::chrono::steady_clock::now();
        if (now < m_nextPoll) {
            return pending();
        }
        m_nextPoll = now + std::chrono::seconds{1};

        auto response = m_transport.get(transport::http::HttpRequest{
            .url = joinUrl(
                withoutTrailingSlash(m_config.farcasterRelayBaseUrl),
                "/" + m_config.farcasterRelayVersion + "/channel/status"),
            .headers = {"Authorization: Bearer " + m_relayToken,
                        "Accept: application/json"},
            .timeout = m_config.requestTimeout,
            .maxBodyBytes = 512U * 1024U,
        });
        if (!response) {
            return std::unexpected(std::move(response.error()));
        }
        auto json = parseJson(response->body, "Farcaster authentication relay");
        if (!json) {
            return std::unexpected(std::move(json.error()));
        }
        const std::string state = lower(trim(json->get("state", std::string{})));
        if (response->statusCode == 202U || state == "pending") {
            return pending();
        }
        if (response->statusCode != 200U || (state != "completed" && state != "complete")) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Farcaster authentication relay returned an invalid state."));
        }
        if (trim(json->get("nonce", std::string{})) != m_relayNonce) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "Farcaster authentication relay returned a mismatched nonce."));
        }

        const std::string message = json->get("message", std::string{});
        const std::string signature = trim(json->get("signature", std::string{}));
        const std::string fidText = trim(json->get("fid", std::string{}));
        if (message.empty() || signature.empty() || fidText.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Farcaster authentication relay returned an incomplete proof."));
        }
        auto expectedFid = parseFid(fidText);
        if (!expectedFid) {
            return std::unexpected(std::move(expectedFid.error()));
        }

        const auto username = optionalJsonText(*json, "username");
        const auto displayName = optionalJsonText(*json, "displayName");
        const auto avatar = optionalJsonText(*json, "pfpUrl");
        const std::string body = "{\"message\":\"" + jsonEscape(message)
            + "\",\"signature\":\"" + jsonEscape(signature) + "\"}";

        std::vector<std::string> headers{
            "Content-Type: application/json",
            "Accept: application/json",
        };
        const std::string cookieHeader = m_cookies.requestHeader();
        if (!cookieHeader.empty()) {
            headers.push_back("Cookie: " + cookieHeader);
        }
        auto completed = m_transport.post(transport::http::HttpPostRequest{
            .url = joinUrl(m_config.issuer, "/auth/web3/complete"),
            .headers = std::move(headers),
            .body = std::move(body),
            .timeout = m_config.requestTimeout,
            .maxBodyBytes = 512U * 1024U,
        });
        if (!completed) {
            return std::unexpected(std::move(completed.error()));
        }
        if (auto status = requireStatus(*completed, 200U, "OpenProof Farcaster verification"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        m_cookies.absorb(*completed);

        auto account = loadCookieAccount(
            m_transport,
            m_config,
            m_cookies,
            *expectedFid,
            username,
            displayName,
            avatar);
        if (!account) {
            return std::unexpected(std::move(account.error()));
        }
        return capability::auth::PollResult{
            .state = capability::auth::PollState::Completed,
            .session = sessionFrom(*account, m_config),
        };
    }

    transport::http::IHttpTransport& m_transport;
    Config m_config;
    Mode m_mode{Mode::Device};
    CookieJar m_cookies{};
    std::string m_deviceCode{};
    std::string m_accessToken{};
    std::string m_refreshToken{};
    bool m_waitingForFarcasterLink{false};
    std::string m_relayToken{};
    std::string m_relayNonce{};
    std::string m_farcasterApprovalUrl{};
    std::chrono::seconds m_pollInterval{1};
    std::chrono::steady_clock::time_point m_deadline{};
    std::chrono::steady_clock::time_point m_nextPoll{};
    std::chrono::steady_clock::time_point m_accessExpiresAt{};
};

class Authentication final : public capability::auth::IAuthenticationProvider {
public:
    Authentication(transport::http::IHttpTransport& transport, Config config)
        : m_transport(transport), m_config(std::move(config))
    {
    }

    [[nodiscard]] foundation::result::Result<capability::auth::StartedAuthentication> begin(
        const capability::auth::SignInRequest& request) override
    {
        return request.route == capability::auth::SignInRoute::OpenProof
            ? beginOpenProof()
            : beginFarcaster();
    }

private:
    [[nodiscard]] foundation::result::Result<capability::auth::StartedAuthentication>
    beginOpenProof()
    {
        auto response = m_transport.post(transport::http::HttpPostRequest{
            .url = joinUrl(m_config.issuer, "/oauth/device_authorization"),
            .headers = {"Content-Type: application/x-www-form-urlencoded",
                        "Accept: application/json"},
            .body = formBody({
                {"client_id", m_config.clientId},
                {"scope", "openid profile account offline_access"},
            }),
            .timeout = m_config.requestTimeout,
            .maxBodyBytes = 256U * 1024U,
        });
        if (!response) {
            return std::unexpected(std::move(response.error()));
        }
        if (auto status = requireStatus(*response, 200U, "OpenProof device authorization"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        auto json = parseJson(response->body, "OpenProof device authorization");
        if (!json) {
            return std::unexpected(std::move(json.error()));
        }
        const std::string deviceCode = trim(json->get("device_code", std::string{}));
        const std::string userCode = trim(json->get("user_code", std::string{}));
        const auto interval = json->get("interval", std::int64_t{5});
        if (deviceCode.empty() || userCode.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "OpenProof device authorization returned an incomplete challenge."));
        }
        const std::string returnTarget =
            "/openproof/oauth/device?user_code=" + userCode;
        return capability::auth::StartedAuthentication{
            .challenge = capability::auth::AuthenticationChallenge{
                .approvalUrl = m_config.genyleapAccountUrl
                    + "?return_to=" + percentEncode(returnTarget),
            },
            .attempt = std::make_unique<Attempt>(
                m_transport,
                m_config,
                deviceCode,
                std::chrono::seconds{std::max<std::int64_t>(1, interval)}),
        };
    }

    [[nodiscard]] foundation::result::Result<capability::auth::StartedAuthentication>
    beginFarcaster()
    {
        auto start = m_transport.post(transport::http::HttpPostRequest{
            .url = joinUrl(m_config.issuer, "/auth/web3/start"),
            .headers = {"Content-Type: application/json",
                        "Accept: application/json"},
            .body = "{\"provider\":\"farcaster\"}",
            .timeout = m_config.requestTimeout,
            .maxBodyBytes = 256U * 1024U,
        });
        if (!start) {
            return std::unexpected(std::move(start.error()));
        }
        if (auto status = requireStatus(*start, 200U, "OpenProof Farcaster challenge"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        auto json = parseJson(start->body, "OpenProof Farcaster challenge");
        if (!json) {
            return std::unexpected(std::move(json.error()));
        }
        const std::string nonce = trim(json->get("nonce", std::string{}));
        const std::string domain = trim(json->get("domain", std::string{}));
        const std::string uri = trim(json->get("uri", std::string{}));
        const std::string expiresAt = trim(json->get("expires_at", std::string{}));
        if (nonce.empty() || domain.empty() || uri.empty() || expiresAt.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "OpenProof Farcaster challenge is incomplete."));
        }
        CookieJar cookies{};
        cookies.absorb(*start);
        if (cookies.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "OpenProof Farcaster challenge omitted its ceremony cookie."));
        }

        const std::string relayUrl = joinUrl(
            withoutTrailingSlash(m_config.farcasterRelayBaseUrl),
            "/" + m_config.farcasterRelayVersion + "/channel");
        const std::string relayBody =
            "{\"siweUri\":\"" + jsonEscape(uri)
            + "\",\"domain\":\"" + jsonEscape(domain)
            + "\",\"nonce\":\"" + jsonEscape(nonce)
            + "\",\"expirationTime\":\"" + jsonEscape(expiresAt)
            + "\",\"acceptAuthAddress\":true}";
        auto relay = m_transport.post(transport::http::HttpPostRequest{
            .url = relayUrl,
            .headers = {"Content-Type: application/json",
                        "Accept: application/json"},
            .body = relayBody,
            .timeout = m_config.requestTimeout,
            .maxBodyBytes = 512U * 1024U,
        });
        if (!relay) {
            return std::unexpected(std::move(relay.error()));
        }
        if (auto status = requireSuccessStatus(*relay, "Farcaster authentication relay"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        auto relayJson = parseJson(relay->body, "Farcaster authentication relay");
        if (!relayJson) {
            return std::unexpected(std::move(relayJson.error()));
        }
        const std::string relayToken = trim(relayJson->get("channelToken", std::string{}));
        const std::string relayNonce = trim(relayJson->get("nonce", std::string{}));
        const std::string approvalUrl = trim(relayJson->get("url", std::string{}));
        if (relayToken.empty() || relayNonce != nonce || !validHttpsUrl(approvalUrl)) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Farcaster relay returned an invalid OpenProof-bound approval channel."));
        }
        return capability::auth::StartedAuthentication{
            .challenge = capability::auth::AuthenticationChallenge{
                .approvalUrl = approvalUrl,
            },
            .attempt = std::make_unique<Attempt>(
                m_transport,
                m_config,
                std::move(cookies),
                relayToken,
                relayNonce),
        };
    }

    transport::http::IHttpTransport& m_transport;
    Config m_config;
};

}

foundation::result::Result<std::unique_ptr<capability::auth::IAuthenticationProvider>>
makeAuthentication(transport::http::IHttpTransport& transport, Config config)
{
    config.issuer = withoutTrailingSlash(std::move(config.issuer));
    config.genyleapAccountUrl =
        withoutTrailingSlash(std::move(config.genyleapAccountUrl));
    config.farcasterRelayBaseUrl =
        withoutTrailingSlash(std::move(config.farcasterRelayBaseUrl));
    config.clientId = trim(std::move(config.clientId));
    config.farcasterRelayVersion = trim(std::move(config.farcasterRelayVersion));

    if (!validHttpsUrl(config.issuer)
        || !validHttpsUrl(config.genyleapAccountUrl)
        || !validHttpsUrl(config.farcasterRelayBaseUrl)
        || config.issuer.find_first_of("/?#", std::string_view{"https://"}.size()) != std::string::npos
        || config.genyleapAccountUrl.find_first_of("?#") != std::string::npos
        || config.clientId.empty()
        || config.clientId.size() > 256U
        || config.farcasterRelayVersion.empty()
        || config.farcasterRelayVersion.find_first_of("/?#\\") != std::string::npos
        || config.requestTimeout <= std::chrono::milliseconds::zero()
        || config.attemptLifetime <= std::chrono::minutes::zero()
        || config.sessionLifetime <= std::chrono::hours::zero()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "OpenProof authentication configuration is invalid."));
    }

    std::unique_ptr<capability::auth::IAuthenticationProvider> authentication =
        std::make_unique<Authentication>(transport, std::move(config));
    return authentication;
}

}
