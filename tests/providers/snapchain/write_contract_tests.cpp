#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <deque>
#include <expected>
#include <string>
#include <utility>
#include <vector>

import farcaster.capability.write_submit;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.domain.write;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.provider.snapchain.write;
import farcaster.transport.http;

namespace {

namespace capability = farcaster::capability;
namespace domain = farcaster::domain;
namespace foundation = farcaster::foundation;
namespace provider = farcaster::provider;
namespace transport = farcaster::transport;

class FakeTransport final : public transport::http::IHttpTransport {
public:
    std::deque<transport::http::HttpResponse> getReplies{};
    std::deque<transport::http::HttpResponse> postReplies{};
    std::vector<transport::http::HttpRequest> gets{};
    std::vector<transport::http::HttpPostRequest> posts{};

    [[nodiscard]] foundation::result::Result<transport::http::HttpResponse> get(
        const transport::http::HttpRequest& request) override
    {
        gets.push_back(request);
        if (getReplies.empty()) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InternalFailure,
                "Unexpected test GET."));
        }
        auto reply = std::move(getReplies.front());
        getReplies.pop_front();
        return reply;
    }

    [[nodiscard]] foundation::result::Result<transport::http::HttpResponse> post(
        const transport::http::HttpPostRequest& request) override
    {
        posts.push_back(request);
        if (postReplies.empty()) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InternalFailure,
                "Unexpected test POST."));
        }
        auto reply = std::move(postReplies.front());
        postReplies.pop_front();
        return reply;
    }
};

[[nodiscard]] domain::write::SignedOperation operation()
{
    constexpr auto castId = "0x00112233445566778899aabbccddeeff00112233";
    return domain::write::SignedOperation{
        .kind = domain::write::OperationKind::PublishCast,
        .idempotencyKey = domain::write::IdempotencyKey{castId},
        .author = domain::identity::Fid{42U},
        .optimisticCast = domain::cast::Cast{
            .id = domain::cast::CastId{castId},
            .author = domain::identity::Fid{42U},
            .text = "hello",
            .createdAt = foundation::time::now(),
        },
        .signerPublicKey = domain::signer::PublicKey{
            "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"},
        .messageBytes = std::string{"\x01\x02\x00signed", 9U},
        .signedAt = foundation::time::now(),
    };
}

[[nodiscard]] std::string authoritativeJson(
    const std::string& hash = "0x00112233445566778899aabbccddeeff00112233",
    const std::uint64_t fid = 42U)
{
    return "{\"hash\":\"" + hash + "\",\"data\":{\"type\":\"MESSAGE_TYPE_CAST_ADD\",\"fid\":" +
        std::to_string(fid) + "}}";
}

void submitUsesCanonicalBinaryEndpoint()
{
    FakeTransport http{};
    http.postReplies.push_back({.statusCode = 200U, .body = authoritativeJson()});
    auto submitter = provider::snapchain::write::makeSubmitter(
        http,
        provider::snapchain::write::Config{
            .baseUrl = "https://snapchain.test/",
            .timeout = std::chrono::milliseconds{4321},
            .maxBodyBytes = 12345U,
        });
    assert(submitter);
    const auto signedOperation = operation();
    auto result = submitter->submit(signedOperation);
    assert(result);
    assert(http.posts.size() == 1U);
    const auto& request = http.posts.front();
    assert(request.url == "https://snapchain.test/v1/submitMessage");
    assert(request.body == signedOperation.messageBytes);
    assert(request.timeout == std::chrono::milliseconds{4321});
    assert(request.maxBodyBytes == 12345U);
    assert(request.headers.size() == 2U);
    assert(request.headers.at(1) == "Content-Type: application/octet-stream");
}

