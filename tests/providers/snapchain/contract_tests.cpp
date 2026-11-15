#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

import farcaster.capability.signer;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.provider.snapchain.signer;
import farcaster.transport.http;

namespace {

using namespace farcaster;

class FakeTransport final : public transport::http::IHttpTransport {
public:
    struct Reply final {
        std::uint16_t statusCode{};
        std::string body{};
    };

    void enqueue(const std::uint16_t statusCode, std::string body)
    {
        m_replies.push_back(Reply{.statusCode = statusCode, .body = std::move(body)});
    }

    [[nodiscard]] foundation::result::Result<transport::http::HttpResponse> get(
        const transport::http::HttpRequest& request) override
    {
        m_urls.push_back(request.url);
        m_headers.push_back(request.headers);
        m_timeouts.push_back(request.timeout);
        m_maxBodyBytes.push_back(request.maxBodyBytes);
        if (m_index >= m_replies.size()) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::TransportFailure,
                "No fake Snapchain response queued."));
        }
        const Reply& reply = m_replies[m_index++];
        return transport::http::HttpResponse{.statusCode = reply.statusCode, .body = reply.body};
    }

    [[nodiscard]] foundation::result::Result<transport::http::HttpResponse> post(
        const transport::http::HttpPostRequest& request) override
    {
        m_posts.push_back(request);
        if (m_index >= m_replies.size()) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::TransportFailure,
                "No fake Snapchain response queued."));
        }
        const Reply& reply = m_replies[m_index++];
        return transport::http::HttpResponse{.statusCode = reply.statusCode, .body = reply.body};
    }

    [[nodiscard]] const std::vector<std::string>& urls() const noexcept { return m_urls; }
    [[nodiscard]] const std::vector<std::vector<std::string>>& headers() const noexcept { return m_headers; }
    [[nodiscard]] const std::vector<std::chrono::milliseconds>& timeouts() const noexcept { return m_timeouts; }
    [[nodiscard]] const std::vector<std::size_t>& maxBodyBytes() const noexcept { return m_maxBodyBytes; }
    [[nodiscard]] const std::vector<transport::http::HttpPostRequest>& posts() const noexcept { return m_posts; }

private:
    std::vector<Reply> m_replies{};
    std::size_t m_index{};
    std::vector<std::string> m_urls{};
    std::vector<std::vector<std::string>> m_headers{};
    std::vector<std::chrono::milliseconds> m_timeouts{};
    std::vector<std::size_t> m_maxBodyBytes{};
    std::vector<transport::http::HttpPostRequest> m_posts{};
};

