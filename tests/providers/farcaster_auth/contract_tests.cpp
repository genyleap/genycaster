#include <cassert>
#include <cstdint>
#include <deque>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import farcaster.capability.auth;
import farcaster.domain.identity;
import farcaster.domain.session;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.compatibility.auth;
import farcaster.transport.http;

namespace {

namespace capability = farcaster::capability;
namespace domain = farcaster::domain;
namespace foundation = farcaster::foundation;
namespace provider = farcaster::provider::compatibility::auth;
namespace transport = farcaster::transport::http;

struct RecordedCall final {
    std::string method{};
    std::string url{};
    std::vector<std::string> headers{};
    std::string body{};
};

class FakeTransport final : public transport::IHttpTransport {
public:
    void enqueue(std::uint16_t statusCode, std::string body)
    {
        m_responses.push_back(transport::HttpResponse{
            .statusCode = statusCode,
            .body = std::move(body),
        });
    }

    [[nodiscard]] foundation::result::Result<transport::HttpResponse>
    get(const transport::HttpRequest& request) override
    {
        m_calls.push_back(RecordedCall{
            .method = "GET",
            .url = request.url,
            .headers = request.headers,
        });
        return pop();
    }

    [[nodiscard]] foundation::result::Result<transport::HttpResponse>
    post(const transport::HttpPostRequest& request) override
    {
        m_calls.push_back(RecordedCall{
            .method = "POST",
            .url = request.url,
            .headers = request.headers,
            .body = request.body,
        });
        return pop();
    }

    [[nodiscard]] const std::vector<RecordedCall>& calls() const noexcept
    {
        return m_calls;
    }

private:
    [[nodiscard]] foundation::result::Result<transport::HttpResponse> pop()
    {
        if (m_responses.empty()) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InternalFailure,
                "No deterministic authentication response remains."));
        }
        auto response = std::move(m_responses.front());
        m_responses.pop_front();
        return response;
    }

    std::deque<transport::HttpResponse> m_responses{};
    std::vector<RecordedCall> m_calls{};
};

[[nodiscard]] std::string escapeJson(std::string_view value)
{
    std::string result{};
    for (const char ch : value) {
        switch (ch) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result.push_back(ch); break;
        }
    }
    return result;
}

[[nodiscard]] std::string abiWord(std::uint64_t value)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string word(64, '0');
    std::size_t index = word.size();
    do {
        word[--index] = digits[value & 0xfU];
        value >>= 4U;
    } while (value != 0U && index > 0U);
    return word;
}

[[nodiscard]] std::string abiAddressWord(const char hexDigit)
{
    return std::string(24U, '0') + std::string(40U, hexDigit);
}

[[nodiscard]] std::string siweMessage(
    const std::uint64_t resourceFid,
    const std::string_view temporalField = {})
{
    std::string message = "genycaster.example wants you to sign in with your Ethereum account:\n"
        "0x1111111111111111111111111111111111111111\n\n"
        "Farcaster Auth\n\n"
        "URI: https://genycaster.example/sign-in\n"
        "Version: 1\n"
        "Chain ID: 10\n"
        "Nonce: abcdef12\n"
        "Issued At: 2026-08-28T00:00:00.000Z\n";
    if (!temporalField.empty()) {
        message += temporalField;
        message += '\n';
    }
    message += "Resources:\n- farcaster://fid/" + std::to_string(resourceFid);
    return message;
}

[[nodiscard]] std::string completedStatus(
    const std::uint64_t resourceFid,
    const std::string_view temporalField = {},
    std::string signature = {})
{
    if (signature.empty()) {
        signature = "0x" + std::string(128, '0') + "1b";
    }
    return "{"
        "\"state\":\"completed\","
        "\"nonce\":\"abcdef12\","
        "\"message\":\"" + escapeJson(siweMessage(resourceFid, temporalField)) + "\","
        "\"signature\":\"" + signature + "\","
        "\"fid\":42,"
        "\"username\":\"alice\","
        "\"displayName\":\"Alice\","
        "\"acceptAuthAddress\":true,"
        "\"signatureParams\":{"
            "\"domain\":\"genycaster.example\","
            "\"siweUri\":\"https://genycaster.example/sign-in\""
        "}"
    "}";
}