void acceptsExactProtoJsonNativeFid()
{
    // HyperSnap native IDs begin at 2^32; protobuf JSON serializes uint64
    // as decimal strings, not numeric JavaScript doubles.
    constexpr std::uint64_t nativeFid = 4294967296ULL;
    auto native = operation();
    native.author = domain::identity::Fid{nativeFid};
    native.optimisticCast->author = native.author;
    const auto reply = "{\"hash\":\"0x00112233445566778899aabbccddeeff00112233\","
        "\"data\":{\"type\":\"MESSAGE_TYPE_CAST_ADD\","
        "\"fid\":\"4294967296\"}}";
    FakeTransport http{};
    http.postReplies.push_back({.statusCode = 200U, .body = reply});
    http.getReplies.push_back({.statusCode = 200U, .body = reply});
    auto submitter = provider::snapchain::write::makeSubmitter(
        http, provider::snapchain::write::Config{.baseUrl = "https://hypersnap.test"});
    const auto submitted = submitter->submit(native);
    assert(submitted);
    auto confirmed = submitter->reconcile(native);
    assert(confirmed && *confirmed == capability::write::ConfirmationState::Confirmed);
    assert(http.gets.size() == 1U);
    assert(http.gets.front().url.find("fid=4294967296&") != std::string::npos);

    FakeTransport malformed{};
    malformed.postReplies.push_back({
        .statusCode = 200U,
        .body = "{\"data\":{\"type\":\"MESSAGE_TYPE_CAST_ADD\","
                "\"fid\":\"18446744073709551616\"}}",
    });
    auto invalidSubmitter = provider::snapchain::write::makeSubmitter(
        malformed, provider::snapchain::write::Config{.baseUrl = "https://hypersnap.test"});
    auto invalid = invalidSubmitter->submit(native);
    assert(!invalid);
    assert(invalid.error().code == foundation::error::ErrorCode::ProtocolFailure);
}

void unknownFidProvidesNetworkSpecificDiagnostic()
{
    FakeTransport http{};
    http.postReplies.push_back({
        .statusCode = 400U,
        .body = R"({"error":"Failed to submit message","error_detail":"bad_request.validation_failure/unknown fid"})",
    });
    auto submitter = provider::snapchain::write::makeSubmitter(
        http, provider::snapchain::write::Config{.baseUrl = "https://hypersnap.test"});
    auto result = submitter->submit(operation());
    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::ProtocolFailure);
    assert(result.error().message.find("SAME node") != std::string::npos);
}

void defaultConfigurationRequiresCompositionOrigin()
{
    const auto config = provider::snapchain::write::Config{};
    assert(config.baseUrl.empty());
    assert(config.authorizationHeaderName.empty());
    assert(config.authorizationHeaderValue.empty());

    FakeTransport http{};
    auto submitter = provider::snapchain::write::makeSubmitter(http, config);
    auto result = submitter->submit(operation());
    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::InvalidArgument);
    assert(http.posts.empty());
}

void explicitAuthorizationHeaderIsForwarded()
{
    FakeTransport http{};
    http.postReplies.push_back({.statusCode = 200U, .body = authoritativeJson()});
    auto submitter = provider::snapchain::write::makeSubmitter(
        http,
        provider::snapchain::write::Config{
            .baseUrl = "https://snapchain.test",
            .authorizationHeaderName = "X-Upstream-Credential",
            .authorizationHeaderValue = "test-secret",
        });
    auto result = submitter->submit(operation());
    assert(result);
    assert(http.posts.size() == 1U);
    assert(std::ranges::find(
        http.posts.front().headers,
        std::string{"X-Upstream-Credential: test-secret"}) != http.posts.front().headers.end());
}

void reconcileUsesCanonicalCastIdentity()
{
    FakeTransport http{};
    http.getReplies.push_back({.statusCode = 200U, .body = authoritativeJson()});
    auto submitter = provider::snapchain::write::makeSubmitter(
        http,
        provider::snapchain::write::Config{.baseUrl = "https://snapchain.test"});
    const auto signedOperation = operation();
    auto result = submitter->reconcile(signedOperation);
    assert(result && *result == capability::write::ConfirmationState::Confirmed);
    assert(http.gets.size() == 1U);
    assert(http.gets.front().url ==
        "https://snapchain.test/v1/castById?fid=42&hash=0x00112233445566778899aabbccddeeff00112233");
}

