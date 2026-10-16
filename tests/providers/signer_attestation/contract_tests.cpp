#include <cassert>
#include <chrono>
#include <expected>
#include <optional>
#include <string>

import farcaster.capability.signer;
import farcaster.capability.signer_attestation;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.provider.compatibility.signer_attestation;
import farcaster.transport.http;

namespace {

namespace capability = farcaster::capability;
namespace domain = farcaster::domain;
namespace foundation = farcaster::foundation;
namespace provider = farcaster::provider;
namespace transport = farcaster::transport;

class FakeTransport final : public transport::http::IHttpTransport {
public:
    transport::http::HttpResponse response{
        .statusCode = 200U,
        .body =
            R"({"requestFid":"314","publicKey":"0x0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","deadline":123,"requestSignerAddress":"0x1111111111111111111111111111111111111111","signedKeyRequestSignature":"0x2222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222"})",
    };
    std::optional<transport::http::HttpPostRequest> lastPost{};

    [[nodiscard]] foundation::result::Result<transport::http::HttpResponse>
    get(const transport::http::HttpRequest&) override
    {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Unsupported,
            "Signer attestation tests do not issue GET requests."));
    }

    [[nodiscard]] foundation::result::Result<transport::http::HttpResponse>
    post(const transport::http::HttpPostRequest& request) override
    {
        lastPost = request;
        return response;
    }
};

[[nodiscard]] capability::signer::KeyAddRequest request()
{
    using namespace std::chrono;
    return capability::signer::KeyAddRequest{
        .fid = domain::identity::Fid{42U},
        .publicKey = domain::signer::PublicKey{
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"},
        .scopes = domain::signer::standardSocialScopes(),
        .ttlSeconds = domain::signer::DefaultGaslessTtlSeconds,
        .nonce = 7U,
        .deadline = foundation::time::Timestamp{
            duration_cast<milliseconds>(seconds{1'609'459'200LL + 123LL})},
        .requestFid = domain::identity::Fid{314U},
    };
}

void successfulResponseIsBoundToExactRequest()
{
    FakeTransport transport{};
    auto attestor = provider::compatibility::signer_attestation::makeAttestor(
        transport,
        provider::compatibility::signer_attestation::Config{
            .endpointUrl = "https://auth.genycaster.example/v1/signer/request",
        });
    assert(attestor);
    const auto keyRequest = request();
    auto result = (*attestor)->attest(
        keyRequest.requestFid.value, keyRequest.publicKey.hex, keyRequest.deadline);
    assert(result);
    assert(result->requestSignerAddress == std::string(20U, static_cast<char>(0x11)));
    assert(result->signature == std::string(65U, static_cast<char>(0x22)));
    assert(transport.lastPost);
    assert(transport.lastPost->url == "https://auth.genycaster.example/v1/signer/request");
    assert(transport.lastPost->body.find(R"("requestFid":"314")") != std::string::npos);
    assert(transport.lastPost->body.find(R"("deadline":123)") != std::string::npos);
    assert(transport.lastPost->body.find("custodySignature") == std::string::npos);
}

void responseMismatchFailsClosed()
{
    FakeTransport transport{};
    transport.response.body =
        R"({"requestFid":"315","publicKey":"0x0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","deadline":123,"requestSignerAddress":"0x1111111111111111111111111111111111111111","signedKeyRequestSignature":"0x2222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222"})";
    auto attestor = provider::compatibility::signer_attestation::makeAttestor(
        transport,
        provider::compatibility::signer_attestation::Config{
            .endpointUrl = "https://auth.genycaster.example/v1/signer/request",
        });
    assert(attestor);
    const auto keyRequest = request();
    auto result = (*attestor)->attest(
        keyRequest.requestFid.value, keyRequest.publicKey.hex, keyRequest.deadline);
    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::Conflict);
}

void endpointPolicyAndServerFailuresAreExplicit()
{
    FakeTransport transport{};
    auto insecure = provider::compatibility::signer_attestation::makeAttestor(
        transport,
        provider::compatibility::signer_attestation::Config{
            .endpointUrl = "http://example.com/v1/signer/request",
        });
    assert(!insecure);
    assert(insecure.error().code == foundation::error::ErrorCode::InvalidArgument);

    transport.response = transport::http::HttpResponse{
        .statusCode = 503U,
        .body = "temporarily unavailable",
    };
    auto attestor = provider::compatibility::signer_attestation::makeAttestor(
        transport,
        provider::compatibility::signer_attestation::Config{
            .endpointUrl = "http://127.0.0.1:8080/v1/signer/request",
        });
    assert(attestor);
    const auto keyRequest = request();
    auto result = (*attestor)->attest(
        keyRequest.requestFid.value, keyRequest.publicKey.hex, keyRequest.deadline);
    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::Unavailable);

    // Snapchain's read/write gateway intentionally has no app signing route.
    // HTTP 404 is missing endpoint configuration, NOT a contradicted signer key.
    transport.response = transport::http::HttpResponse{
        .statusCode = 404U,
        .body = R"({"error":"route_not_available"})",
    };
    auto missing = (*attestor)->attest(
        keyRequest.requestFid.value, keyRequest.publicKey.hex, keyRequest.deadline);
    assert(!missing);
    assert(missing.error().code == foundation::error::ErrorCode::NotFound);
}

} // namespace

int main()
{
    successfulResponseIsBoundToExactRequest();
    responseMismatchFailsClosed();
    endpointPolicyAndServerFailuresAreExplicit();
    return 0;
}