void verifiedCustodyFlowReturnsNormalizedSession()
{
    FakeTransport transport{};
    transport.enqueue(200,
        "{\"channelToken\":\"secret-token\",\"url\":\"https://farcaster.xyz/~/sign-in-with-farcaster?channelToken=secret-token\",\"nonce\":\"abcdef12\"}");
    transport.enqueue(200, completedStatus(42));
    transport.enqueue(200, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x01\"}");
    transport.enqueue(200,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x" + abiWord(0) + abiWord(0) + "\"}");
    transport.enqueue(200,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x" + abiWord(42) + "\"}");
    transport.enqueue(200,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x" + abiAddressWord('1') + "\"}");

    auto authentication = provider::makeAuthentication(transport, provider::Config{
        .relayBaseUrl = "https://relay.example",
        .relayVersion = "v1",
        .optimismRpcUrl = "https://rpc.example",
    });
    assert(authentication);
    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .domain = "genycaster.example",
        .siweUri = "https://genycaster.example/sign-in",
    });
    assert(started);
    assert(started->challenge.approvalUrl ==
        "https://farcaster.xyz/~/sign-in-with-farcaster?channelToken=secret-token");

    auto polled = started->attempt->poll();
    assert(polled);
    assert(polled->state == capability::auth::PollState::Completed);
    assert(polled->session);
    assert(polled->session->fid.value == 42);
    assert(polled->session->username && *polled->session->username == "alice");
    assert(polled->session->method == domain::session::AuthenticationMethod::Custody);
    assert(polled->session->custodyAddress &&
        *polled->session->custodyAddress == "0x1111111111111111111111111111111111111111");

    const auto& calls = transport.calls();
    assert(calls.size() == 6);
    assert(calls.front().method == "POST");
    assert(calls.front().url == "https://relay.example/v1/channel");
    assert(calls.front().body.find("secret-token") == std::string::npos);
    assert(calls[1].method == "GET");
    assert(calls[1].url == "https://relay.example/v1/channel/status");
    bool foundBearer = false;
    for (const auto& header : calls[1].headers) {
        foundBearer = foundBearer || header == "Authorization: Bearer secret-token";
    }
    assert(foundBearer);
    assert(calls[2].method == "POST");
    assert(calls[2].url == "https://rpc.example");
    assert(calls[2].body.find("\"method\":\"eth_call\"") != std::string::npos);
    assert(calls[2].body.find("\"to\"") == std::string::npos);
}

void currentCustodyCanBeResolvedWithoutNewSignIn()
{
    FakeTransport transport{};
    transport.enqueue(200,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x" + abiAddressWord('3') + "\"}");

    auto authentication = provider::makeAuthentication(transport, provider::Config{
        .relayBaseUrl = "https://relay.example",
        .relayVersion = "v1",
        .optimismRpcUrl = "https://rpc.example",
    });
    assert(authentication);
    auto custody = (*authentication)->resolveCustodyAddress(
        farcaster::domain::identity::Fid{42});
    assert(custody);
    assert(*custody == "0x3333333333333333333333333333333333333333");
    assert(transport.calls().size() == 1U);
    assert(transport.calls().front().url == "https://rpc.example");
    assert(transport.calls().front().body.find("custodyOf") == std::string::npos);
    assert(transport.calls().front().body.find("\"method\":\"eth_call\"") != std::string::npos);
}

void activeAuthAddressIsPreferredOverCustodyFallback()
{
    FakeTransport transport{};
    transport.enqueue(200,
        "{\"channelToken\":\"secret-token\",\"url\":\"https://farcaster.xyz/~/sign-in-with-farcaster?channelToken=secret-token\",\"nonce\":\"abcdef12\"}");
    transport.enqueue(200, completedStatus(42));
    transport.enqueue(200, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x01\"}");
    transport.enqueue(200,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x" + abiWord(1) + abiWord(2) + "\"}");
    transport.enqueue(200,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x" + abiAddressWord('2') + "\"}");

    auto authentication = provider::makeAuthentication(transport, provider::Config{
        .relayBaseUrl = "https://relay.example",
        .relayVersion = "v1",
        .optimismRpcUrl = "https://rpc.example",
    });
    assert(authentication);
    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .domain = "genycaster.example",
        .siweUri = "https://genycaster.example/sign-in",
    });
    assert(started);
    auto polled = started->attempt->poll();
    assert(polled);
    assert(polled->session);
    assert(polled->session->method == domain::session::AuthenticationMethod::AuthAddress);
    assert(polled->session->custodyAddress &&
        *polled->session->custodyAddress == "0x2222222222222222222222222222222222222222");
    assert(transport.calls().size() == 5);
}