[[nodiscard]] provider::snapchain::signer::Config testConfig()
{
    return provider::snapchain::signer::Config{
        .baseUrl = "https://snapchain.test///",
        .timeout = std::chrono::milliseconds{4'000},
        .maxBodyBytes = 512U * 1024U,
        .confirmationTimeout = std::chrono::milliseconds{100},
        .confirmationPollInterval = std::chrono::milliseconds{1},
    };
}

[[nodiscard]] std::string keyA()
{
    return "1111111111111111111111111111111111111111111111111111111111111111";
}

[[nodiscard]] std::string keyB()
{
    return "2222222222222222222222222222222222222222222222222222222222222222";
}

class FakeSigner final : public capability::signer::INativeSigner {
public:
    explicit FakeSigner(std::string publicKey = keyA())
        : m_publicKey(std::move(publicKey))
    {
    }

    [[nodiscard]] domain::signer::SignerId signerId() const override
    {
        return domain::signer::SignerId{"fake"};
    }

    [[nodiscard]] foundation::result::Result<domain::signer::PublicKey> publicKey() const override
    {
        return domain::signer::PublicKey{m_publicKey};
    }

    [[nodiscard]] foundation::result::Result<domain::signer::Signature> sign(
        const std::string& bytes) override
    {
        m_signedBytes.push_back(bytes);
        return domain::signer::Signature{std::string(64U, static_cast<char>(0x5A))};
    }

    [[nodiscard]] const std::vector<std::string>& signedBytes() const noexcept
    {
        return m_signedBytes;
    }

private:
    std::string m_publicKey{};
    std::vector<std::string> m_signedBytes{};
};

void testGaslessSignerAndNonceMapping()
{
    FakeTransport transport{};
    transport.enqueue(200, R"({
        "signers":[
            {
                "source":"SIGNER_SOURCE_OFFCHAIN",
                "key":"0x1111111111111111111111111111111111111111111111111111111111111111",
                "keyType":1,
                "fid":42,
                "addedAt":1787900000,
                "lastUsedAt":4102440000,
                "ttl":2592000,
                "expiresAt":4105032000,
                "scopes":[
                    "MESSAGE_TYPE_CAST_ADD",
                    "MESSAGE_TYPE_CAST_REMOVE",
                    "MESSAGE_TYPE_REACTION_ADD",
                    "MESSAGE_TYPE_REACTION_REMOVE",
                    "MESSAGE_TYPE_LINK_ADD",
                    "MESSAGE_TYPE_LINK_REMOVE",
                    "MESSAGE_TYPE_USER_DATA_ADD"
                ],
                "requestFid":42,
                "nonce":7
            }
        ],
        "gaslessSignerCount":1,
        "gaslessSignerLimit":1000,
        "currentUserNonce":7,
        "requesterFidNonces":{"42":3}
    })");

    auto registry = provider::snapchain::signer::makeRegistry(transport, testConfig());
    auto result = registry->inspect(
        domain::identity::Fid{42},
        domain::signer::PublicKey{keyA()},
        domain::identity::Fid{42});

    assert(result);
    assert(result->nonces.currentUserNonce == 7U);
    assert(result->nonces.currentRequesterNonce == 3U);
    assert(result->registration);
    assert(result->registration->kind == domain::signer::RegistrationKind::Gasless);
    assert(result->registration->state == domain::signer::RegistrationState::Active);
    assert(result->registration->publicKey.hex == keyA());
    assert(result->registration->ttlSeconds == 2'592'000U);
    assert(result->registration->requestFid);
    assert(result->registration->requestFid->value == 42U);
    assert(result->registration->scopes == domain::signer::standardSocialScopes());
    assert(transport.urls().size() == 1U);
    assert(transport.urls().front() ==
        "https://snapchain.test/v1/signersByFid?fid=42&pageSize=1000&requesterFids[0]=42");
    assert(transport.timeouts().front() == std::chrono::milliseconds{4'000});
    assert(transport.maxBodyBytes().front() == 512U * 1024U);
}


void testProtoJsonUint64SignerIdentifiers()
{
    // ProtoJSON returns uint64 identifiers as decimal strings. Keep legacy
    // numeric JSON responses working, but never round native FIDs through a
    // floating-point representation or accept malformed/overflowed strings.
    auto response = [](const std::uint64_t expectedFid,
                       const std::string_view fidJson,
                       const std::string_view requestFidJson) {
        std::string body = R"({"signers":[{"source":"SIGNER_SOURCE_OFFCHAIN",)"
            R"("key":"0x1111111111111111111111111111111111111111111111111111111111111111",)"
            R"("keyType":1,"fid":)";
        body.append(fidJson);
        body += R"(,"lastUsedAt":"4102440000","ttl":2592000,)"
                R"("expiresAt":"4105032000","scopes":["MESSAGE_TYPE_CAST_ADD"],"requestFid":)";
        body.append(requestFidJson);
        body += R"(}],"currentUserNonce":3,"requesterFidNonces":{")";
        body += std::to_string(expectedFid);
        body += R"(":2}})";
        return body;
    };

    constexpr std::uint64_t NativeFid = 4'294'967'296ULL;
    constexpr std::uint64_t ExactFidOverJsSafeInteger = 9'007'199'254'740'993ULL;
    for (const auto& [fid, fidJson, requestFidJson] : {
             std::tuple{NativeFid, std::string_view{"\"4294967296\""},
                        std::string_view{"\"4294967296\""}},
             std::tuple{NativeFid, std::string_view{"4294967296"},
                        std::string_view{"\"4294967296\""}},
             std::tuple{ExactFidOverJsSafeInteger,
                        std::string_view{"\"9007199254740993\""},
                        std::string_view{"\"9007199254740993\""}}
         }) {
        FakeTransport transport{};
        transport.enqueue(200, response(fid, fidJson, requestFidJson));
        auto registry = provider::snapchain::signer::makeRegistry(transport, testConfig());
        auto result = registry->inspect(
            domain::identity::Fid{fid}, domain::signer::PublicKey{keyA()},
            domain::identity::Fid{fid});
        assert(result && result->registration);
        assert(result->registration->fid.value == fid);
        assert(result->registration->requestFid);
        assert(result->registration->requestFid->value == fid);
        assert(result->registration->state == domain::signer::RegistrationState::Active);
        assert(result->nonces.currentUserNonce == 3U);
        assert(result->nonces.currentRequesterNonce == 2U);
        assert(transport.posts().empty());
    }
    // No signs, exponents, decimal points, spaces or out-of-range uint64s.
    for (const std::string_view invalid : {
             "\"-1\"", "\"+4294967296\"", "\"4e9\"",
             "\"4294967296.0\"", "\" 4294967296\"",
             "\"4294967296x\"", "\"18446744073709551616\"",
             "4294967296.0", "null"
         }) {
        FakeTransport transport{};
        transport.enqueue(200, response(NativeFid, invalid, "\"4294967296\""));
        auto registry = provider::snapchain::signer::makeRegistry(transport, testConfig());
        auto result = registry->inspect(
            domain::identity::Fid{NativeFid},
            domain::signer::PublicKey{keyA()},
            domain::identity::Fid{NativeFid});
        assert(!result);
        assert(result.error().code == foundation::error::ErrorCode::ProtocolFailure);
        assert(transport.posts().empty());
    }
}

