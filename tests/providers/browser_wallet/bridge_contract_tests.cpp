#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <future>
#include <string>
#include <string_view>
#include <thread>

import farcaster.capability.wallet;
import farcaster.foundation.result;
import farcaster.provider.compatibility.browser_wallet.bridge;

namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
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
    return {
        .host = "127.0.0.1",
        .port = static_cast<std::uint16_t>(
            std::stoul(std::string{url.substr(prefix.size(), slash - prefix.size())})),
        .target = std::string{url.substr(slash)},
    };
}

[[nodiscard]] http::response<http::string_body> request(
    const ParsedUrl& url,
    const http::verb method,
    std::string target,
    std::string body = {},
    std::string originOverride = {},
    std::string hostOverride = {})
{
    asio::io_context context{};
    beast::tcp_stream stream{context};
    stream.connect(tcp::resolver{context}.resolve(url.host, std::to_string(url.port)));
    http::request<http::string_body> message{method, std::move(target), 11};
    const std::string authority = url.host + ":" + std::to_string(url.port);
    message.set(http::field::host, hostOverride.empty() ? authority : hostOverride);
    if (method == http::verb::post) {
        message.set(http::field::origin,
            originOverride.empty() ? "http://" + authority : originOverride);
    }
    if (!body.empty()) message.set(http::field::content_type, "application/json");
    message.body() = std::move(body);
    message.prepare_payload();
    http::write(stream, message);
    beast::flat_buffer buffer{};
    http::response<http::string_body> response{};
    http::read(stream, buffer, response);
    beast::error_code ignored{};
    stream.socket().shutdown(tcp::socket::shutdown_both, ignored);
    return response;
}