void erc6492CounterfactualSignatureUsesUniversalVerifier()
{
    FakeTransport transport{};
    transport.enqueue(200,
        "{\"channelToken\":\"secret-token\",\"url\":\"https://farcaster.xyz/~/sign-in-with-farcaster?channelToken=secret-token\",\"nonce\":\"abcdef12\"}");
    const std::string erc6492Magic =
        "6492649264926492649264926492649264926492649264926492649264926492";
    transport.enqueue(200, completedStatus(42, {}, "0x1234" + erc6492Magic));
    transport.enqueue(200, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x01\"}");
    transport.enqueue(200,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x" + abiWord(0) + abiWord(0) + "\"}");
    transport.enqueue(200,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x" + abiWord(42) + "\"}");
    transport.enqueue(200,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x" + abiAddressWord('1') + "\"}");

    auto authentication = provider::makeAuthentication(transport, provider::Config{
        .relayBaseUrl = "https://relay.example",
        .relayVersion = "v1",
        .optimismRpcUrl = "https://rpc.example",
    });
    assert(authentication);
    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .domain = "genycaster.example",
        .siweUri = "https://genycaster.example/sign-in",
    });
    assert(started);
    auto polled = started->attempt->poll();
    assert(polled);
    assert(polled->state == capability::auth::PollState::Completed);
    assert(polled->session && polled->session->fid.value == 42);
    assert(polled->session->custodyAddress &&
        *polled->session->custodyAddress == "0x1111111111111111111111111111111111111111");
    assert(transport.calls().size() == 6);
    assert(transport.calls()[2].body.find(erc6492Magic) != std::string::npos);
}

void invalidUniversalSignatureIsRejectedBeforeFidAuthorization()
{
    FakeTransport transport{};
    transport.enqueue(200,
        "{\"channelToken\":\"secret-token\",\"url\":\"https://farcaster.xyz/~/sign-in-with-farcaster?channelToken=secret-token\",\"nonce\":\"abcdef12\"}");
    transport.enqueue(200, completedStatus(42));
    transport.enqueue(200, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0x00\"}");

    auto authentication = provider::makeAuthentication(transport, provider::Config{
        .relayBaseUrl = "https://relay.example",
        .relayVersion = "v1",
        .optimismRpcUrl = "https://rpc.example",
    });
    assert(authentication);
    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .domain = "genycaster.example",
        .siweUri = "https://genycaster.example/sign-in",
    });
    assert(started);
    auto polled = started->attempt->poll();
    assert(!polled);
    assert(polled.error().code == foundation::error::ErrorCode::Unauthorized);
    assert(transport.calls().size() == 3);
}

void wrongFidResourceIsRejectedBeforeChainVerification()
{
    FakeTransport transport{};
    transport.enqueue(200,
        "{\"channelToken\":\"secret-token\",\"url\":\"https://farcaster.xyz/~/sign-in-with-farcaster?channelToken=secret-token\",\"nonce\":\"abcdef12\"}");
    transport.enqueue(200, completedStatus(43));

    auto authentication = provider::makeAuthentication(transport, provider::Config{
        .relayBaseUrl = "https://relay.example",
        .relayVersion = "v1",
        .optimismRpcUrl = "https://rpc.example",
    });
    assert(authentication);
    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .domain = "genycaster.example",
        .siweUri = "https://genycaster.example/sign-in",
    });
    assert(started);
    auto polled = started->attempt->poll();
    assert(!polled);
    assert(polled.error().code == foundation::error::ErrorCode::Unauthorized);
    assert(transport.calls().size() == 2);
}