void testPaginationAndGaslessPreference()
{
    FakeTransport transport{};
    transport.enqueue(200, R"({
        "signers":[{
            "source":"SIGNER_SOURCE_ONCHAIN",
            "key":"0x1111111111111111111111111111111111111111111111111111111111111111",
            "keyType":1,
            "fid":42,
            "scopes":[]
        }],
        "nextPageToken":"cursor+/=",
        "gaslessSignerCount":1,
        "gaslessSignerLimit":1000,
        "currentUserNonce":8,
        "requesterFidNonces":{"99":4}
    })");
    transport.enqueue(200, R"({
        "signers":[{
            "source":"SIGNER_SOURCE_OFFCHAIN",
            "key":"0x1111111111111111111111111111111111111111111111111111111111111111",
            "keyType":1,
            "fid":42,
            "lastUsedAt":4102440000,
            "ttl":3600,
            "expiresAt":4102443600,
            "scopes":["MESSAGE_TYPE_CAST_ADD"],
            "requestFid":99,
            "nonce":8
        }],
        "gaslessSignerCount":1,
        "gaslessSignerLimit":1000,
        "currentUserNonce":9,
        "requesterFidNonces":{"99":6}
    })");

    auto registry = provider::snapchain::signer::makeRegistry(transport, testConfig());
    auto result = registry->inspect(
        domain::identity::Fid{42},
        domain::signer::PublicKey{keyA()},
        domain::identity::Fid{99});

    assert(result && result->registration);
    assert(result->registration->kind == domain::signer::RegistrationKind::Gasless);
    assert(result->nonces.currentUserNonce == 9U);
    assert(result->nonces.currentRequesterNonce == 6U);
    assert(transport.urls().size() == 2U);
    assert(transport.urls().back().find("pageToken=cursor%2B%2F%3D") != std::string::npos);
}

void testMissingSignerStillReturnsReplayState()
{
    FakeTransport transport{};
    transport.enqueue(200, R"({
        "signers":[{
            "source":"SIGNER_SOURCE_ONCHAIN",
            "key":"0x2222222222222222222222222222222222222222222222222222222222222222",
            "keyType":1,
            "fid":42,
            "scopes":[]
        }],
        "gaslessSignerCount":0,
        "gaslessSignerLimit":1000,
        "currentUserNonce":12,
        "requesterFidNonces":{"42":5}
    })");

    auto registry = provider::snapchain::signer::makeRegistry(transport, testConfig());
    auto result = registry->inspect(
        domain::identity::Fid{42},
        domain::signer::PublicKey{keyA()},
        domain::identity::Fid{42});

    assert(result);
    assert(!result->registration);
    assert(result->nonces.currentUserNonce == 12U);
    assert(result->nonces.currentRequesterNonce == 5U);
}

