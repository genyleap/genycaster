#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

import farcaster.capability.signer;
import farcaster.capability.signer_attestation;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.provider.compatibility.browser_wallet.authorizer;

namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;
using namespace farcaster;

struct ParsedUrl final {
    std::string host{};
    std::uint16_t port{};
    std::string target{};
};

[[nodiscard]] ParsedUrl parseLoopbackUrl(const std::string_view url)
{
    constexpr std::string_view prefix{"http://127.0.0.1:"};
    assert(url.starts_with(prefix));
    const auto slash = url.find('/', prefix.size());
    assert(slash != std::string_view::npos);
    const auto portText = url.substr(prefix.size(), slash - prefix.size());
    return ParsedUrl{
        .host = "127.0.0.1",
        .port = static_cast<std::uint16_t>(std::stoul(std::string{portText})),
        .target = std::string{url.substr(slash)},
    };
}

[[nodiscard]] http::response<http::string_body> request(
    const ParsedUrl& url,
    const http::verb method,
    const std::string_view target,
    std::string body = {},
    std::string contentType = {})
{
    asio::io_context context{};
    tcp::resolver resolver{context};
    beast::tcp_stream stream{context};
    auto endpoints = resolver.resolve(url.host, std::to_string(url.port));
    stream.connect(endpoints);
    http::request<http::string_body> req{method, std::string{target}, 11};
    req.set(http::field::host, url.host);
    if (!contentType.empty()) req.set(http::field::content_type, contentType);
    req.body() = std::move(body);
    req.prepare_payload();
    http::write(stream, req);
    beast::flat_buffer buffer{};
    http::response<http::string_body> response{};
    http::read(stream, buffer, response);
    beast::error_code ignored{};
    stream.socket().shutdown(tcp::socket::shutdown_both, ignored);
    return response;
}