void expiredSiwfProofIsRejectedBeforeChainVerification()
{
    FakeTransport transport{};
    transport.enqueue(200,
        "{\"channelToken\":\"secret-token\",\"url\":\"https://farcaster.xyz/~/sign-in-with-farcaster?channelToken=secret-token\",\"nonce\":\"abcdef12\"}");
    transport.enqueue(200, completedStatus(
        42, "Expiration Time: 2020-01-01T00:00:00.000Z"));

    auto authentication = provider::makeAuthentication(transport, provider::Config{
        .relayBaseUrl = "https://relay.example",
        .relayVersion = "v1",
        .optimismRpcUrl = "https://rpc.example",
    });
    assert(authentication);
    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .domain = "genycaster.example",
        .siweUri = "https://genycaster.example/sign-in",
    });
    assert(started);
    auto polled = started->attempt->poll();
    assert(!polled);
    assert(polled.error().code == foundation::error::ErrorCode::Unauthorized);
    assert(transport.calls().size() == 2);
}

void futureNotBeforeIsRejectedBeforeChainVerification()
{
    FakeTransport transport{};
    transport.enqueue(200,
        "{\"channelToken\":\"secret-token\",\"url\":\"https://farcaster.xyz/~/sign-in-with-farcaster?channelToken=secret-token\",\"nonce\":\"abcdef12\"}");
    transport.enqueue(200, completedStatus(
        42, "Not Before: 2099-01-01T00:00:00.000Z"));

    auto authentication = provider::makeAuthentication(transport, provider::Config{
        .relayBaseUrl = "https://relay.example",
        .relayVersion = "v1",
        .optimismRpcUrl = "https://rpc.example",
    });
    assert(authentication);
    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .domain = "genycaster.example",
        .siweUri = "https://genycaster.example/sign-in",
    });
    assert(started);
    auto polled = started->attempt->poll();
    assert(!polled);
    assert(polled.error().code == foundation::error::ErrorCode::Unauthorized);
    assert(transport.calls().size() == 2);
}

void relayValidationFailureKeepsSafeDiagnosticDetail()
{
    FakeTransport transport{};
    transport.enqueue(200,
        "{\"channelToken\":\"secret-token\",\"url\":\"https://farcaster.xyz/~/sign-in-with-farcaster?channelToken=secret-token\",\"nonce\":\"abcdef12\"}");
    transport.enqueue(400,
        "{\"error\":\"Validation error\",\"message\":\"Authorization header is malformed\"}");

    auto authentication = provider::makeAuthentication(transport, provider::Config{
        .relayBaseUrl = "https://relay.example",
        .relayVersion = "v1",
        .optimismRpcUrl = "https://rpc.example",
    });
    assert(authentication);
    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .domain = "genycaster.example",
        .siweUri = "https://genycaster.example/sign-in",
    });
    assert(started);

    auto polled = started->attempt->poll();
    assert(!polled);
    assert(polled.error().code == foundation::error::ErrorCode::ProtocolFailure);
    assert(polled.error().message.find("Validation error") != std::string::npos);
    assert(polled.error().message.find("Authorization header is malformed") != std::string::npos);
    assert(polled.error().message.find("secret-token") == std::string::npos);

    const auto& statusCall = transport.calls()[1];
    assert(statusCall.headers.size() == 1);
    assert(statusCall.headers.front() == "Authorization: Bearer secret-token");
}

void relayRejectsInvalidRelyingPartyAsConfigurationError()
{
    FakeTransport transport{};
    transport.enqueue(400,
        "{\"error\":\"Validation error\",\"message\":\"body/siweUri must match format uri\"}");

    auto authentication = provider::makeAuthentication(transport, provider::Config{
        .relayBaseUrl = "https://relay.example",
        .relayVersion = "v1",
        .optimismRpcUrl = "https://rpc.example",
    });
    assert(authentication);
    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .domain = "genycaster.example",
        .siweUri = "https://genycaster.example/login",
    });
    assert(!started);
    assert(started.error().code == foundation::error::ErrorCode::InvalidArgument);
    assert(started.error().message.find("Validation error") != std::string::npos);
}

void malformedSiwfUriIsRejectedBeforeRelayRequest()
{
    FakeTransport transport{};
    auto authentication = provider::makeAuthentication(transport, provider::Config{
        .relayBaseUrl = "https://relay.example",
        .relayVersion = "v1",
        .optimismRpcUrl = "https://rpc.example",
    });
    assert(authentication);

    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .domain = "localhost",
        .siweUri = "http://localhost/login \\",
    });
    assert(!started);
    assert(started.error().code == foundation::error::ErrorCode::InvalidArgument);
    assert(transport.calls().empty());
}