void testUnknownScopeAndMalformedFieldsFailClosed()
{
    FakeTransport unknownScopeTransport{};
    unknownScopeTransport.enqueue(200, R"({
        "signers":[{
            "source":"SIGNER_SOURCE_OFFCHAIN",
            "key":"0x1111111111111111111111111111111111111111111111111111111111111111",
            "keyType":1,
            "fid":42,
            "lastUsedAt":4102440000,
            "ttl":3600,
            "expiresAt":4102443600,
            "scopes":["MESSAGE_TYPE_KEY_ADD"],
            "requestFid":42
        }],
        "gaslessSignerCount":1,
        "gaslessSignerLimit":1000,
        "currentUserNonce":1,
        "requesterFidNonces":{"42":0}
    })");
    auto unknownScopeRegistry = provider::snapchain::signer::makeRegistry(unknownScopeTransport, testConfig());
    auto unknownScope = unknownScopeRegistry->inspect(
        domain::identity::Fid{42},
        domain::signer::PublicKey{keyA()},
        domain::identity::Fid{42});
    assert(!unknownScope);
    assert(unknownScope.error().code == foundation::error::ErrorCode::ProtocolFailure);

    FakeTransport expiryTransport{};
    expiryTransport.enqueue(200, R"({
        "signers":[{
            "source":"SIGNER_SOURCE_OFFCHAIN",
            "key":"0x1111111111111111111111111111111111111111111111111111111111111111",
            "keyType":1,
            "fid":42,
            "lastUsedAt":4102440000,
            "ttl":3600,
            "expiresAt":4102443601,
            "scopes":["MESSAGE_TYPE_CAST_ADD"],
            "requestFid":42
        }],
        "gaslessSignerCount":1,
        "gaslessSignerLimit":1000,
        "currentUserNonce":1,
        "requesterFidNonces":{"42":0}
    })");
    auto expiryRegistry = provider::snapchain::signer::makeRegistry(expiryTransport, testConfig());
    auto badExpiry = expiryRegistry->inspect(
        domain::identity::Fid{42},
        domain::signer::PublicKey{keyA()},
        domain::identity::Fid{42});
    assert(!badExpiry);
    assert(badExpiry.error().code == foundation::error::ErrorCode::ProtocolFailure);
}

