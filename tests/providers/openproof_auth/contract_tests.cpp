#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <thread>
#include <utility>
#include <vector>

import farcaster.capability.auth;
import farcaster.domain.session;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.identity.openproof;
import farcaster.transport.http;

namespace {

namespace capability = farcaster::capability;
namespace foundation = farcaster::foundation;
namespace provider = farcaster::provider::identity::openproof;
namespace transport = farcaster::transport;

enum class Method : std::uint8_t { Get, Post };

struct Step final {
    Method method{Method::Get};
    std::string urlSuffix{};
    std::uint16_t status{};
    std::string responseBody{};
    std::vector<std::string> responseHeaders{};
    std::string requestBodyContains{};
    std::string requestHeaderContains{};
};

class ScriptedTransport final : public transport::http::IHttpTransport {
public:
    explicit ScriptedTransport(std::vector<Step> steps)
        : m_steps(std::move(steps))
    {
    }

    [[nodiscard]] foundation::result::Result<transport::http::HttpResponse>
    get(const transport::http::HttpRequest& request) override
    {
        return consume(Method::Get, request.url, {}, request.headers);
    }

    [[nodiscard]] foundation::result::Result<transport::http::HttpResponse>
    post(const transport::http::HttpPostRequest& request) override
    {
        return consume(Method::Post, request.url, request.body, request.headers);
    }

    [[nodiscard]] bool empty() const noexcept { return m_index == m_steps.size(); }

private:
    [[nodiscard]] foundation::result::Result<transport::http::HttpResponse> consume(
        const Method method,
        const std::string& url,
        const std::string& body,
        const std::vector<std::string>& headers)
    {
        assert(m_index < m_steps.size());
        const Step& step = m_steps[m_index++];
        assert(step.method == method);
        assert(url.ends_with(step.urlSuffix));
        if (!step.requestBodyContains.empty()) {
            assert(body.contains(step.requestBodyContains));
        }
        if (!step.requestHeaderContains.empty()) {
            bool found = false;
            for (const auto& header : headers) {
                if (header.contains(step.requestHeaderContains)) {
                    found = true;
                    break;
                }
            }
            assert(found);
        }
        return transport::http::HttpResponse{
            .statusCode = step.status,
            .body = step.responseBody,
            .headers = step.responseHeaders,
        };
    }

    std::vector<Step> m_steps{};
    std::size_t m_index{};
};

[[nodiscard]] provider::Config config()
{
    return provider::Config{
        .issuer = "https://openproof.example",
        .clientId = "genycaster-native-v1",
        .genyleapAccountUrl = "https://genyleap.example/account",
        .farcasterRelayBaseUrl = "https://relay.example",
        .farcasterRelayVersion = "v1",
        .requestTimeout = std::chrono::milliseconds{500},
        .attemptLifetime = std::chrono::minutes{1},
        .sessionLifetime = std::chrono::hours{1},
    };
}

void deviceAuthorizationCompletesForAlreadyLinkedAccount()
{
    ScriptedTransport transport{{
        Step{
            .method = Method::Post,
            .urlSuffix = "/oauth/device_authorization",
            .status = 200,
            .responseBody =
                R"({"device_code":"device-1","user_code":"ABCD-EFGH","expires_in":600,"interval":1})",
            .requestBodyContains = "client_id=genycaster-native-v1",
        },
        Step{
            .method = Method::Post,
            .urlSuffix = "/oauth/token",
            .status = 200,
            .responseBody =
                R"({"access_token":"access-1","refresh_token":"refresh-1","token_type":"Bearer","expires_in":300})",
            .requestBodyContains = "device_code=device-1",
        },
        Step{
            .method = Method::Get,
            .urlSuffix = "/oauth/userinfo",
            .status = 200,
            .responseBody =
                R"({"sub":"identity-1","preferred_username":"alice","name":"Alice","picture":"https://cdn.example/alice.png"})",
            .requestHeaderContains = "Authorization: Bearer access-1",
        },
        Step{
            .method = Method::Get,
            .urlSuffix = "/account/connections",
            .status = 200,
            .responseBody =
                R"({"connections":[{"provider":"farcaster","subject":"fid:42","fid":"42"}]})",
            .requestHeaderContains = "Authorization: Bearer access-1",
        },
    }};

    auto authentication = provider::makeAuthentication(transport, config());
    assert(authentication);
    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .route = capability::auth::SignInRoute::OpenProof,
    });
    assert(started);
    assert(started->challenge.approvalUrl ==
        "https://genyleap.example/account?return_to=%2Fopenproof%2Foauth%2Fdevice%3Fuser_code%3DABCD-EFGH");

    auto polled = started->attempt->poll();
    assert(polled);
    assert(polled->state == capability::auth::PollState::Completed);
    assert(polled->session);
    assert(polled->session->identityIssuer &&
        *polled->session->identityIssuer == "https://openproof.example");
    assert(polled->session->identitySubject &&
        *polled->session->identitySubject == "identity-1");
    assert(polled->session->fid.value == 42U);
    assert(polled->session->username && *polled->session->username == "alice");
    assert(!polled->session->custodyAddress);
    assert(transport.empty());
}

