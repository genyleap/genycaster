module;

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

module farcaster.provider.compatibility.browser_wallet.bridge;

import farcaster.capability.wallet;
import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::provider::compatibility::browser_wallet::bridge {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using tcp = asio::ip::tcp;
using Clock = std::chrono::steady_clock;

constexpr std::size_t TokenBytes = 32U;
constexpr std::size_t MaximumQueuedRequests = 32U;
constexpr std::chrono::milliseconds PollDelay{20};

[[nodiscard]] foundation::error::Error makeError(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] foundation::result::Result<std::string> randomHex(const std::size_t bytes)
{
    std::vector<unsigned char> buffer(bytes);
    if (RAND_bytes(buffer.data(), static_cast<int>(buffer.size())) != 1) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unavailable,
            "OpenSSL failed to create an EIP-1193 wallet-session capability token."));
    }
    constexpr char digits[] = "0123456789abcdef";
    std::string result(buffer.size() * 2U, '0');
    for (std::size_t index{}; index < buffer.size(); ++index) {
        result[index * 2U] = digits[(buffer[index] >> 4U) & 0x0FU];
        result[index * 2U + 1U] = digits[buffer[index] & 0x0FU];
    }
    return result;
}

[[nodiscard]] std::string htmlEscape(const std::string_view value)
{
    std::string escaped{};
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '"': escaped += "&quot;"; break;
        case '\'': escaped += "&#39;"; break;
        default: escaped.push_back(ch); break;
        }
    }
    return escaped;
}

[[nodiscard]] bool validMethod(const std::string_view method)
{
    return method == "personal_sign" || method == "net_version" || method == "web3_clientVersion"
        || method.starts_with("eth_") || method.starts_with("wallet_");
}

[[nodiscard]] bool sensitiveMethod(
    const std::string_view method,
    const bool hasAccountAccessGrant)
{
    if (method == "eth_requestAccounts") {
        return !hasAccountAccessGrant;
    }
    if (method == "personal_sign"
        || method == "eth_decrypt" || method == "eth_getEncryptionPublicKey") {
        return true;
    }
    if (method.starts_with("eth_sign") || method.starts_with("eth_send")) {
        return true;
    }
    if (!method.starts_with("wallet_")) {
        return false;
    }
    return method != "wallet_getCapabilities" && method != "wallet_getPermissions"
        && method != "wallet_getCallsStatus";
}

[[nodiscard]] std::optional<std::string> requestedChainId(
    const std::string_view method,
    const std::string_view paramsJson)
{
    if (method != "wallet_switchEthereumChain" && method != "wallet_addEthereumChain") {
        return std::nullopt;
    }
    boost::system::error_code error{};
    const auto parsed = json::parse(paramsJson, error);
    if (error || !parsed.is_array() || parsed.as_array().empty()
        || !parsed.as_array().front().is_object()) {
        return std::string{};
    }
    const auto& object = parsed.as_array().front().as_object();
    const auto* value = object.if_contains("chainId");
    if (value == nullptr || !value->is_string()) return std::string{};
    return std::string{value->as_string()};
}

[[nodiscard]] std::optional<std::uint64_t> parseHexChainId(std::string_view value)
{
    if (!value.starts_with("0x") && !value.starts_with("0X")) return std::nullopt;
    value.remove_prefix(2U);
    if (value.empty() || value.size() > 16U) return std::nullopt;
    std::uint64_t result{};
    for (const char ch : value) {
        std::uint64_t digit{};
        if (ch >= '0' && ch <= '9') digit = static_cast<std::uint64_t>(ch - '0');
        else if (ch >= 'a' && ch <= 'f') digit = static_cast<std::uint64_t>(10 + ch - 'a');
        else if (ch >= 'A' && ch <= 'F') digit = static_cast<std::uint64_t>(10 + ch - 'A');
        else return std::nullopt;
        if (result > (UINT64_MAX - digit) / 16U) return std::nullopt;
        result = result * 16U + digit;
    }
    return result;
}

[[nodiscard]] bool configuredChain(
    const std::vector<std::string>& supportedChains,
    const std::string_view hexadecimalChainId)
{
    const auto numeric = parseHexChainId(hexadecimalChainId);
    if (!numeric) return false;
    const std::string caip = "eip155:" + std::to_string(*numeric);
    return std::ranges::find(supportedChains, caip) != supportedChains.end();
}