void testConfigurationAndHttpErrors()
{
    const auto defaultConfig = provider::snapchain::signer::Config{};
    assert(defaultConfig.baseUrl.empty());
    assert(defaultConfig.authorizationHeaderName.empty());
    assert(defaultConfig.authorizationHeaderValue.empty());

    FakeTransport missingOriginTransport{};
    auto missingOriginRegistry = provider::snapchain::signer::makeRegistry(
        missingOriginTransport, defaultConfig);
    auto missingOrigin = missingOriginRegistry->inspect(
        domain::identity::Fid{42},
        domain::signer::PublicKey{keyA()},
        std::nullopt);
    assert(!missingOrigin);
    assert(missingOrigin.error().code == foundation::error::ErrorCode::InvalidArgument);
    assert(missingOriginTransport.urls().empty());

    FakeTransport authenticatedTransport{};
    authenticatedTransport.enqueue(200, R"({
        "signers":[],
        "currentUserNonce":0
    })");
    auto authenticatedConfig = testConfig();
    authenticatedConfig.authorizationHeaderName = "X-Upstream-Credential";
    authenticatedConfig.authorizationHeaderValue = "test-secret";
    auto authenticatedRegistry = provider::snapchain::signer::makeRegistry(
        authenticatedTransport, authenticatedConfig);
    auto authenticated = authenticatedRegistry->inspect(
        domain::identity::Fid{42},
        domain::signer::PublicKey{keyA()},
        std::nullopt);
    assert(authenticated);
    assert(authenticatedTransport.headers().size() == 1U);
    assert(std::ranges::find(
        authenticatedTransport.headers().front(),
        std::string{"X-Upstream-Credential: test-secret"}) != authenticatedTransport.headers().front().end());

    FakeTransport incompleteCredentialTransport{};
    auto incompleteCredentialConfig = testConfig();
    incompleteCredentialConfig.authorizationHeaderName = "X-Upstream-Credential";
    auto incompleteCredentialRegistry = provider::snapchain::signer::makeRegistry(
        incompleteCredentialTransport, incompleteCredentialConfig);
    auto incompleteCredential = incompleteCredentialRegistry->inspect(
        domain::identity::Fid{42},
        domain::signer::PublicKey{keyA()},
        std::nullopt);
    assert(!incompleteCredential);
    assert(incompleteCredential.error().code == foundation::error::ErrorCode::InvalidArgument);
    assert(incompleteCredentialTransport.urls().empty());

    FakeTransport htmlTransport{};
    htmlTransport.enqueue(200, "<!doctype html><html><body>docs</body></html>");
    auto htmlRegistry = provider::snapchain::signer::makeRegistry(htmlTransport, testConfig());
    auto html = htmlRegistry->inspect(
        domain::identity::Fid{42},
        domain::signer::PublicKey{keyA()},
        std::nullopt);
    assert(!html);
    assert(html.error().message.find("returned HTML instead of JSON") != std::string::npos);

    FakeTransport transport{};
    auto config = testConfig();
    config.baseUrl.clear();
    auto invalidRegistry = provider::snapchain::signer::makeRegistry(transport, config);
    auto invalid = invalidRegistry->inspect(
        domain::identity::Fid{42},
        domain::signer::PublicKey{keyA()},
        std::nullopt);
    assert(!invalid);
    assert(invalid.error().code == foundation::error::ErrorCode::InvalidArgument);

    FakeTransport badValidatorTransport{};
    auto badValidatorConfig = testConfig();
    badValidatorConfig.submitBaseUrl = "javascript:invalid";
    auto badValidatorRegistry = provider::snapchain::signer::makeRegistry(
        badValidatorTransport, badValidatorConfig);
    auto badValidator = badValidatorRegistry->inspect(
        domain::identity::Fid{42}, domain::signer::PublicKey{keyA()}, std::nullopt);
    assert(!badValidator);
    assert(badValidator.error().code == foundation::error::ErrorCode::InvalidArgument);
    assert(badValidatorTransport.urls().empty());

    FakeTransport unavailableTransport{};
    unavailableTransport.enqueue(503, R"({"error":"temporarily unavailable"})");
    auto unavailableRegistry = provider::snapchain::signer::makeRegistry(unavailableTransport, testConfig());
    auto unavailable = unavailableRegistry->inspect(
        domain::identity::Fid{42},
        domain::signer::PublicKey{keyA()},
        std::nullopt);
    assert(!unavailable);
    assert(unavailable.error().code == foundation::error::ErrorCode::Unavailable);
}