[[nodiscard]] capability::signer::KeyAddRequest keyRequest()
{
    return capability::signer::KeyAddRequest{
        .fid = domain::identity::Fid{42},
        .publicKey = domain::signer::PublicKey{
            "1111111111111111111111111111111111111111111111111111111111111111"},
        .scopes = domain::signer::standardSocialScopes(),
        .ttlSeconds = domain::signer::DefaultGaslessTtlSeconds,
        .nonce = 7U,
        .deadline = foundation::time::Timestamp{std::chrono::seconds{4'102'444'800LL}},
        .requestFid = domain::identity::Fid{42},
        .custodyAddress = "0x1111111111111111111111111111111111111111",
    };
}

[[nodiscard]] std::string repeatedHex(const std::size_t bytes, const char pair)
{
    return std::string(bytes * 2U, pair);
}

void testApprovalRoundTrip()
{
    using provider::compatibility::browser_wallet::authorizer::Config;
    using provider::compatibility::browser_wallet::authorizer::makeAuthorizer;

    std::jthread client{};
    auto authorizer = makeAuthorizer(Config{
        .launch = [&](const std::string& approvalUrl) -> foundation::result::Status {
            const auto parsed = parseLoopbackUrl(approvalUrl);
            client = std::jthread([parsed] {
                const auto page = request(parsed, http::verb::get, parsed.target);
                assert(page.result() == http::status::ok);
                assert(page[http::field::cache_control] == "no-store, max-age=0");
                const std::string body = page.body();
                assert(body.find("eth_requestAccounts") != std::string::npos);
                assert(body.find("eth_signTypedData_v4") != std::string::npos);
                assert(body.find("Farcaster KeyAdd") != std::string::npos);
                assert(body.find("SignedKeyRequest") != std::string::npos);
                assert(body.find("KeyAdd") != std::string::npos);

                const auto callbackMarker = std::string{"const callbackPath=\""};
                const auto callbackStart = body.find(callbackMarker);
                assert(callbackStart != std::string::npos);
                const auto valueStart = callbackStart + callbackMarker.size();
                const auto valueEnd = body.find('"', valueStart);
                assert(valueEnd != std::string::npos);
                const auto callback = body.substr(valueStart, valueEnd - valueStart);

                const std::string json =
                    "{\"account\":\"0x" + repeatedHex(20U, '1') +
                    "\",\"signedKeyRequestSignature\":\"0x" + repeatedHex(65U, '2') +
                    "\",\"custodySignature\":\"0x" + repeatedHex(65U, '3') + "\"}";
                const auto posted = request(
                    parsed, http::verb::post, callback, json, "application/json; charset=utf-8");
                assert(posted.result() == http::status::ok);
            });
            return {};
        },
        .approvalTimeout = std::chrono::seconds{3},
        .requestTimeout = std::chrono::seconds{2},
        .maxRequestBytes = 16U * 1024U,
    });
    assert(authorizer);

    auto result = (*authorizer)->authorize(keyRequest());
    assert(result);
    assert(result->custodySignature.size() == 65U);
    assert(result->custodySignature == std::string(65U, static_cast<char>(0x33)));
    // `signedKeyRequestMetadata` is the ABI encoding of a single dynamic
    // tuple argument (matching the protocol fixture in tests/protocol): the
    // top-level offset word (0x20) is followed by requestFid, the padded
    // address word and the dynamic-bytes offset, then deadline and payload.
    // ABI word 2 (address) is left padded with 12 zero bytes and then 20 bytes.
    assert(result->signedKeyRequestMetadata.size() == 288U);
    assert(result->signedKeyRequestMetadata.substr(64U, 12U) == std::string(12U, '\0'));
    assert(result->signedKeyRequestMetadata.substr(76U, 20U) == std::string(20U, static_cast<char>(0x11)));
}

void testAppAttestationRequiresOnlyKeyAddWalletSignature()
{
    using provider::compatibility::browser_wallet::authorizer::Config;
    using provider::compatibility::browser_wallet::authorizer::makeAuthorizer;

    std::jthread client{};
    auto authorizer = makeAuthorizer(Config{
        .launch = [&](const std::string& approvalUrl) -> foundation::result::Status {
            const auto parsed = parseLoopbackUrl(approvalUrl);
            client = std::jthread([parsed] {
                const auto page = request(parsed, http::verb::get, parsed.target);
                assert(page.result() == http::status::ok);
                const std::string body = page.body();
                assert(body.find("const signedKeyRequest=") == std::string::npos);
                const auto firstSignature = body.find("eth_signTypedData_v4");
                assert(firstSignature != std::string::npos);
                assert(body.find("eth_signTypedData_v4", firstSignature + 1U) == std::string::npos);
                assert(body.find("No transaction or gas is required") != std::string::npos);

                const auto callbackMarker = std::string{"const callbackPath=\""};
                const auto callbackStart = body.find(callbackMarker);
                assert(callbackStart != std::string::npos);
                const auto valueStart = callbackStart + callbackMarker.size();
                const auto valueEnd = body.find('"', valueStart);
                assert(valueEnd != std::string::npos);
                const auto callback = body.substr(valueStart, valueEnd - valueStart);

                const std::string json =
                    "{\"account\":\"0x" + repeatedHex(20U, '1') +
                    "\",\"custodySignature\":\"0x" + repeatedHex(65U, '3') + "\"}";
                const auto posted = request(
                    parsed, http::verb::post, callback, json, "application/json; charset=utf-8");
                assert(posted.result() == http::status::ok);
            });
            return {};
        },
        .approvalTimeout = std::chrono::seconds{3},
        .requestTimeout = std::chrono::seconds{2},
        .maxRequestBytes = 16U * 1024U,
    });
    assert(authorizer);

    auto requestValue = keyRequest();
    requestValue.requestFid = domain::identity::Fid{314U};
    auto result = (*authorizer)->authorize(
        requestValue,
        capability::signer_attestation::SignedKeyRequestAttestation{
            .requestSignerAddress = std::string(20U, static_cast<char>(0x44)),
            .signature = std::string(65U, static_cast<char>(0x55)),
        });
    assert(result);
    assert(result->custodySignature == std::string(65U, static_cast<char>(0x33)));
    assert(result->signedKeyRequestMetadata.size() == 288U);
    assert(result->signedKeyRequestMetadata.substr(76U, 20U) ==
        std::string(20U, static_cast<char>(0x44)));
}

void testWrongWalletIsRejectedBeforeRegistration()
{
    using provider::compatibility::browser_wallet::authorizer::Config;
    using provider::compatibility::browser_wallet::authorizer::makeAuthorizer;

    std::jthread client{};
    auto authorizer = makeAuthorizer(Config{
        .launch = [&](const std::string& approvalUrl) -> foundation::result::Status {
            const auto parsed = parseLoopbackUrl(approvalUrl);
            client = std::jthread([parsed] {
                const auto page = request(parsed, http::verb::get, parsed.target);
                assert(page.result() == http::status::ok);
                assert(page.body().find("0x1111111111111111111111111111111111111111") !=
                    std::string::npos);
                const auto marker = std::string{"const callbackPath=\""};
                const auto start = page.body().find(marker) + marker.size();
                const auto end = page.body().find('"', start);
                const auto callback = page.body().substr(start, end - start);
                const std::string json =
                    "{\"account\":\"0x" + repeatedHex(20U, '2') +
                    "\",\"signedKeyRequestSignature\":\"0x" + repeatedHex(65U, '2') +
                    "\",\"custodySignature\":\"0x" + repeatedHex(65U, '3') + "\"}";
                const auto posted = request(
                    parsed, http::verb::post, callback, json, "application/json");
                assert(posted.result() == http::status::bad_request);
                assert(posted.body().find("Expected 0x1111111111111111111111111111111111111111") !=
                    std::string::npos);
            });
            return {};
        },
        .approvalTimeout = std::chrono::seconds{3},
        .requestTimeout = std::chrono::seconds{2},
        .maxRequestBytes = 16U * 1024U,
    });
    assert(authorizer);

    auto result = (*authorizer)->authorize(keyRequest());
    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::Unauthorized);
    assert(result.error().message.find("not the current Farcaster custody wallet") !=
        std::string::npos);
}

void testMalformedCallbackFailsClosed()
{
    using provider::compatibility::browser_wallet::authorizer::Config;
    using provider::compatibility::browser_wallet::authorizer::makeAuthorizer;

    std::jthread client{};
    auto authorizer = makeAuthorizer(Config{
        .launch = [&](const std::string& approvalUrl) -> foundation::result::Status {
            const auto parsed = parseLoopbackUrl(approvalUrl);
            client = std::jthread([parsed] {
                const auto page = request(parsed, http::verb::get, parsed.target);
                const auto marker = std::string{"const callbackPath=\""};
                const auto start = page.body().find(marker) + marker.size();
                const auto end = page.body().find('"', start);
                const auto callback = page.body().substr(start, end - start);
                const auto posted = request(
                    parsed,
                    http::verb::post,
                    callback,
                    R"({"account":"0x1234"})",
                    "application/json");
                assert(posted.result() == http::status::bad_request);
            });
            return {};
        },
        .approvalTimeout = std::chrono::seconds{3},
        .requestTimeout = std::chrono::seconds{2},
        .maxRequestBytes = 16U * 1024U,
    });
    assert(authorizer);
    auto result = (*authorizer)->authorize(keyRequest());
    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::ProtocolFailure);
}