[[nodiscard]] capability::wallet::EthereumRpcResponse rpcError(
    const std::uint64_t id,
    const std::int32_t code,
    std::string message,
    std::string details = {})
{
    capability::wallet::EthereumRpcResponse response{};
    response.id = id;
    response.error = capability::wallet::EthereumRpcError{
        .code = code, .message = std::move(message), .details = std::move(details)};
    return response;
}

[[nodiscard]] std::optional<capability::wallet::EthereumWalletEventKind> eventKind(
    const std::string_view name)
{
    using Kind = capability::wallet::EthereumWalletEventKind;
    if (name == "accountsChanged") return Kind::AccountsChanged;
    if (name == "chainChanged") return Kind::ChainChanged;
    if (name == "connect") return Kind::Connect;
    if (name == "disconnect") return Kind::Disconnect;
    if (name == "message") return Kind::Message;
    return std::nullopt;
}

[[nodiscard]] std::string bridgePage(
    const std::string_view basePath,
    const std::string_view nonce,
    const std::string_view appName,
    const std::string_view origin)
{
    std::string page = R"html(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Genycaster wallet bridge</title><style nonce=")html";
    page += nonce;
    page += R"html(">:root{color-scheme:light dark;font-family:ui-sans-serif,system-ui,sans-serif}body{max-width:46rem;margin:2.5rem auto;padding:0 1.25rem;line-height:1.45}.card{border:1px solid color-mix(in srgb,currentColor 18%,transparent);border-radius:1.25rem;padding:1.25rem;margin:1rem 0}.muted{opacity:.7}code,pre{font-family:ui-monospace,SFMono-Regular,monospace;overflow-wrap:anywhere}pre{white-space:pre-wrap;max-height:16rem;overflow:auto;background:color-mix(in srgb,currentColor 6%,transparent);padding:.75rem;border-radius:.75rem}button{font:inherit;padding:.75rem 1rem;border-radius:.75rem;border:1px solid currentColor;cursor:pointer}.danger{font-weight:700}.error{color:#d33}.ok{color:#299b54}#request{display:none}</style></head><body><h1>Genycaster wallet bridge</h1><div class="card"><div><strong>Mini App</strong>: )html";
    page += htmlEscape(appName);
    page += R"html(</div><div><strong>Origin</strong>: <code>)html";
    page += htmlEscape(origin);
    page += R"html(</code></div><div id="wallet" class="muted">Checking wallet…</div></div><p class="muted">Genycaster never receives your private key. Signing and transaction requests are handed to the EIP-1193 wallet installed in this browser.</p><div id="request" class="card"><div id="kind" class="danger"></div><h2 id="method"></h2><pre id="params"></pre><button id="approve">Continue to wallet</button></div><p id="status" class="muted">Waiting for a Mini App wallet request…</p><script nonce=")html";
    page += nonce;
    page += R"html(">const base=")html";
    page += basePath;
    page += R"html(";const status=document.getElementById('status');const wallet=document.getElementById('wallet');const box=document.getElementById('request');const approve=document.getElementById('approve');let current=null;let busy=false;const post=async(path,data)=>{try{await fetch(base+path,{method:'POST',headers:{'content-type':'application/json'},cache:'no-store',body:JSON.stringify(data)});}catch(_ignored){}};const describeWallet=async()=>{if(!window.ethereum){wallet.className='error';wallet.textContent='No EIP-1193 wallet detected. Open this URL in a wallet browser or a browser with a wallet extension.';return;}try{const accounts=await window.ethereum.request({method:'eth_accounts'});const chain=await window.ethereum.request({method:'eth_chainId'});wallet.className='ok';wallet.textContent='Wallet detected · '+chain+(accounts&&accounts[0]?' · '+accounts[0]:' · not connected');}catch(error){wallet.className='error';wallet.textContent=error&&error.message?error.message:String(error);}};const sendEvent=(event,params)=>post('/event',{event,params});if(window.ethereum&&window.ethereum.on){for(const event of ['accountsChanged','chainChanged','connect','disconnect','message'])window.ethereum.on(event,(value)=>{sendEvent(event,[value]);describeWallet();});}const execute=async()=>{if(!current||busy)return;if(!window.ethereum){status.className='error';status.textContent='No EIP-1193 wallet is available.';return;}busy=true;approve.disabled=true;try{status.className='muted';status.textContent='Waiting for wallet…';const result=await window.ethereum.request({method:current.method,params:current.params});await post('/result',{transportId:current.transportId,result:result===undefined?null:result});status.className='ok';status.textContent='Wallet request completed.';}catch(error){const code=error&&Number.isInteger(Number(error.code))?Number(error.code):4001;let details='';try{if(error&&error.data!==undefined)details=typeof error.data==='string'?error.data:JSON.stringify(error.data);}catch(_ignored){}await post('/result',{transportId:current.transportId,error:{code,message:error&&error.message?error.message:String(error),details}});status.className=code===4001?'muted':'error';status.textContent=code===4001?'Wallet request rejected.':'Wallet request failed.';}finally{current=null;busy=false;approve.disabled=false;box.style.display='none';describeWallet();}};approve.addEventListener('click',execute);const poll=async()=>{if(busy||current){setTimeout(poll,500);return;}try{const response=await fetch(base+'/next',{cache:'no-store'});if(response.status===204){setTimeout(poll,650);return;}if(!response.ok)throw new Error(await response.text());current=await response.json();document.getElementById('method').textContent=current.method;const hex=current.method==='personal_sign'&&Array.isArray(current.params)&&typeof current.params[0]==='string'?current.params[0]:'';let printable='';if(/^0x(?:[0-9a-fA-F]{2}){1,4096}$/.test(hex)){try{printable=new TextDecoder('utf-8',{fatal:true}).decode(Uint8Array.from(hex.slice(2).match(/../g),v=>parseInt(v,16)));}catch(_ignored){}}document.getElementById('params').textContent=printable||JSON.stringify(current.params,null,2);const rememberedAccess=current.method==='eth_requestAccounts'&&!current.requiresApproval;document.getElementById('kind').textContent=current.requiresApproval?'Explicit approval required':rememberedAccess?'Previously approved account access':'Read-only wallet request';box.style.display='block';status.className='muted';status.textContent=current.requiresApproval?'Review this request before continuing.':rememberedAccess?'Using the account access you already approved for this Mini App.':'Routing read-only request to the wallet…';if(!current.requiresApproval)execute();}catch(error){status.className='error';status.textContent=error&&error.message?error.message:String(error);}setTimeout(poll,500);};describeWallet();poll();</script></body></html>)html";
    return page;
}