void testGaslessRegistrationSubmitsCanonicalEnvelopeAndReReadsNetwork()
{
    FakeTransport transport{};
    transport.enqueue(200, R"({"accepted":true})");
    transport.enqueue(200, R"({
        "signers":[{
            "source":"SIGNER_SOURCE_OFFCHAIN",
            "key":"0x1111111111111111111111111111111111111111111111111111111111111111",
            "keyType":1,
            "fid":42,
            "lastUsedAt":4102440000,
            "ttl":3600,
            "expiresAt":4102443600,
            "scopes":["MESSAGE_TYPE_CAST_ADD"],
            "requestFid":42,
            "nonce":7
        }],
        "gaslessSignerCount":1,
        "gaslessSignerLimit":1000,
        "currentUserNonce":7,
        "requesterFidNonces":{"42":0}
    })");

    auto splitRouteConfig = testConfig();
    splitRouteConfig.submitBaseUrl = "https://validator.test///";
    splitRouteConfig.authorizationHeaderName = "X-Read-Token";
    splitRouteConfig.authorizationHeaderValue = "read-test-value";
    splitRouteConfig.submitAuthorizationHeaderName = "X-Write-Token";
    splitRouteConfig.submitAuthorizationHeaderValue = "write-test-value";
    auto registry = provider::snapchain::signer::makeRegistry(transport, splitRouteConfig);
    FakeSigner signer{};
    const auto deadline = foundation::time::now() + std::chrono::hours{1};
    auto result = registry->registerGasless(
        capability::signer::KeyAddRequest{
            .fid = domain::identity::Fid{42},
            .publicKey = domain::signer::PublicKey{keyA()},
            .scopes = {domain::signer::Scope::CastAdd},
            .ttlSeconds = 3'600U,
            .nonce = 7U,
            .deadline = deadline,
            .requestFid = domain::identity::Fid{42},
        },
        capability::signer::KeyAddAuthorization{
            .custodySignature = std::string(65U, static_cast<char>(0x11)),
            .signedKeyRequestMetadata = std::string(256U, static_cast<char>(0x22)),
        },
        signer);

    assert(result);
    assert(result->kind == domain::signer::RegistrationKind::Gasless);
    assert(result->state == domain::signer::RegistrationState::Active);
    assert(result->publicKey.hex == keyA());
    assert(signer.signedBytes().size() == 1U);
    assert(signer.signedBytes().front().size() == 20U);
    assert(transport.posts().size() == 1U);
    const auto& post = transport.posts().front();
    assert(post.url == "https://validator.test/v1/submitMessage");
    assert(!post.body.empty());
    assert(std::ranges::find(post.headers, std::string{"Content-Type: application/octet-stream"}) != post.headers.end());
    assert(std::ranges::find(post.headers, std::string{"Accept: application/json"}) != post.headers.end());
    assert(std::ranges::find(post.headers, std::string{"X-Read-Token: read-test-value"}) == post.headers.end());
    assert(std::ranges::find(post.headers, std::string{"X-Write-Token: write-test-value"}) != post.headers.end());
    assert(post.timeout == std::chrono::milliseconds{4'000});
    assert(post.maxBodyBytes == 512U * 1024U);
    assert(transport.urls().size() == 1U);
    assert(transport.urls().front().starts_with("https://snapchain.test/v1/signersByFid?"));
    assert(transport.headers().size() == 1U);
    assert(std::ranges::find(transport.headers().front(),
        std::string{"X-Read-Token: read-test-value"}) != transport.headers().front().end());
    assert(std::ranges::find(transport.headers().front(),
        std::string{"X-Write-Token: write-test-value"}) == transport.headers().front().end());
}

void testGaslessRegistrationPollsUntilSignerIsObservable()
{
    FakeTransport transport{};
    transport.enqueue(200, R"({"accepted":true})");
    transport.enqueue(200, R"({
        "signers":[],
        "gaslessSignerCount":0,
        "gaslessSignerLimit":1000,
        "currentUserNonce":6,
        "requesterFidNonces":{"42":0}
    })");
    transport.enqueue(200, R"({
        "signers":[{
            "source":"SIGNER_SOURCE_OFFCHAIN",
            "key":"0x1111111111111111111111111111111111111111111111111111111111111111",
            "keyType":1,
            "fid":42,
            "lastUsedAt":4102440000,
            "ttl":3600,
            "expiresAt":4102443600,
            "scopes":["MESSAGE_TYPE_CAST_ADD"],
            "requestFid":42,
            "nonce":7
        }],
        "gaslessSignerCount":1,
        "gaslessSignerLimit":1000,
        "currentUserNonce":7,
        "requesterFidNonces":{"42":0}
    })");

    auto registry = provider::snapchain::signer::makeRegistry(transport, testConfig());
    FakeSigner signer{};
    auto result = registry->registerGasless(
        capability::signer::KeyAddRequest{
            .fid = domain::identity::Fid{42},
            .publicKey = domain::signer::PublicKey{keyA()},
            .scopes = {domain::signer::Scope::CastAdd},
            .ttlSeconds = 3'600U,
            .nonce = 7U,
            .deadline = foundation::time::now() + std::chrono::hours{1},
            .requestFid = domain::identity::Fid{42},
        },
        capability::signer::KeyAddAuthorization{
            .custodySignature = std::string(65U, static_cast<char>(0x11)),
            .signedKeyRequestMetadata = std::string(256U, static_cast<char>(0x22)),
        },
        signer);

    assert(result);
    assert(transport.posts().size() == 1U);
    assert(transport.urls().size() == 2U);
}