void defaultConfigurationUsesOfficialFarcasterRelay()
{
    FakeTransport transport{};
    transport.enqueue(201,
        "{\"channelToken\":\"secret-token\",\"url\":\"https://farcaster.xyz/~/sign-in-with-farcaster?channelToken=secret-token\",\"nonce\":\"abcdef12\"}");

    auto authentication = provider::makeAuthentication(transport);
    assert(authentication);
    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .domain = "genycaster.example",
        .siweUri = "https://genycaster.example/sign-in",
    });
    assert(started);
    assert(transport.calls().size() == 1U);
    assert(transport.calls().front().url == "https://relay.farcaster.xyz/v1/channel");
}

void quotedProviderConfigurationIsNormalizedBeforeNetworkUse()
{
    FakeTransport transport{};
    transport.enqueue(201,
        "{\"channelToken\":\"secret-token\",\"url\":\"https://farcaster.xyz/~/sign-in-with-farcaster?channelToken=secret-token\",\"nonce\":\"abcdef12\"}");

    auto authentication = provider::makeAuthentication(transport, provider::Config{
        .relayBaseUrl = "  \"https://relay.example\"  ",
        .relayVersion = " 'v1' ",
        .optimismRpcUrl = "  'https://rpc.example'  ",
    });
    assert(authentication);

    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .domain = "genycaster.example",
        .siweUri = "https://genycaster.example/sign-in",
    });
    assert(started);
    assert(transport.calls().size() == 1U);
    assert(transport.calls().front().url == "https://relay.example/v1/channel");
}

void malformedProviderBaseUrlIsRejectedBeforeNetworkUse()
{
    FakeTransport transport{};
    auto authentication = provider::makeAuthentication(transport, provider::Config{
        .relayBaseUrl = "https://relay.example bad",
        .relayVersion = "v1",
        .optimismRpcUrl = "https://rpc.example",
    });
    assert(!authentication);
    assert(authentication.error().code == foundation::error::ErrorCode::InvalidArgument);
    assert(transport.calls().empty());
}

void quotedLocalhostConfigurationIsNormalizedBeforeRelayRequest()
{
    FakeTransport transport{};
    transport.enqueue(201,
        "{\"channelToken\":\"secret-token\",\"url\":\"https://farcaster.xyz/~/sign-in-with-farcaster?channelToken=secret-token\",\"nonce\":\"abcdef12\"}");

    auto authentication = provider::makeAuthentication(transport, provider::Config{
        .relayBaseUrl = "https://relay.example",
        .relayVersion = "v1",
        .optimismRpcUrl = "https://rpc.example",
    });
    assert(authentication);
    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .domain = "  \"localhost\"  ",
        .siweUri = "  'http://localhost/login'  ",
    });
    assert(started);
    assert(transport.calls().size() == 1);
    assert(transport.calls().front().body ==
        "{\"siweUri\":\"http://localhost/login\",\"domain\":\"localhost\",\"acceptAuthAddress\":true}");
}

}

int main()
{
    verifiedCustodyFlowReturnsNormalizedSession();
    currentCustodyCanBeResolvedWithoutNewSignIn();
    activeAuthAddressIsPreferredOverCustodyFallback();
    erc6492CounterfactualSignatureUsesUniversalVerifier();
    invalidUniversalSignatureIsRejectedBeforeFidAuthorization();
    wrongFidResourceIsRejectedBeforeChainVerification();
    expiredSiwfProofIsRejectedBeforeChainVerification();
    futureNotBeforeIsRejectedBeforeChainVerification();
    relayValidationFailureKeepsSafeDiagnosticDetail();
    relayRejectsInvalidRelyingPartyAsConfigurationError();
    malformedSiwfUriIsRejectedBeforeRelayRequest();
    defaultConfigurationUsesOfficialFarcasterRelay();
    quotedProviderConfigurationIsNormalizedBeforeNetworkUse();
    malformedProviderBaseUrlIsRejectedBeforeNetworkUse();
    quotedLocalhostConfigurationIsNormalizedBeforeRelayRequest();
    return 0;
}