struct Pending final {
    std::uint64_t transportId{};
    capability::wallet::EthereumRpcRequest request{};
    capability::wallet::EthereumRpcCompletion completion{};
    Clock::time_point created{};
    std::string origin{};
    bool delivered{};
};

class Bridge final : public capability::wallet::IEthereumWalletBridge {
public:
    explicit Bridge(Config config, std::shared_ptr<asio::io_context> io, tcp::acceptor acceptor, std::string token, std::string nonce)
        : m_config(std::move(config)), m_io(std::move(io)), m_acceptor(std::move(acceptor)), m_token(std::move(token)),
          m_nonce(std::move(nonce)), m_port(m_acceptor.local_endpoint().port()),
          m_worker([this](const std::stop_token stopToken) { run(stopToken); })
    {
    }

    ~Bridge() override {
        endSession();
        m_worker.request_stop();
    }

    [[nodiscard]] bool available() const noexcept override {
        return static_cast<bool>(m_config.launch) && !m_config.supportedChains.empty();
    }

    [[nodiscard]] std::vector<std::string> supportedChains() const override {
        return m_config.supportedChains;
    }

    [[nodiscard]] foundation::result::Status beginSession(
        std::string appName,
        std::string origin) override
    {
        if (appName.empty() || origin.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "A Mini App wallet session requires a non-empty application name and origin."));
        }
        endSession();
        auto token = randomHex(TokenBytes);
        auto nonce = randomHex(16U);
        if (!token) return std::unexpected(token.error());
        if (!nonce) return std::unexpected(nonce.error());
        std::scoped_lock lock{m_mutex};
        m_token = std::move(*token);
        m_nonce = std::move(*nonce);
        m_appName = std::move(appName);
        m_origin = std::move(origin);
        m_active = true;
        m_pageLaunched = false;
        ++m_sessionGeneration;
        return {};
    }

    void endSession() noexcept override {
        std::vector<Pending> cancelled{};
        {
            std::scoped_lock lock{m_mutex};
            m_active = false;
            m_pageLaunched = false;
            m_appName.clear();
            m_origin.clear();
            ++m_sessionGeneration;
            for (auto& [id, pending] : m_pending) cancelled.push_back(std::move(pending));
            m_pending.clear();
        }
        for (auto& pending : cancelled) {
            if (pending.completion) {
                pending.completion(rpcError(
                    pending.request.id, 4900, "The Genycaster wallet session was closed."));
            }
        }
    }

    void setEventSink(capability::wallet::EthereumWalletEventSink sink) override {
        std::scoped_lock lock{m_mutex};
        m_eventSink = std::move(sink);
    }

    void request(
        capability::wallet::EthereumRpcRequest request,
        capability::wallet::EthereumRpcCompletion completion) override
    {
        if (!completion) return;
        if (!validMethod(request.method)) {
            completion(rpcError(request.id, 4200, "Unsupported EIP-1193 method."));
            return;
        }
        if (request.paramsJson.size() > m_config.maxRequestBytes) {
            completion(rpcError(request.id, 4200, "Wallet request exceeds the host size limit."));
            return;
        }
        boost::system::error_code parseError{};
        const auto params = json::parse(request.paramsJson, parseError);
        if (parseError || !params.is_array()) {
            completion(rpcError(request.id, 4200, "Wallet request params must be a JSON array."));
            return;
        }
        if (const auto chain = requestedChainId(request.method, request.paramsJson); chain) {
            if (chain->empty() || !configuredChain(m_config.supportedChains, *chain)) {
                completion(rpcError(
                    request.id, 4901, "The requested Ethereum chain is not enabled by this host."));
                return;
            }
        }

        std::string launchUrl{};
        std::uint64_t transportId{};
        std::optional<capability::wallet::EthereumRpcResponse> immediateError{};
        {
            std::scoped_lock lock{m_mutex};
            if (!m_active || !available()) {
                immediateError = rpcError(
                    request.id, 4900, "No active Genycaster wallet session is available.");
            } else if (m_pending.size() >= MaximumQueuedRequests) {
                immediateError = rpcError(request.id, 4900, "Too many wallet requests are pending.");
            } else {
                transportId = ++m_nextTransportId;
                m_pending.emplace(transportId, Pending{
                    .transportId = transportId,
                    .request = std::move(request),
                    .completion = std::move(completion),
                    .created = Clock::now(),
                    .origin = m_origin,
                    .delivered = false,
                });
                if (!m_pageLaunched) {
                    m_pageLaunched = true;
                    launchUrl = sessionUrlLocked();
                }
            }
        }
        if (immediateError) {
            completion(std::move(*immediateError));
            return;
        }
        if (!launchUrl.empty()) {
            auto launched = m_config.launch(launchUrl);
            if (!launched) {
                Pending failed{};
                bool found = false;
                {
                    std::scoped_lock lock{m_mutex};
                    m_pageLaunched = false;
                    const auto position = m_pending.find(transportId);
                    if (position != m_pending.end()) {
                        failed = std::move(position->second);
                        m_pending.erase(position);
                        found = true;
                    }
                }
                if (found && failed.completion) {
                    failed.completion(rpcError(
                        failed.request.id, 4900, "Genycaster could not open the configured wallet browser.",
                        launched.error().message));
                }
            }
        }
    }