void testWalletRejectionMapsToUnauthorized()
{
    using provider::compatibility::browser_wallet::authorizer::Config;
    using provider::compatibility::browser_wallet::authorizer::makeAuthorizer;

    std::jthread client{};
    auto authorizer = makeAuthorizer(Config{
        .launch = [&](const std::string& approvalUrl) -> foundation::result::Status {
            const auto parsed = parseLoopbackUrl(approvalUrl);
            client = std::jthread([parsed] {
                const auto page = request(parsed, http::verb::get, parsed.target);
                const auto marker = std::string{"const callbackPath=\""};
                const auto start = page.body().find(marker) + marker.size();
                const auto end = page.body().find('"', start);
                const auto callback = page.body().substr(start, end - start);
                const auto posted = request(
                    parsed,
                    http::verb::post,
                    callback,
                    R"({"error":"rejected"})",
                    "application/json");
                assert(posted.result() == http::status::bad_request);
            });
            return {};
        },
        .approvalTimeout = std::chrono::seconds{3},
        .requestTimeout = std::chrono::seconds{2},
        .maxRequestBytes = 16U * 1024U,
    });
    assert(authorizer);
    auto result = (*authorizer)->authorize(keyRequest());
    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::Unauthorized);
}

void testPolicyAndLauncherFailures()
{
    using provider::compatibility::browser_wallet::authorizer::Config;
    using provider::compatibility::browser_wallet::authorizer::makeAuthorizer;

    assert(!makeAuthorizer(Config{}));

    auto authorizer = makeAuthorizer(Config{
        .launch = [](const std::string&) -> foundation::result::Status {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "launcher failed"));
        },
    });
    assert(authorizer);
    auto result = (*authorizer)->authorize(keyRequest());
    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::Unavailable);

    auto requestValue = keyRequest();
    requestValue.requestFid = domain::identity::Fid{99};
    result = (*authorizer)->authorize(requestValue);
    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::Unsupported);
}

}

int main()
{
    testApprovalRoundTrip();
    testAppAttestationRequiresOnlyKeyAddWalletSignature();
    testWrongWalletIsRejectedBeforeRegistration();
    testMalformedCallbackFailsClosed();
    testWalletRejectionMapsToUnauthorized();
    testPolicyAndLauncherFailures();
    return 0;
}