void reactionAndFollowReconciliationUseCanonicalIdentity()
{
    const domain::cast::CastReference castTarget{
        .author = domain::identity::Fid{7U},
        .id = domain::cast::CastId{
            "0x000102030405060708090a0b0c0d0e0f10111213"},
    };
    const domain::write::SignedOperation like{
        .kind = domain::write::OperationKind::Like,
        .idempotencyKey = domain::write::IdempotencyKey{
            "0x1111222233334444555566667777888899990000"},
        .author = domain::identity::Fid{42U},
        .target = domain::write::OperationTarget{.cast = castTarget},
        .signerPublicKey = operation().signerPublicKey,
        .messageBytes = "signed-like",
        .signedAt = foundation::time::now(),
    };
    FakeTransport reactionHttp{};
    reactionHttp.getReplies.push_back({
        .statusCode = 200U,
        .body =
            "{\"hash\":\"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
            "\"data\":{\"type\":\"MESSAGE_TYPE_REACTION_ADD\",\"fid\":42,"
            "\"reactionBody\":{\"type\":\"REACTION_TYPE_LIKE\","
            "\"targetCastId\":{\"fid\":7,\"hash\":\"0x000102030405060708090a0b0c0d0e0f10111213\"}}}}",
    });
    auto reactionSubmitter = provider::snapchain::write::makeSubmitter(
        reactionHttp,
        provider::snapchain::write::Config{.baseUrl = "https://snapchain.test"});
    auto reaction = reactionSubmitter->reconcile(like);
    assert(reaction && *reaction == capability::write::ConfirmationState::Confirmed);
    assert(reactionHttp.gets.front().url ==
        "https://snapchain.test/v1/reactionById?fid=42&target_fid=7&target_hash="
        "0x000102030405060708090a0b0c0d0e0f10111213&reaction_type=Like");

    auto unlike = like;
    unlike.kind = domain::write::OperationKind::Unlike;
    FakeTransport unlikeHttp{};
    unlikeHttp.getReplies.push_back({.statusCode = 404U, .body = "not found"});
    auto unlikeSubmitter = provider::snapchain::write::makeSubmitter(
        unlikeHttp,
        provider::snapchain::write::Config{.baseUrl = "https://snapchain.test"});
    auto absentReaction = unlikeSubmitter->reconcile(unlike);
    assert(absentReaction && *absentReaction == capability::write::ConfirmationState::Confirmed);

    const domain::write::SignedOperation follow{
        .kind = domain::write::OperationKind::Follow,
        .idempotencyKey = domain::write::IdempotencyKey{
            "0xaaaabbbbccccddddeeeeffff0000111122223333"},
        .author = domain::identity::Fid{42U},
        .target = domain::write::OperationTarget{.fid = domain::identity::Fid{99U}},
        .signerPublicKey = operation().signerPublicKey,
        .messageBytes = "signed-follow",
        .signedAt = foundation::time::now(),
    };
    FakeTransport followHttp{};
    followHttp.getReplies.push_back({
        .statusCode = 200U,
        .body =
            "{\"hash\":\"0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\","
            "\"data\":{\"type\":\"MESSAGE_TYPE_LINK_ADD\",\"fid\":42,"
            "\"linkBody\":{\"type\":\"follow\",\"targetFid\":99}}}",
    });
    auto followSubmitter = provider::snapchain::write::makeSubmitter(
        followHttp,
        provider::snapchain::write::Config{.baseUrl = "https://snapchain.test"});
    auto followed = followSubmitter->reconcile(follow);
    assert(followed && *followed == capability::write::ConfirmationState::Confirmed);
    assert(followHttp.gets.front().url ==
        "https://snapchain.test/v1/linkById?fid=42&target_fid=99&link_type=follow");

    auto unfollow = follow;
    unfollow.kind = domain::write::OperationKind::Unfollow;
    FakeTransport unfollowHttp{};
    unfollowHttp.getReplies.push_back({.statusCode = 404U, .body = "not found"});
    auto unfollowSubmitter = provider::snapchain::write::makeSubmitter(
        unfollowHttp,
        provider::snapchain::write::Config{.baseUrl = "https://snapchain.test"});
    auto absentFollow = unfollowSubmitter->reconcile(unfollow);
    assert(absentFollow && *absentFollow == capability::write::ConfirmationState::Confirmed);
}

void missingRemovedCastConfirmsDeletion()
{
    auto remove = operation();
    remove.kind = domain::write::OperationKind::RemoveCast;
    remove.target.cast = domain::cast::CastReference{
        .author = domain::identity::Fid{42U},
        .id = domain::cast::CastId{
            "0x00112233445566778899aabbccddeeff00112233"},
    };
    remove.optimisticCast.reset();

    FakeTransport http{};
    http.getReplies.push_back({.statusCode = 404U, .body = "not found"});
    auto submitter = provider::snapchain::write::makeSubmitter(
        http,
        provider::snapchain::write::Config{.baseUrl = "https://snapchain.test"});
    auto result = submitter->reconcile(remove);
    assert(result && *result == capability::write::ConfirmationState::Confirmed);
    assert(http.gets.size() == 1U);
    assert(http.gets.front().url ==
        "https://snapchain.test/v1/castById?fid=42&hash=0x00112233445566778899aabbccddeeff00112233");
}