private:
    [[nodiscard]] std::string basePathLocked() const {
        return "/session/" + m_token;
    }

    [[nodiscard]] std::string sessionUrlLocked() const {
        return "http://127.0.0.1:" + std::to_string(m_port) + basePathLocked();
    }

    template <typename Body>
    void secureHeaders(http::response<Body>& response, const std::string_view nonce = {}) const {
        response.set(http::field::cache_control, "no-store, max-age=0");
        response.set(http::field::pragma, "no-cache");
        response.set("Referrer-Policy", "no-referrer");
        response.set("X-Content-Type-Options", "nosniff");
        response.set("X-Frame-Options", "DENY");
        if (!nonce.empty()) {
            response.set("Content-Security-Policy",
                "default-src 'none'; connect-src 'self'; script-src 'nonce-" + std::string{nonce}
                    + "'; style-src 'nonce-" + std::string{nonce} + "'; base-uri 'none'; frame-ancestors 'none'");
        }
    }

    [[nodiscard]] bool authorizedTarget(const std::string_view target, std::string& suffix) const {
        std::scoped_lock lock{m_mutex};
        if (!m_active) return false;
        const std::string base = basePathLocked();
        if (!target.starts_with(base)) return false;
        suffix.assign(target.substr(base.size()));
        return suffix.empty() || suffix == "/next" || suffix == "/result" || suffix == "/event";
    }

    void run(const std::stop_token stopToken) {
        boost::system::error_code error{};
        m_acceptor.non_blocking(true, error);
        while (!stopToken.stop_requested()) {
            expireRequests();
            tcp::socket socket{m_acceptor.get_executor()};
            m_acceptor.accept(socket, error);
            if (error == asio::error::would_block || error == asio::error::try_again) {
                error.clear();
                std::this_thread::sleep_for(PollDelay);
                continue;
            }
            if (error) {
                error.clear();
                std::this_thread::sleep_for(PollDelay);
                continue;
            }
            handle(std::move(socket));
        }
    }

    void expireRequests() {
        std::vector<Pending> expired{};
        const auto now = Clock::now();
        {
            std::scoped_lock lock{m_mutex};
            for (auto it = m_pending.begin(); it != m_pending.end();) {
                if (now - it->second.created >= m_config.requestTimeout) {
                    expired.push_back(std::move(it->second));
                    it = m_pending.erase(it);
                } else {
                    ++it;
                }
            }
        }
        for (auto& pending : expired) {
            if (pending.completion) {
                pending.completion(rpcError(
                    pending.request.id, 4900, "The wallet request timed out before completion."));
            }
        }
    }

    void handle(tcp::socket socket) {
        beast::flat_buffer buffer{};
        http::request_parser<http::string_body> parser{};
        parser.body_limit(m_config.maxRequestBytes);
        boost::system::error_code error{};
        http::read(socket, buffer, parser, error);
        if (error) return;
        const auto request = parser.release();
        const std::string target{request.target()};
        std::string suffix{};
        if (!authorizedTarget(target, suffix)) {
            writeText(socket, http::status::not_found, "Not found.");
            return;
        }
        if (request.method() == http::verb::get && suffix.empty()) {
            std::string nonce{};
            std::string appName{};
            std::string origin{};
            std::string base{};
            {
                std::scoped_lock lock{m_mutex};
                nonce = m_nonce;
                appName = m_appName;
                origin = m_origin;
                base = basePathLocked();
            }
            http::response<http::string_body> response{http::status::ok, request.version()};
            response.set(http::field::content_type, "text/html; charset=utf-8");
            secureHeaders(response, nonce);
            response.body() = bridgePage(base, nonce, appName, origin);
            response.prepare_payload();
            http::write(socket, response, error);
            return;
        }
        if (request.method() == http::verb::get && suffix == "/next") {
            writeNext(socket, request.version());
            return;
        }
        if (request.method() == http::verb::post && suffix == "/result") {
            receiveResult(socket, request.version(), request.body());
            return;
        }
        if (request.method() == http::verb::post && suffix == "/event") {
            receiveEvent(socket, request.version(), request.body());
            return;
        }
        writeText(socket, http::status::method_not_allowed, "Unsupported method.");
    }

    void writeNext(tcp::socket& socket, const unsigned version) {
        std::optional<json::object> payload{};
        {
            std::scoped_lock lock{m_mutex};
            for (auto& [id, pending] : m_pending) {
                if (pending.delivered) continue;
                pending.delivered = true;
                boost::system::error_code error{};
                auto params = json::parse(pending.request.paramsJson, error);
                if (error || !params.is_array()) params = json::array{};
                payload = json::object{
                    {"transportId", id},
                    {"method", pending.request.method},
                    {"params", std::move(params)},
                    {"requiresApproval", sensitiveMethod(
                        pending.request.method,
                        m_accountAccessGrants.contains(pending.origin))},
                };
                break;
            }
        }
        if (!payload) {
            http::response<http::empty_body> response{http::status::no_content, version};
            secureHeaders(response);
            boost::system::error_code error{};
            http::write(socket, response, error);
            return;
        }
        writeJson(socket, version, *payload);
    }

    void receiveResult(tcp::socket& socket, const unsigned, const std::string_view body) {
        boost::system::error_code parseError{};
        const auto parsed = json::parse(body, parseError);
        if (parseError || !parsed.is_object()) {
            writeText(socket, http::status::bad_request, "Invalid result payload.");
            return;
        }
        const auto& object = parsed.as_object();
        const auto* idValue = object.if_contains("transportId");
        if (idValue == nullptr || (!idValue->is_uint64() && !idValue->is_int64())) {
            writeText(socket, http::status::bad_request, "Missing transport id.");
            return;
        }
        const std::uint64_t transportId = idValue->is_uint64()
            ? idValue->as_uint64() : static_cast<std::uint64_t>(std::max<std::int64_t>(0, idValue->as_int64()));
        Pending pending{};
        bool found = false;
        {
            std::scoped_lock lock{m_mutex};
            const auto position = m_pending.find(transportId);
            if (position != m_pending.end()) {
                pending = std::move(position->second);
                m_pending.erase(position);
                found = true;
            }
        }
        if (!found) {
            writeText(socket, http::status::gone, "Wallet request is no longer active.");
            return;
        }

        capability::wallet::EthereumRpcResponse response{};
        response.id = pending.request.id;
        if (const auto* errorValue = object.if_contains("error"); errorValue != nullptr && errorValue->is_object()) {
            const auto& failure = errorValue->as_object();
            std::int32_t code = 4001;
            if (const auto* value = failure.if_contains("code"); value != nullptr && value->is_int64()) {
                code = static_cast<std::int32_t>(value->as_int64());
            }
            std::string message{"Wallet request failed."};
            if (const auto* value = failure.if_contains("message"); value != nullptr && value->is_string()) {
                message = std::string{value->as_string()};
            }
            std::string details{};
            if (const auto* value = failure.if_contains("details"); value != nullptr && value->is_string()) {
                details = std::string{value->as_string()};
            }
            response.error = capability::wallet::EthereumRpcError{
                .code = code, .message = std::move(message), .details = std::move(details)};
        } else if (const auto* result = object.if_contains("result")) {
            response.resultJson = json::serialize(*result);
            if (pending.request.method == "eth_requestAccounts"
                && result->is_array() && !result->as_array().empty()) {
                std::scoped_lock lock{m_mutex};
                m_accountAccessGrants.insert(pending.origin);
            }
        } else {
            response.error = capability::wallet::EthereumRpcError{
                .code = 4200, .message = "Wallet response omitted both result and error.", .details = {}};
        }
        if (pending.completion) pending.completion(std::move(response));
        writeText(socket, http::status::ok, "ok");
    }

    void receiveEvent(tcp::socket& socket, const unsigned, const std::string_view body) {
        boost::system::error_code parseError{};
        const auto parsed = json::parse(body, parseError);
        if (parseError || !parsed.is_object()) {
            writeText(socket, http::status::bad_request, "Invalid event payload.");
            return;
        }
        const auto& object = parsed.as_object();
        const auto* name = object.if_contains("event");
        const auto* params = object.if_contains("params");
        if (name == nullptr || !name->is_string() || params == nullptr || !params->is_array()) {
            writeText(socket, http::status::bad_request, "Invalid event fields.");
            return;
        }
        const auto kind = eventKind(name->as_string());
        if (!kind) {
            writeText(socket, http::status::bad_request, "Unsupported provider event.");
            return;
        }
        capability::wallet::EthereumWalletEventSink sink{};
        {
            std::scoped_lock lock{m_mutex};
            if (!m_active) {
                writeText(socket, http::status::gone, "Wallet session is closed.");
                return;
            }
            const bool disconnected = *kind == capability::wallet::EthereumWalletEventKind::Disconnect;
            const bool accountsCleared = *kind == capability::wallet::EthereumWalletEventKind::AccountsChanged
                && !params->as_array().empty()
                && params->as_array().front().is_array()
                && params->as_array().front().as_array().empty();
            if (disconnected || accountsCleared) {
                m_accountAccessGrants.erase(m_origin);
            }
            sink = m_eventSink;
        }
        if (sink) {
            sink(capability::wallet::EthereumWalletEvent{
                .kind = *kind,
                .paramsJson = json::serialize(*params),
            });
        }
        writeText(socket, http::status::ok, "ok");
    }

    void writeText(tcp::socket& socket, const http::status status, std::string body) const {
        http::response<http::string_body> response{status, 11};
        response.set(http::field::content_type, "text/plain; charset=utf-8");
        secureHeaders(response);
        response.body() = std::move(body);
        response.prepare_payload();
        boost::system::error_code error{};
        http::write(socket, response, error);
    }

    void writeJson(tcp::socket& socket, const unsigned version, const json::value& value) const {
        http::response<http::string_body> response{http::status::ok, version};
        response.set(http::field::content_type, "application/json; charset=utf-8");
        secureHeaders(response);
        response.body() = json::serialize(value);
        response.prepare_payload();
        boost::system::error_code error{};
        http::write(socket, response, error);
    }

    Config m_config{};
    mutable std::mutex m_mutex{};
    std::shared_ptr<asio::io_context> m_io{};
    tcp::acceptor m_acceptor;
    std::string m_token{};
    std::string m_nonce{};
    std::string m_appName{};
    std::string m_origin{};
    std::uint16_t m_port{};
    std::uint64_t m_nextTransportId{};
    std::uint64_t m_sessionGeneration{};
    bool m_active{};
    bool m_pageLaunched{};
    std::map<std::uint64_t, Pending> m_pending{};
    std::set<std::string, std::less<>> m_accountAccessGrants{};
    capability::wallet::EthereumWalletEventSink m_eventSink{};
    std::jthread m_worker{};
};

} // namespace