void testGaslessRegistrationWaitsForNonceAndUsesUnorderedScopeSet()
{
    FakeTransport transport{};
    transport.enqueue(200, R"({"accepted":true})");
    // A previously observed signer may remain visible after KEY_ADD was
    // accepted, even if the old registration has a different scope set.
    transport.enqueue(200, R"({
        "signers":[{
            "source":"SIGNER_SOURCE_OFFCHAIN",
            "key":"0x1111111111111111111111111111111111111111111111111111111111111111",
            "keyType":1,"fid":42,"lastUsedAt":4102440000,"ttl":3600,
            "expiresAt":4102443600,"scopes":["MESSAGE_TYPE_CAST_ADD"],
            "requestFid":42,"nonce":6
        }],
        "currentUserNonce":6,"requesterFidNonces":{"42":0}
    })");
    // A hub is free to return the set in a different order than our
    // canonical encoder. Exact membership and the nonce are what matter.
    transport.enqueue(200, R"({
        "signers":[{
            "source":"SIGNER_SOURCE_OFFCHAIN",
            "key":"0x1111111111111111111111111111111111111111111111111111111111111111",
            "keyType":1,"fid":42,"lastUsedAt":4102440000,"ttl":3600,
            "expiresAt":4102443600,
            "scopes":["MESSAGE_TYPE_REACTION_ADD","MESSAGE_TYPE_CAST_ADD"],
            "requestFid":42,"nonce":7
        }],
        "currentUserNonce":7,"requesterFidNonces":{"42":0}
    })");
    auto config = testConfig();
    config.confirmationPollInterval = std::chrono::milliseconds{1};
    auto registry = provider::snapchain::signer::makeRegistry(transport, config);
    FakeSigner signer{};
    const auto result = registry->registerGasless(
        capability::signer::KeyAddRequest{
            .fid = domain::identity::Fid{42},
            .publicKey = domain::signer::PublicKey{keyA()},
            .scopes = {domain::signer::Scope::CastAdd,
                       domain::signer::Scope::ReactionAdd},
            .ttlSeconds = 3'600U,
            .nonce = 7U,
            .deadline = foundation::time::now() + std::chrono::hours{1},
            .requestFid = domain::identity::Fid{42},
        },
        capability::signer::KeyAddAuthorization{
            .custodySignature = std::string(65U, static_cast<char>(0x11)),
            .signedKeyRequestMetadata = std::string(256U, static_cast<char>(0x22)),
        },
        signer);
    assert(result);
    assert(result->scopes.size() == 2U);
    assert(transport.urls().size() == 2U);
}

void testSelfRevokeSubmitsCanonicalEnvelopeAndVerifiesRemoval()
{
    FakeTransport transport{};
    transport.enqueue(200, R"({"accepted":true})");
    transport.enqueue(200, R"({
        "signers":[],
        "gaslessSignerCount":0,
        "gaslessSignerLimit":1000,
        "currentUserNonce":7,
        "requesterFidNonces":{"42":9}
    })");

    auto readOnlyConfig = testConfig();
    readOnlyConfig.submitBaseUrl = "https://validator.test";
    readOnlyConfig.authorizationHeaderName = "X-Read-Token";
    readOnlyConfig.authorizationHeaderValue = "read-test-value";
    auto registry = provider::snapchain::signer::makeRegistry(transport, readOnlyConfig);
    FakeSigner signer{};
    const auto result = registry->selfRevoke(
        domain::identity::Fid{42},
        domain::identity::Fid{42},
        9U,
        foundation::time::now() + std::chrono::hours{1},
        signer);

    assert(result);
    assert(signer.signedBytes().size() == 1U);
    assert(signer.signedBytes().front().size() == 20U);
    assert(transport.posts().size() == 1U);
    assert(transport.posts().front().url == "https://validator.test/v1/submitMessage");
    assert(std::ranges::find(transport.posts().front().headers,
        std::string{"X-Read-Token: read-test-value"}) == transport.posts().front().headers.end());
    assert(!transport.posts().front().body.empty());
    assert(transport.urls().size() == 1U);
}