void testBridgeRoutesWalletRequestsAndEvents()
{
    using provider::compatibility::browser_wallet::bridge::Config;
    using provider::compatibility::browser_wallet::bridge::makeBridge;

    std::promise<std::string> launchedPromise{};
    auto launched = launchedPromise.get_future();
    auto bridge = makeBridge(Config{
        .launch = [&launchedPromise](const std::string& url) -> foundation::result::Status {
            launchedPromise.set_value(url);
            return {};
        },
        .supportedChains = {"eip155:1", "eip155:8453"},
        .requestTimeout = std::chrono::seconds{3},
        .maxRequestBytes = 16U * 1024U,
    });
    assert(bridge);
    assert((*bridge)->available());
    assert((*bridge)->supportedChains().size() == 2U);
    assert((*bridge)->beginSession("Test Mini App", "https://mini.example"));
    assert(launched.wait_for(std::chrono::milliseconds{50}) == std::future_status::timeout);

    std::promise<capability::wallet::EthereumRpcResponse> responsePromise{};
    auto responseFuture = responsePromise.get_future();
    (*bridge)->request(
        capability::wallet::EthereumRpcRequest{
            .id = 7U,
            .method = "eth_requestAccounts",
            .paramsJson = "[]",
        },
        [&responsePromise](capability::wallet::EthereumRpcResponse response) mutable {
            responsePromise.set_value(std::move(response));
        });

    assert(launched.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const ParsedUrl url = parseLoopbackUrl(launched.get());
    const auto page = request(url, http::verb::get, url.target);
    assert(page.result() == http::status::ok);
    assert(page[http::field::cache_control] == "no-store, max-age=0");
    assert(page.body().find("Test Mini App") != std::string::npos);
    assert(page.body().find("https://mini.example") != std::string::npos);
    assert(page.body().find("Explicit approval required") != std::string::npos);
    // The wallet chooser is a styled, accessible GenyCaster control, not a
    // platform/browser-native select widget.
    assert(page.body().find("role=\"radiogroup\"") != std::string::npos);
    assert(page.body().find("<select id=\"options\"") == std::string::npos);
    assert(page.body().find("className='wallet-option'") != std::string::npos);
    // Wallet labels and the Extension badge must never clip against a
    // 196px list scrollport (the fourth provider used to overflow).
    assert(page.body().find("max-height:196px") == std::string::npos);
    assert(page.body().find("grid-template-columns:16px minmax(0,1fr) auto") != std::string::npos);
    assert(page.body().find("scrollbar-gutter:stable") != std::string::npos);
    assert(page.body().find("result-ready") != std::string::npos);
    assert(page.body().find("request.method==='eth_requestAccounts'") != std::string::npos);
    assert(page.body().find("Why is this a local page?") != std::string::npos);
    assert(page.body().find("Your private key stays there.") != std::string::npos);
    assert(page.body().find("window.ethereum.request") != std::string::npos);
    const auto wrongHost = request(url, http::verb::get, url.target, {}, {}, "attacker.example");
    assert(wrongHost.result() == http::status::forbidden);
    const auto wrongOrigin = request(url, http::verb::post, url.target + "/result",
        R"({"transportId":1,"result":"bogus"})", "https://attacker.example");
    assert(wrongOrigin.result() == http::status::forbidden);

    const auto next = request(url, http::verb::get, url.target + "/next");
    assert(next.result() == http::status::ok);
    boost::system::error_code parseError{};
    const auto payload = json::parse(next.body(), parseError);
    assert(!parseError && payload.is_object());
    const auto& object = payload.as_object();
    assert(object.at("method").as_string() == "eth_requestAccounts");
    assert(object.at("requiresApproval").as_bool());
    const auto& transportValue = object.at("transportId");
    const std::uint64_t transportId = transportValue.is_uint64()
        ? transportValue.as_uint64()
        : static_cast<std::uint64_t>(transportValue.as_int64());

    const auto posted = request(
        url,
        http::verb::post,
        url.target + "/result",
        json::serialize(json::object{
            {"transportId", transportId},
            {"result", json::array{"0x1111111111111111111111111111111111111111"}},
        }));
    assert(posted.result() == http::status::ok);
    assert(responseFuture.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const auto response = responseFuture.get();
    assert(!response.error);
    assert(response.id == 7U);
    assert(response.resultJson.find("0x1111111111111111111111111111111111111111") !=
        std::string::npos);

    std::promise<capability::wallet::EthereumRpcResponse> chainResponsePromise{};
    auto chainResponseFuture = chainResponsePromise.get_future();
    (*bridge)->request(
        capability::wallet::EthereumRpcRequest{
            .id = 8U,
            .method = "eth_chainId",
            .paramsJson = "[]",
        },
        [&chainResponsePromise](capability::wallet::EthereumRpcResponse chainResponse) mutable {
            chainResponsePromise.set_value(std::move(chainResponse));
        });
    const auto chainNext = request(url, http::verb::get, url.target + "/next");
    assert(chainNext.result() == http::status::ok);
    const auto chainPayload = json::parse(chainNext.body(), parseError);
    assert(!parseError && chainPayload.is_object());
    const auto& chainObject = chainPayload.as_object();
    assert(chainObject.at("method").as_string() == "eth_chainId");
    assert(!chainObject.at("requiresApproval").as_bool());
    const auto& chainTransportValue = chainObject.at("transportId");
    const std::uint64_t chainTransportId = chainTransportValue.is_uint64()
        ? chainTransportValue.as_uint64()
        : static_cast<std::uint64_t>(chainTransportValue.as_int64());
    const auto chainPosted = request(
        url, http::verb::post, url.target + "/result",
        json::serialize(json::object{{"transportId", chainTransportId}, {"result", "0x2105"}}));
    assert(chainPosted.result() == http::status::ok);
    assert(chainResponseFuture.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const auto chainResponse = chainResponseFuture.get();
    assert(!chainResponse.error);
    assert(chainResponse.id == 8U);
    assert(chainResponse.resultJson == R"("0x2105")");

    std::promise<capability::wallet::EthereumWalletEvent> eventPromise{};
    auto eventFuture = eventPromise.get_future();
    (*bridge)->setEventSink(
        [&eventPromise](capability::wallet::EthereumWalletEvent event) mutable {
            eventPromise.set_value(std::move(event));
        });
    const auto eventPosted = request(
        url,
        http::verb::post,
        url.target + "/event",
        R"({"event":"chainChanged","params":["0x2105"]})");
    assert(eventPosted.result() == http::status::ok);
    assert(eventFuture.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const auto event = eventFuture.get();
    assert(event.kind == capability::wallet::EthereumWalletEventKind::ChainChanged);
    assert(event.paramsJson == R"(["0x2105"])");
}


void testAccountAccessConsentIsRememberedPerOrigin()
{
    using provider::compatibility::browser_wallet::bridge::Config;
    using provider::compatibility::browser_wallet::bridge::makeBridge;

    std::promise<std::string> firstLaunchPromise{};
    std::promise<std::string> secondLaunchPromise{};
    std::promise<std::string> thirdLaunchPromise{};
    auto firstLaunch = firstLaunchPromise.get_future();
    auto secondLaunch = secondLaunchPromise.get_future();
    auto thirdLaunch = thirdLaunchPromise.get_future();
    std::atomic_int launchCount{};

    auto bridge = makeBridge(Config{
        .launch = [&firstLaunchPromise, &secondLaunchPromise, &thirdLaunchPromise, &launchCount](
                      const std::string& url) -> foundation::result::Status {
            const int index = launchCount.fetch_add(1, std::memory_order_relaxed);
            if (index == 0) {
                firstLaunchPromise.set_value(url);
            } else if (index == 1) {
                secondLaunchPromise.set_value(url);
            } else if (index == 2) {
                thirdLaunchPromise.set_value(url);
            }
            return {};
        },
        .supportedChains = {"eip155:8453"},
        .requestTimeout = std::chrono::seconds{3},
    });
    assert(bridge);
    assert((*bridge)->beginSession("Test Mini App", "https://mini.example"));

    std::promise<capability::wallet::EthereumRpcResponse> firstResponsePromise{};
    auto firstResponse = firstResponsePromise.get_future();
    (*bridge)->request(
        capability::wallet::EthereumRpcRequest{
            .id = 21U,
            .method = "eth_requestAccounts",
            .paramsJson = "[]",
        },
        [&firstResponsePromise](capability::wallet::EthereumRpcResponse response) mutable {
            firstResponsePromise.set_value(std::move(response));
        });

    assert(firstLaunch.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const ParsedUrl firstUrl = parseLoopbackUrl(firstLaunch.get());
    const auto firstNext = request(firstUrl, http::verb::get, firstUrl.target + "/next");
    assert(firstNext.result() == http::status::ok);
    boost::system::error_code parseError{};
    const auto firstPayload = json::parse(firstNext.body(), parseError);
    assert(!parseError && firstPayload.is_object());
    assert(firstPayload.as_object().at("requiresApproval").as_bool());
    const auto& firstTransportValue = firstPayload.as_object().at("transportId");
    const std::uint64_t firstTransportId = firstTransportValue.is_uint64()
        ? firstTransportValue.as_uint64()
        : static_cast<std::uint64_t>(firstTransportValue.as_int64());
    const auto firstPosted = request(
        firstUrl,
        http::verb::post,
        firstUrl.target + "/result",
        json::serialize(json::object{
            {"transportId", firstTransportId},
            {"result", json::array{"0x1111111111111111111111111111111111111111"}},
        }));
    assert(firstPosted.result() == http::status::ok);
    assert(firstResponse.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    assert(!firstResponse.get().error);

    // Many EIP-1193 wallets announce the already-approved account once more
    // after eth_requestAccounts resolves. This is not an identity switch.
    const auto sameAccountEvent = request(
        firstUrl, http::verb::post, firstUrl.target + "/event",
        R"({"event":"accountsChanged","params":[["0x1111111111111111111111111111111111111111"]]})");
    assert(sameAccountEvent.result() == http::status::ok);

    (*bridge)->endSession();
    assert((*bridge)->beginSession("Test Mini App", "https://mini.example"));

    std::promise<capability::wallet::EthereumRpcResponse> secondResponsePromise{};
    auto secondResponse = secondResponsePromise.get_future();
    (*bridge)->request(
        capability::wallet::EthereumRpcRequest{
            .id = 22U,
            .method = "eth_requestAccounts",
            .paramsJson = "[]",
        },
        [&secondResponsePromise](capability::wallet::EthereumRpcResponse response) mutable {
            secondResponsePromise.set_value(std::move(response));
        });

    assert(secondLaunch.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const ParsedUrl secondUrl = parseLoopbackUrl(secondLaunch.get());
    const auto secondNext = request(secondUrl, http::verb::get, secondUrl.target + "/next");
    assert(secondNext.result() == http::status::ok);
    const auto secondPayload = json::parse(secondNext.body(), parseError);
    assert(!parseError && secondPayload.is_object());
    // Even a remembered origin grant must not silently drive a new connection
    // request through the browser without a fresh click.
    assert(secondPayload.as_object().at("requiresApproval").as_bool());
    const auto& secondTransportValue = secondPayload.as_object().at("transportId");
    const std::uint64_t secondTransportId = secondTransportValue.is_uint64()
        ? secondTransportValue.as_uint64()
        : static_cast<std::uint64_t>(secondTransportValue.as_int64());
    const auto secondPosted = request(
        secondUrl,
        http::verb::post,
        secondUrl.target + "/result",
        json::serialize(json::object{
            {"transportId", secondTransportId},
            {"result", json::array{"0x1111111111111111111111111111111111111111"}},
        }));
    assert(secondPosted.result() == http::status::ok);
    assert(secondResponse.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    assert(!secondResponse.get().error);

    // A real principal switch must still revoke the origin grant.
    const auto differentAccountEvent = request(
        secondUrl, http::verb::post, secondUrl.target + "/event",
        R"({"event":"accountsChanged","params":[["0x2222222222222222222222222222222222222222"]]})");
    assert(differentAccountEvent.result() == http::status::ok);
    std::promise<capability::wallet::EthereumRpcResponse> revokedPromise{};
    auto revoked = revokedPromise.get_future();
    (*bridge)->request({.id=221U,.method="eth_accounts",.paramsJson="[]"},
        [&revokedPromise](auto response) { revokedPromise.set_value(std::move(response)); });
    assert(revoked.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    assert(revoked.get().resultJson == "[]");

    (*bridge)->endSession();
    assert((*bridge)->beginSession("Other Mini App", "https://other.example"));

    std::promise<capability::wallet::EthereumRpcResponse> thirdResponsePromise{};
    auto thirdResponse = thirdResponsePromise.get_future();
    (*bridge)->request(
        capability::wallet::EthereumRpcRequest{
            .id = 23U,
            .method = "eth_requestAccounts",
            .paramsJson = "[]",
        },
        [&thirdResponsePromise](capability::wallet::EthereumRpcResponse response) mutable {
            thirdResponsePromise.set_value(std::move(response));
        });

    assert(thirdLaunch.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const ParsedUrl thirdUrl = parseLoopbackUrl(thirdLaunch.get());
    const auto thirdNext = request(thirdUrl, http::verb::get, thirdUrl.target + "/next");
    assert(thirdNext.result() == http::status::ok);
    const auto thirdPayload = json::parse(thirdNext.body(), parseError);
    assert(!parseError && thirdPayload.is_object());
    assert(thirdPayload.as_object().at("requiresApproval").as_bool());
    const auto& thirdTransportValue = thirdPayload.as_object().at("transportId");
    const std::uint64_t thirdTransportId = thirdTransportValue.is_uint64()
        ? thirdTransportValue.as_uint64()
        : static_cast<std::uint64_t>(thirdTransportValue.as_int64());
    const auto thirdPosted = request(
        thirdUrl,
        http::verb::post,
        thirdUrl.target + "/result",
        json::serialize(json::object{
            {"transportId", thirdTransportId},
            {"error", json::object{{"code", 4001}, {"message", "Rejected"}}},
        }));
    assert(thirdPosted.result() == http::status::ok);
    assert(thirdResponse.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const auto rejected = thirdResponse.get();
    assert(rejected.error);
    assert(rejected.error->code == 4001);
}

void testAccountPrivacyUntilExplicitPermission()
{
    using provider::compatibility::browser_wallet::bridge::Config;
    using provider::compatibility::browser_wallet::bridge::makeBridge;

    std::promise<std::string> launchPromise{};
    auto launched = launchPromise.get_future();
    auto bridge = makeBridge(Config{
        .launch = [&launchPromise](const std::string& url) -> foundation::result::Status {
            launchPromise.set_value(url);
            return {};
        },
        .supportedChains = {"eip155:8453"},
    });
    assert(bridge);
    assert((*bridge)->beginSession("Accounts", "https://accounts.example"));

    std::promise<capability::wallet::EthereumRpcResponse> privateAccountsPromise{};
    auto privateAccounts = privateAccountsPromise.get_future();
    (*bridge)->request({.id=40U,.method="eth_accounts",.paramsJson="[]"},
        [&privateAccountsPromise](auto result) {
            privateAccountsPromise.set_value(std::move(result));
        });
    assert(privateAccounts.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const auto privateResult = privateAccounts.get();
    assert(!privateResult.error);
    assert(privateResult.resultJson == "[]");
    assert(launched.wait_for(std::chrono::milliseconds{50}) == std::future_status::timeout);

    std::promise<capability::wallet::EthereumRpcResponse> signPromise{};
    auto sign = signPromise.get_future();
    (*bridge)->request({.id=41U,.method="personal_sign",.paramsJson="[]"},
        [&signPromise](auto result) { signPromise.set_value(std::move(result)); });
    assert(sign.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    assert(sign.get().error->code == 4100);

    std::promise<capability::wallet::EthereumRpcResponse> permissionPromise{};
    auto permission = permissionPromise.get_future();
    (*bridge)->request({
        .id=42U,.method="wallet_requestPermissions",
        .paramsJson=R"([{"eth_accounts":{}}])"},
        [&permissionPromise](auto result) {
            permissionPromise.set_value(std::move(result));
        });
    assert(launched.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const ParsedUrl url = parseLoopbackUrl(launched.get());
    const auto next = request(url,http::verb::get,url.target+"/next");
    assert(next.result() == http::status::ok);
    boost::system::error_code parseError{};
    const auto payload = json::parse(next.body(),parseError);
    assert(!parseError && payload.is_object());
    const auto transport = payload.as_object().at("transportId");
    const auto transportId = transport.is_uint64()
        ? transport.as_uint64() : static_cast<std::uint64_t>(transport.as_int64());
    const auto posted = request(url,http::verb::post,url.target+"/result",
        json::serialize(json::object{
            {"transportId",transportId},
            {"result",json::array{json::object{{"parentCapability","eth_accounts"}}}},
        }));
    assert(posted.result() == http::status::ok);
    assert(permission.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    assert(!permission.get().error);

    std::promise<capability::wallet::EthereumRpcResponse> exposedAccountsPromise{};
    auto exposedAccounts = exposedAccountsPromise.get_future();
    (*bridge)->request({.id=43U,.method="eth_accounts",.paramsJson="[]"},
        [&exposedAccountsPromise](auto result) {
            exposedAccountsPromise.set_value(std::move(result));
        });
    const auto accountsNext = request(url,http::verb::get,url.target+"/next");
    assert(accountsNext.result() == http::status::ok);
    const auto accountsPayload = json::parse(accountsNext.body(),parseError);
    assert(!parseError && accountsPayload.is_object());
    assert(accountsPayload.as_object().at("method").as_string() == "eth_accounts");
    (*bridge)->endSession();

    assert((*bridge)->beginSession("Other app","https://other.example"));
    std::promise<capability::wallet::EthereumRpcResponse> differentOriginPromise{};
    auto differentOrigin = differentOriginPromise.get_future();
    (*bridge)->request({.id=44U,.method="eth_accounts",.paramsJson="[]"},
        [&differentOriginPromise](auto result) {
            differentOriginPromise.set_value(std::move(result));
        });
    assert(differentOrigin.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    assert(differentOrigin.get().resultJson == "[]");
}

void testPassiveChainDiscoveryDoesNotOpenBrowser()
{
    using provider::compatibility::browser_wallet::bridge::Config;
    using provider::compatibility::browser_wallet::bridge::makeBridge;

    std::atomic_int launchCount{};
    auto bridge = makeBridge(Config{
        .launch = [&launchCount](const std::string&) -> foundation::result::Status {
            launchCount.fetch_add(1, std::memory_order_relaxed);
            return {};
        },
        .supportedChains = {"eip155:8453"},
    });
    assert(bridge);
    assert((*bridge)->beginSession("Passive app", "https://passive.example"));

    std::promise<capability::wallet::EthereumRpcResponse> chainPromise{};
    auto chainFuture = chainPromise.get_future();
    (*bridge)->request({.id=101U,.method="eth_chainId",.paramsJson="[]"},
        [&chainPromise](auto response) { chainPromise.set_value(std::move(response)); });
    assert(chainFuture.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const auto chain = chainFuture.get();
    assert(chain.error && chain.error->code == 4900);
    assert(launchCount.load(std::memory_order_relaxed) == 0);

    std::promise<capability::wallet::EthereumRpcResponse> accountsPromise{};
    auto accountsFuture = accountsPromise.get_future();
    (*bridge)->request({.id=102U,.method="eth_accounts",.paramsJson="[]"},
        [&accountsPromise](auto response) { accountsPromise.set_value(std::move(response)); });
    assert(accountsFuture.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const auto accounts = accountsFuture.get();
    assert(!accounts.error && accounts.resultJson == "[]");
    assert(launchCount.load(std::memory_order_relaxed) == 0);

    std::promise<capability::wallet::EthereumRpcResponse> connectPromise{};
    auto connectFuture = connectPromise.get_future();
    (*bridge)->request({.id=103U,.method="eth_requestAccounts",.paramsJson="[]"},
        [&connectPromise](auto response) { connectPromise.set_value(std::move(response)); });
    assert(launchCount.load(std::memory_order_relaxed) == 1);
    (*bridge)->endSession();
    assert(connectFuture.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    assert(connectFuture.get().error->code == 4900);
}

void testClosingBridgeWithHalfOpenHttpConnectionIsBounded()
{
    using provider::compatibility::browser_wallet::bridge::Config;
    using provider::compatibility::browser_wallet::bridge::makeBridge;

    std::promise<std::string> launchPromise{};
    auto launched = launchPromise.get_future();
    auto bridge = makeBridge(Config{
        .launch = [&launchPromise](const std::string& url) -> foundation::result::Status {
            launchPromise.set_value(url);
            return {};
        },
        .supportedChains = {"eip155:8453"},
    });
    assert(bridge);
    assert((*bridge)->beginSession("Closing app", "https://app.example"));

    std::promise<capability::wallet::EthereumRpcResponse> pendingPromise{};
    (*bridge)->request({.id=71U,.method="eth_requestAccounts",.paramsJson="[]"},
        [&pendingPromise](auto response) { pendingPromise.set_value(std::move(response)); });
    assert(launched.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const ParsedUrl url = parseLoopbackUrl(launched.get());

    asio::io_context io{};
    tcp::socket stalled{io};
    stalled.connect(tcp::endpoint{asio::ip::make_address_v4(url.host), url.port});
    const std::string partial = "GET " + url.target + " HTTP/1.1\r\nHost: "
        + url.host + ":" + std::to_string(url.port) + "\r\n";
    asio::write(stalled, asio::buffer(partial));
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    // A half-written browser request used to block http::read indefinitely
    // and could keep the bridge's jthread from joining on application exit.
    const auto shutdownStarted = std::chrono::steady_clock::now();
    bridge->reset();
    const auto elapsed = std::chrono::steady_clock::now() - shutdownStarted;
    assert(elapsed < std::chrono::seconds{8});
}

void testBridgeRejectsUnsupportedChainBeforeWallet()
{
    using provider::compatibility::browser_wallet::bridge::Config;
    using provider::compatibility::browser_wallet::bridge::makeBridge;

    auto bridge = makeBridge(Config{
        .launch = [](const std::string&) -> foundation::result::Status { return {}; },
        .supportedChains = {"eip155:8453"},
    });
    assert(bridge);
    assert((*bridge)->beginSession("Test Mini App", "https://mini.example"));

    std::promise<capability::wallet::EthereumRpcResponse> responsePromise{};
    auto responseFuture = responsePromise.get_future();
    (*bridge)->request(
        capability::wallet::EthereumRpcRequest{
            .id = 9U,
            .method = "wallet_switchEthereumChain",
            .paramsJson = R"([{"chainId":"0x1"}])",
        },
        [&responsePromise](capability::wallet::EthereumRpcResponse response) mutable {
            responsePromise.set_value(std::move(response));
        });
    assert(responseFuture.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const auto response = responseFuture.get();
    assert(response.error);
    assert(response.error->code == 4901);
}


void testOriginGrantRevocationAndChainChangedRequireFreshConsent()
{
    using provider::compatibility::browser_wallet::bridge::Config;
    using provider::compatibility::browser_wallet::bridge::makeBridge;

    std::promise<std::string> launchPromise{};
    auto launched = launchPromise.get_future();
    auto bridge = makeBridge(Config{
        .launch = [&launchPromise](const std::string& url) -> foundation::result::Status {
            launchPromise.set_value(url);
            return {};
        },
        .supportedChains = {"eip155:1", "eip155:8453"},
        .requestTimeout = std::chrono::seconds{3},
    });
    assert(bridge);
    assert((*bridge)->beginSession("Wallet permissions", "https://permissions.example"));

    const auto rpc = [&bridge](std::uint64_t id, std::string method,
                               std::string params = "[]") {
        std::promise<capability::wallet::EthereumRpcResponse> promise{};
        auto result = promise.get_future();
        (*bridge)->request({
            .id = id, .method = std::move(method), .paramsJson = std::move(params),
        }, [&promise](auto response) { promise.set_value(std::move(response)); });
        return std::pair{std::move(result), std::move(promise)};
    };
    auto [connect, connectCallback] = rpc(201U, "eth_requestAccounts");
    assert(launched.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const auto url = parseLoopbackUrl(launched.get());
    const auto finish = [&url](json::value completion) {
        const auto next = request(url, http::verb::get, url.target + "/next");
        assert(next.result() == http::status::ok);
        boost::system::error_code error{};
        const auto payload = json::parse(next.body(), error);
        assert(!error && payload.is_object());
        const auto value = payload.as_object().at("transportId");
        const auto id = value.is_uint64() ? value.as_uint64()
            : static_cast<std::uint64_t>(value.as_int64());
        const auto posted = request(url, http::verb::post, url.target + "/result",
            json::serialize(json::object{
                {"transportId", id},
                {"result", std::move(completion)},
            }));
        assert(posted.result() == http::status::ok);
    };
    const json::array approvedAccounts{"0x1111111111111111111111111111111111111111"};
    finish(approvedAccounts);
    assert(connect.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    assert(!connect.get().error);

    // A successful EIP-2255 revoke must clear both public-account disclosure
    // and the remembered signer/RPC grant for exactly this Mini App origin.
    auto [revoke, revokeCallback] = rpc(202U, "wallet_revokePermissions",
                                      R"([{"eth_accounts":{}}])");
    finish(json::array{});
    assert(revoke.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    assert(!revoke.get().error);

    auto [privateAccounts, privateCallback] = rpc(203U, "eth_accounts");
    assert(privateAccounts.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const auto hidden = privateAccounts.get();
    assert(!hidden.error && hidden.resultJson == "[]");

    auto [blockedSign, signCallback] = rpc(204U, "personal_sign",
                                           R"(["0x6869","0x1111111111111111111111111111111111111111"])");
    assert(blockedSign.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    assert(blockedSign.get().error->code == 4100);

    // Reconnecting is explicit, never inherited from a successful revoke.
    auto [reconnect, reconnectCallback] = rpc(205U, "eth_requestAccounts");
    finish(approvedAccounts);
    assert(reconnect.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    assert(!reconnect.get().error);

    // A chain switch invalidates the old origin/account permission even if
    // the provider retains the same public EVM address.
    const auto switchEvent = request(url, http::verb::post, url.target + "/event",
        R"({"event":"chainChanged","params":["0x2105"]})");
    assert(switchEvent.result() == http::status::ok);
    auto [chainAccounts, chainCallback] = rpc(206U, "eth_accounts");
    assert(chainAccounts.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    const auto deniedAccounts = chainAccounts.get();
    assert(!deniedAccounts.error && deniedAccounts.resultJson == "[]");
    auto [blockedSend, sendCallback] = rpc(207U, "eth_sendTransaction",
                                           R"([{"to":"0x2222222222222222222222222222222222222222"}])");
    assert(blockedSend.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    assert(blockedSend.get().error->code == 4100);
}

} // namespace

int main()
{
    testBridgeRoutesWalletRequestsAndEvents();
    testAccountAccessConsentIsRememberedPerOrigin();
    testAccountPrivacyUntilExplicitPermission();
    testOriginGrantRevocationAndChainChangedRequireFreshConsent();
    testBridgeRejectsUnsupportedChainBeforeWallet();
    testPassiveChainDiscoveryDoesNotOpenBrowser();
    testClosingBridgeWithHalfOpenHttpConnectionIsBounded();
    return 0;
}