void deviceAuthorizationRequiresAndCompletesFarcasterLink()
{
    ScriptedTransport transport{{
        Step{
            .method = Method::Post,
            .urlSuffix = "/oauth/device_authorization",
            .status = 200,
            .responseBody =
                R"({"device_code":"device-2","user_code":"WXYZ-1234","expires_in":600,"interval":1})",
        },
        Step{
            .method = Method::Post,
            .urlSuffix = "/oauth/token",
            .status = 200,
            .responseBody =
                R"({"access_token":"access-2","refresh_token":"refresh-2","token_type":"Bearer","expires_in":300})",
        },
        Step{
            .method = Method::Get,
            .urlSuffix = "/oauth/userinfo",
            .status = 200,
            .responseBody = R"({"sub":"identity-2","name":"Bob"})",
        },
        Step{
            .method = Method::Get,
            .urlSuffix = "/account/connections",
            .status = 200,
            .responseBody = R"({"connections":[]})",
        },
        Step{
            .method = Method::Post,
            .urlSuffix = "/account/connections/web3/start",
            .status = 200,
            .responseBody =
                R"({"provider":"farcaster","challenge_id":"c-1","expires_at":"2026-09-29T07:00:00Z","chain_id":"10","nonce":"nonce-12345678","domain":"genyleap.example","uri":"https://genyleap.example/openproof","statement":"Farcaster Auth","resource_prefix":"farcaster://fid/","signer_kind":"custody"})",
            .responseHeaders = {"Set-Cookie: __Host-openproof-web3=ceremony; Path=/; Secure; HttpOnly"},
            .requestHeaderContains = "Authorization: Bearer access-2",
        },
        Step{
            .method = Method::Post,
            .urlSuffix = "/v1/channel",
            .status = 200,
            .responseBody =
                R"({"channelToken":"relay-2","url":"https://farcaster.xyz/~/sign-in-with-farcaster?channelToken=relay-2","nonce":"nonce-12345678"})",
            .requestBodyContains = "nonce-12345678",
        },
        Step{
            .method = Method::Get,
            .urlSuffix = "/v1/channel/status",
            .status = 200,
            .responseBody =
                R"({"state":"completed","nonce":"nonce-12345678","message":"signed message","signature":"0x1234","fid":"84","username":"bob"})",
            .requestHeaderContains = "Authorization: Bearer relay-2",
        },
        Step{
            .method = Method::Post,
            .urlSuffix = "/account/connections/web3/complete",
            .status = 200,
            .responseBody = R"({"connected":true,"provider":"farcaster","subject":"fid:84"})",
            .requestBodyContains = "\"signature\":\"0x1234\"",
            .requestHeaderContains = "__Host-openproof-web3=ceremony",
        },
        Step{
            .method = Method::Get,
            .urlSuffix = "/oauth/userinfo",
            .status = 200,
            .responseBody = R"({"sub":"identity-2","name":"Bob"})",
        },
        Step{
            .method = Method::Get,
            .urlSuffix = "/account/connections",
            .status = 200,
            .responseBody =
                R"({"connections":[{"provider":"farcaster","subject":"fid:84","fid":"84"}]})",
        },
    }};

    auto authentication = provider::makeAuthentication(transport, config());
    assert(authentication);
    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .route = capability::auth::SignInRoute::OpenProof,
    });
    assert(started);

    auto linking = started->attempt->poll();
    assert(linking);
    assert(linking->state == capability::auth::PollState::FarcasterLinkRequired);
    assert(linking->approvalUrl &&
        linking->approvalUrl->starts_with("https://farcaster.xyz/"));

    std::this_thread::sleep_for(std::chrono::milliseconds{1100});
    auto completed = started->attempt->poll();
    assert(completed);
    assert(completed->state == capability::auth::PollState::Completed);
    assert(completed->session);
    assert(completed->session->identitySubject &&
        *completed->session->identitySubject == "identity-2");
    assert(completed->session->fid.value == 84U);
    assert(transport.empty());
}