void testSelfRevokePollsUntilNetworkConfirmsRemoval()
{
    FakeTransport transport{};
    transport.enqueue(200, R"({"accepted":true})");
    // The first read may still expose the active key and previous nonce.
    transport.enqueue(200, R"({
        "signers":[{
            "source":"SIGNER_SOURCE_OFFCHAIN",
            "key":"0x1111111111111111111111111111111111111111111111111111111111111111",
            "keyType":1,
            "fid":42,
            "lastUsedAt":4102440000,
            "ttl":3600,
            "expiresAt":4102443600,
            "scopes":["MESSAGE_TYPE_CAST_ADD"],
            "requestFid":42,
            "nonce":7
        }],
        "gaslessSignerCount":1,
        "gaslessSignerLimit":1000,
        "currentUserNonce":7,
        "requesterFidNonces":{"42":8}
    })");
    transport.enqueue(200, R"({
        "signers":[],
        "gaslessSignerCount":0,
        "gaslessSignerLimit":1000,
        "currentUserNonce":7,
        "requesterFidNonces":{"42":9}
    })");

    auto config = testConfig();
    config.confirmationPollInterval = std::chrono::milliseconds{1};
    auto registry = provider::snapchain::signer::makeRegistry(transport, config);
    FakeSigner signer{};
    const auto result = registry->selfRevoke(
        domain::identity::Fid{42},
        domain::identity::Fid{42},
        9U,
        foundation::time::now() + std::chrono::hours{1},
        signer);
    assert(result);
    assert(transport.posts().size() == 1U);
    assert(transport.urls().size() == 2U);
}

void testWriteRejectsSignerMismatchBeforeTransport()
{
    FakeTransport transport{};
    auto registry = provider::snapchain::signer::makeRegistry(transport, testConfig());
    FakeSigner wrongSigner{keyB()};

    auto result = registry->registerGasless(
        capability::signer::KeyAddRequest{
            .fid = domain::identity::Fid{42},
            .publicKey = domain::signer::PublicKey{keyA()},
            .scopes = {domain::signer::Scope::CastAdd},
            .ttlSeconds = 3'600U,
            .nonce = 1U,
            .deadline = foundation::time::now() + std::chrono::hours{1},
            .requestFid = domain::identity::Fid{42},
        },
        capability::signer::KeyAddAuthorization{
            .custodySignature = std::string(65U, '\0'),
            .signedKeyRequestMetadata = std::string(256U, '\0'),
        },
        wrongSigner);

    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::Conflict);
    assert(transport.posts().empty());
    assert(wrongSigner.signedBytes().empty());
}

void testWriteMapsSubmitFailureAndDoesNotClaimNetworkSuccess()
{
    FakeTransport transport{};
    transport.enqueue(409, R"({"error":"nonce is not greater than stored nonce"})");
    auto registry = provider::snapchain::signer::makeRegistry(transport, testConfig());
    FakeSigner signer{};

    auto result = registry->registerGasless(
        capability::signer::KeyAddRequest{
            .fid = domain::identity::Fid{42},
            .publicKey = domain::signer::PublicKey{keyA()},
            .scopes = {domain::signer::Scope::CastAdd},
            .ttlSeconds = 3'600U,
            .nonce = 7U,
            .deadline = foundation::time::now() + std::chrono::hours{1},
            .requestFid = domain::identity::Fid{42},
        },
        capability::signer::KeyAddAuthorization{
            .custodySignature = std::string(65U, '\0'),
            .signedKeyRequestMetadata = std::string(256U, '\0'),
        },
        signer);

    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::Conflict);
    assert(transport.posts().size() == 1U);
    assert(transport.urls().empty());
}

}

int main()
{
    testGaslessSignerAndNonceMapping();
    testProtoJsonUint64SignerIdentifiers();
    testPaginationAndGaslessPreference();
    testMissingSignerStillReturnsReplayState();
    testUnknownScopeAndMalformedFieldsFailClosed();
    testConfigurationAndHttpErrors();
    testGaslessRegistrationSubmitsCanonicalEnvelopeAndReReadsNetwork();
    testGaslessRegistrationPollsUntilSignerIsObservable();
    testGaslessRegistrationWaitsForNonceAndUsesUnorderedScopeSet();
    testSelfRevokeSubmitsCanonicalEnvelopeAndVerifiesRemoval();
    testSelfRevokePollsUntilNetworkConfirmsRemoval();
    testWriteRejectsSignerMismatchBeforeTransport();
    testWriteMapsSubmitFailureAndDoesNotClaimNetworkSuccess();
    return 0;
}