foundation::result::Result<std::unique_ptr<capability::wallet::IEthereumWalletBridge>>
makeBridge(Config config)
{
    if (!config.launch) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "The EIP-1193 browser wallet bridge requires a launch callback."));
    }
    if (config.supportedChains.empty()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "The EIP-1193 browser wallet bridge requires at least one supported EVM chain."));
    }
    std::ranges::sort(config.supportedChains);
    config.supportedChains.erase(
        std::ranges::unique(config.supportedChains).begin(), config.supportedChains.end());
    for (const auto& chain : config.supportedChains) {
        if (!chain.starts_with("eip155:") || chain.size() <= 7U
            || !std::ranges::all_of(std::string_view{chain}.substr(7U), [](const char ch) {
                return ch >= '0' && ch <= '9';
            })) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Mini App wallet chains must use numeric eip155 CAIP-2 identifiers."));
        }
    }
    if (config.maxRequestBytes < 1024U || config.maxRequestBytes > 256U * 1024U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Mini App wallet request limit must be between 1 KiB and 256 KiB."));
    }
    if (config.requestTimeout <= std::chrono::milliseconds::zero()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Mini App wallet request timeout must be positive."));
    }

    auto token = randomHex(TokenBytes);
    auto nonce = randomHex(16U);
    if (!token) return std::unexpected(token.error());
    if (!nonce) return std::unexpected(nonce.error());

    auto io = std::make_shared<asio::io_context>();
    boost::system::error_code error{};
    tcp::acceptor acceptor{*io};
    acceptor.open(tcp::v4(), error);
    if (error) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unavailable,
            "Could not create the Mini App wallet loopback listener."));
    }
    acceptor.set_option(asio::socket_base::reuse_address(false), error);
    acceptor.bind(tcp::endpoint{asio::ip::make_address_v4("127.0.0.1"), 0}, error);
    if (error) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unavailable,
            "Could not bind the Mini App wallet loopback listener."));
    }
    acceptor.listen(asio::socket_base::max_listen_connections, error);
    if (error) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unavailable,
            "Could not listen for Mini App wallet loopback requests."));
    }

    // Move the acceptor together with its executor. The Bridge keeps the IO context
    // alive for the listener's complete lifetime.
    auto made = std::make_unique<Bridge>(std::move(config), std::move(io), std::move(acceptor), std::move(*token), std::move(*nonce));
    return std::unique_ptr<capability::wallet::IEthereumWalletBridge>{std::move(made)};
}

} // namespace farcaster::provider::compatibility::browser_wallet::bridge