void missingCastIsAbsent()
{
    FakeTransport http{};
    http.getReplies.push_back({.statusCode = 404U, .body = "{\"error\":\"not found\"}"});
    auto submitter = provider::snapchain::write::makeSubmitter(
        http,
        provider::snapchain::write::Config{.baseUrl = "https://snapchain.test"});
    auto result = submitter->reconcile(operation());
    assert(result && *result == capability::write::ConfirmationState::Absent);
}

void mismatchedNetworkIdentityFailsClosed()
{
    FakeTransport http{};
    http.postReplies.push_back({
        .statusCode = 200U,
        .body = authoritativeJson("0xffffffffffffffffffffffffffffffffffffffff"),
    });
    auto submitter = provider::snapchain::write::makeSubmitter(
        http,
        provider::snapchain::write::Config{.baseUrl = "https://snapchain.test"});
    auto result = submitter->submit(operation());
    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::ProtocolFailure);
}

void splitReadAndValidatorRoutesKeepCredentialsIsolated()
{
    FakeTransport http{};
    http.postReplies.push_back({.statusCode = 200U, .body = authoritativeJson()});
    http.getReplies.push_back({.statusCode = 200U, .body = authoritativeJson()});
    auto submitter = provider::snapchain::write::makeSubmitter(
        http, provider::snapchain::write::Config{
            .baseUrl = "https://validator.test",
            .readBaseUrl = "https://read-node.test",
            .readAuthorizationHeaderName = "X-Read-Token",
            .readAuthorizationHeaderValue = "read-test-value",
            .authorizationHeaderName = "X-Write-Token",
            .authorizationHeaderValue = "write-test-value",
        });
    auto sent = submitter->submit(operation());
    assert(sent);
    auto confirmed = submitter->reconcile(operation());
    assert(confirmed && *confirmed == capability::write::ConfirmationState::Confirmed);
    assert(http.posts.size() == 1U);
    assert(http.posts.front().url == "https://validator.test/v1/submitMessage");
    assert(std::ranges::find(http.posts.front().headers, "X-Read-Token: read-test-value")
        == http.posts.front().headers.end());
    assert(std::ranges::find(http.posts.front().headers, "X-Write-Token: write-test-value")
        != http.posts.front().headers.end());
    assert(http.gets.size() == 1U);
    assert(http.gets.front().url ==
        "https://read-node.test/v1/castById?fid=42&hash=0x00112233445566778899aabbccddeeff00112233");
    assert(std::ranges::find(http.gets.front().headers, "X-Write-Token: write-test-value")
        == http.gets.front().headers.end());
    assert(std::ranges::find(http.gets.front().headers, "X-Read-Token: read-test-value")
        != http.gets.front().headers.end());
}

void invalidReadEndpointFailsClosed()
{
    FakeTransport http{};
    auto submitter = provider::snapchain::write::makeSubmitter(
        http, provider::snapchain::write::Config{
            .baseUrl = "https://validator.test", .readBaseUrl = "bad:invalid",
        });
    const auto status = submitter->reconcile(operation());
    assert(!status);
    assert(status.error().code == foundation::error::ErrorCode::InvalidArgument);
    assert(http.gets.empty());
}

void submitConflictMapsWithoutClaimingSuccess()
{
    FakeTransport http{};
    http.postReplies.push_back({.statusCode = 409U, .body = "conflict"});
    auto submitter = provider::snapchain::write::makeSubmitter(
        http,
        provider::snapchain::write::Config{.baseUrl = "https://snapchain.test"});
    auto result = submitter->submit(operation());
    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::Conflict);
}

} // namespace

int main()
{
    defaultConfigurationRequiresCompositionOrigin();
    splitReadAndValidatorRoutesKeepCredentialsIsolated();
    invalidReadEndpointFailsClosed();
    explicitAuthorizationHeaderIsForwarded();
    submitUsesCanonicalBinaryEndpoint();
    acceptsExactProtoJsonNativeFid();
    unknownFidProvidesNetworkSpecificDiagnostic();
    reconcileUsesCanonicalCastIdentity();
    reactionAndFollowReconciliationUseCanonicalIdentity();
    missingRemovedCastConfirmsDeletion();
    missingCastIsAbsent();
    mismatchedNetworkIdentityFailsClosed();
    submitConflictMapsWithoutClaimingSuccess();
    return 0;
}