void rejectsIssuerThatIsNotAnOrigin()
{
    ScriptedTransport transport{{}};
    auto invalid = config();
    invalid.issuer = "https://openproof.example/tenant";

    auto authentication = provider::makeAuthentication(transport, std::move(invalid));
    assert(!authentication);
    assert(transport.empty());
}

void directFarcasterStillCreatesCanonicalOpenProofIdentity()
{
    ScriptedTransport transport{{
        Step{
            .method = Method::Post,
            .urlSuffix = "/auth/web3/start",
            .status = 200,
            .responseBody =
                R"({"provider":"farcaster","challenge_id":"direct-1","expires_at":"2026-09-29T07:00:00Z","chain_id":"10","nonce":"direct-nonce-1234","domain":"genyleap.example","uri":"https://genyleap.example/openproof","statement":"Farcaster Auth","resource_prefix":"farcaster://fid/","signer_kind":"custody"})",
            .responseHeaders = {"Set-Cookie: __Host-openproof-web3=direct-ceremony; Path=/; Secure; HttpOnly"},
        },
        Step{
            .method = Method::Post,
            .urlSuffix = "/v1/channel",
            .status = 200,
            .responseBody =
                R"({"channelToken":"relay-direct","url":"https://farcaster.xyz/~/sign-in-with-farcaster?channelToken=relay-direct","nonce":"direct-nonce-1234"})",
        },
        Step{
            .method = Method::Get,
            .urlSuffix = "/v1/channel/status",
            .status = 200,
            .responseBody =
                R"({"state":"completed","nonce":"direct-nonce-1234","message":"direct signed message","signature":"0xabcd","fid":"126","username":"carol","displayName":"Carol"})",
        },
        Step{
            .method = Method::Post,
            .urlSuffix = "/auth/web3/complete",
            .status = 200,
            .responseBody = R"({"authenticated":true})",
            .responseHeaders = {"Set-Cookie: __Host-openproof-session=session-126; Path=/; Secure; HttpOnly"},
            .requestBodyContains = "\"message\":\"direct signed message\"",
        },
        Step{
            .method = Method::Get,
            .urlSuffix = "/account/profile",
            .status = 200,
            .responseBody =
                R"({"identity_id":"identity-126","display_name":"Carol","preferred_username":"carol"})",
            .requestHeaderContains = "__Host-openproof-session=session-126",
        },
        Step{
            .method = Method::Get,
            .urlSuffix = "/account/connections",
            .status = 200,
            .responseBody =
                R"({"connections":[{"provider":"farcaster","subject":"fid:126","fid":"126"}]})",
            .requestHeaderContains = "__Host-openproof-session=session-126",
        },
    }};

    auto authentication = provider::makeAuthentication(transport, config());
    assert(authentication);
    auto started = (*authentication)->begin(capability::auth::SignInRequest{
        .route = capability::auth::SignInRoute::Farcaster,
    });
    assert(started);
    assert(started->challenge.approvalUrl.starts_with("https://farcaster.xyz/"));

    auto completed = started->attempt->poll();
    assert(completed);
    assert(completed->state == capability::auth::PollState::Completed);
    assert(completed->session);
    assert(completed->session->identitySubject &&
        *completed->session->identitySubject == "identity-126");
    assert(completed->session->fid.value == 126U);
    assert(transport.empty());
}

}

int main()
{
    deviceAuthorizationCompletesForAlreadyLinkedAccount();
    deviceAuthorizationRequiresAndCompletesFarcasterLink();
    rejectsIssuerThatIsNotAnOrigin();
    directFarcasterStillCreatesCanonicalOpenProofIdentity();
    return 0;
}
