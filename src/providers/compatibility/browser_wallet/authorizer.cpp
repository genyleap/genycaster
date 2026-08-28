module;

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

module farcaster.provider.compatibility.browser_wallet.authorizer;

import farcaster.capability.signer;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.protocol.key_authorization;
import farcaster.protocol.message;

namespace farcaster::provider::compatibility::browser_wallet::authorizer {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using tcp = asio::ip::tcp;

constexpr std::size_t TokenBytes = 32U;
constexpr std::size_t EthereumAddressBytes = 20U;
constexpr std::size_t EcdsaSignatureBytes = 65U;
constexpr std::size_t MaximumHandledRequests = 16U;

[[nodiscard]] foundation::error::Error makeError(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] foundation::error::Error invalid(std::string message)
{
    return makeError(foundation::error::ErrorCode::InvalidArgument, std::move(message));
}

[[nodiscard]] foundation::error::Error unavailable(std::string message)
{
    return makeError(foundation::error::ErrorCode::Unavailable, std::move(message));
}

[[nodiscard]] foundation::error::Error protocolFailure(std::string message)
{
    return makeError(foundation::error::ErrorCode::ProtocolFailure, std::move(message));
}

[[nodiscard]] foundation::result::Result<std::string> randomHexToken()
{
    std::array<unsigned char, TokenBytes> bytes{};
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
        return std::unexpected(unavailable("OpenSSL failed to create a custody-approval capability token."));
    }
    constexpr char digits[] = "0123456789abcdef";
    std::string result(bytes.size() * 2U, '0');
    for (std::size_t index{}; index < bytes.size(); ++index) {
        result[index * 2U] = digits[(bytes[index] >> 4U) & 0x0FU];
        result[index * 2U + 1U] = digits[bytes[index] & 0x0FU];
    }
    return result;
}

[[nodiscard]] bool isHex(const std::string_view value)
{
    return std::all_of(value.begin(), value.end(), [](const char ch) {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
            (ch >= 'A' && ch <= 'F');
    });
}

[[nodiscard]] foundation::result::Result<std::string> decodeHex(
    const std::string_view value,
    const std::size_t requiredBytes,
    const std::string_view label)
{
    std::string_view hex = value;
    if (hex.starts_with("0x") || hex.starts_with("0X")) {
        hex.remove_prefix(2U);
    }
    if (hex.size() != requiredBytes * 2U || !isHex(hex)) {
        return std::unexpected(protocolFailure(
            std::string{label} + " must be exactly " + std::to_string(requiredBytes) +
            " bytes of hexadecimal data."));
    }

    auto nibble = [](const char ch) -> unsigned char {
        if (ch >= '0' && ch <= '9') return static_cast<unsigned char>(ch - '0');
        if (ch >= 'a' && ch <= 'f') return static_cast<unsigned char>(10 + ch - 'a');
        return static_cast<unsigned char>(10 + ch - 'A');
    };

    std::string bytes(requiredBytes, '\0');
    for (std::size_t index{}; index < requiredBytes; ++index) {
        const auto high = nibble(hex[index * 2U]);
        const auto low = nibble(hex[index * 2U + 1U]);
        bytes[index] = static_cast<char>((high << 4U) | low);
    }
    return bytes;
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

[[nodiscard]] std::string approvalPage(
    const std::string_view callbackPath,
    const std::string_view scriptNonce,
    const protocol::key_authorization::TypedDataDocuments& documents)
{
    std::string page = R"html(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Genycaster signer approval</title><style nonce=")html";
    page += scriptNonce;
    page += R"html(">:root{color-scheme:light dark;font-family:system-ui,sans-serif}body{max-width:42rem;margin:3rem auto;padding:0 1.25rem;line-height:1.45}button{font:inherit;padding:.75rem 1rem;border-radius:.65rem;border:1px solid currentColor;cursor:pointer}code{word-break:break-all}.muted{opacity:.72}.error{color:#c33}</style></head><body><h1>Enable Genycaster interactions</h1><p>Genycaster is asking your Ethereum custody wallet to authorize one Farcaster protocol signer. This creates no transaction and spends no gas.</p><p class="muted">The approval is bound to this key, scopes, TTL, nonce and deadline. The private custody key never enters Genycaster.</p><button id="approve">Approve with wallet</button><p id="status" class="muted">No wallet request has been sent yet.</p><script nonce=")html";
    page += scriptNonce;
    page += R"html(">const signedKeyRequest=)html";
    page += documents.signedKeyRequestJson;
    page += R"html(;const keyAdd=)html";
    page += documents.keyAddJson;
    page += R"html(;const callbackPath=")html";
    page += htmlEscape(callbackPath);
    page += R"html(";const status=document.getElementById('status');const reportFailure=async(error)=>{const rejected=error&&Number(error.code)===4001;try{await fetch(callbackPath,{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify({error:rejected?'rejected':'wallet_error'})});}catch(_ignored){}return rejected;};document.getElementById('approve').addEventListener('click',async()=>{try{if(!window.ethereum){const missing=new Error('No EIP-1193 wallet is available in this browser. Open this page inside a compatible wallet browser or install a wallet extension.');missing.code=-1;throw missing;}status.textContent='Waiting for wallet approval…';const accounts=await window.ethereum.request({method:'eth_requestAccounts'});if(!accounts||!accounts[0])throw new Error('The wallet returned no account.');const account=accounts[0];const signedKeyRequestSignature=await window.ethereum.request({method:'eth_signTypedData_v4',params:[account,JSON.stringify(signedKeyRequest)]});const custodySignature=await window.ethereum.request({method:'eth_signTypedData_v4',params:[account,JSON.stringify(keyAdd)]});const response=await fetch(callbackPath,{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify({account,signedKeyRequestSignature,custodySignature})});if(!response.ok)throw new Error(await response.text()||'Genycaster rejected the wallet response.');status.textContent='Signer authorization received. You can return to Genycaster.';document.getElementById('approve').disabled=true;}catch(error){const rejected=await reportFailure(error);status.className='error';status.textContent=rejected?'Wallet approval was rejected.':(error&&error.message?error.message:String(error));}});</script></body></html>)html";
    return page;
}

struct WalletResponse final {
    std::string address{};
    std::string signedKeyRequestSignature{};
    std::string custodySignature{};
};

[[nodiscard]] foundation::result::Result<std::string> jsonString(
    const json::object& object,
    const std::string_view key)
{
    const auto* value = object.if_contains(key);
    if (value == nullptr || !value->is_string()) {
        return std::unexpected(protocolFailure(
            "Wallet approval response is missing string field '" + std::string{key} + "'."));
    }
    return std::string{value->as_string()};
}

[[nodiscard]] foundation::result::Result<WalletResponse> parseWalletResponse(
    const std::string_view body)
{
    boost::system::error_code parseError{};
    auto value = json::parse(body, parseError);
    if (parseError || !value.is_object()) {
        return std::unexpected(protocolFailure("Wallet approval callback must contain a JSON object."));
    }
    const auto& object = value.as_object();
    if (const auto* errorValue = object.if_contains("error"); errorValue != nullptr) {
        if (!errorValue->is_string()) {
            return std::unexpected(protocolFailure("Wallet approval error must be a string."));
        }
        const std::string error{errorValue->as_string()};
        if (error == "rejected") {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "Custody wallet approval was rejected by the user."));
        }
        if (error == "wallet_error") {
            return std::unexpected(unavailable(
                "The wallet could not complete the custody authorization request."));
        }
        return std::unexpected(protocolFailure("Wallet approval returned an unknown error state."));
    }
    auto account = jsonString(object, "account");
    auto signedKeyRequest = jsonString(object, "signedKeyRequestSignature");
    auto custody = jsonString(object, "custodySignature");
    if (!account) return std::unexpected(std::move(account.error()));
    if (!signedKeyRequest) return std::unexpected(std::move(signedKeyRequest.error()));
    if (!custody) return std::unexpected(std::move(custody.error()));

    auto addressBytes = decodeHex(*account, EthereumAddressBytes, "Wallet account");
    auto signedKeyRequestBytes = decodeHex(
        *signedKeyRequest, EcdsaSignatureBytes, "SignedKeyRequest signature");
    auto custodyBytes = decodeHex(*custody, EcdsaSignatureBytes, "KeyAdd custody signature");
    if (!addressBytes) return std::unexpected(std::move(addressBytes.error()));
    if (!signedKeyRequestBytes) return std::unexpected(std::move(signedKeyRequestBytes.error()));
    if (!custodyBytes) return std::unexpected(std::move(custodyBytes.error()));

    return WalletResponse{
        .address = std::move(*addressBytes),
        .signedKeyRequestSignature = std::move(*signedKeyRequestBytes),
        .custodySignature = std::move(*custodyBytes),
    };
}

[[nodiscard]] foundation::result::Status validateConfig(const Config& config)
{
    if (!config.launch) {
        return std::unexpected(invalid("Browser-wallet custody authorizer requires an approval URL launcher."));
    }
    if (config.approvalTimeout <= std::chrono::milliseconds::zero() ||
        config.requestTimeout <= std::chrono::milliseconds::zero()) {
        return std::unexpected(invalid("Browser-wallet custody authorizer timeouts must be positive."));
    }
    if (config.maxRequestBytes < 1024U || config.maxRequestBytes > 1024U * 1024U) {
        return std::unexpected(invalid("Browser-wallet custody authorizer request bound must be between 1 KiB and 1 MiB."));
    }
    return {};
}

class BrowserWalletAuthorizer final : public capability::signer::ICustodyAuthorizer {
public:
    explicit BrowserWalletAuthorizer(Config config) : m_config(std::move(config)) {}

    [[nodiscard]] foundation::result::Result<capability::signer::KeyAddAuthorization> authorize(
        const capability::signer::KeyAddRequest& request) override
    {
        if (request.fid.value == 0U || request.requestFid.value == 0U) {
            return std::unexpected(invalid("Custody authorization requires non-zero user and request FIDs."));
        }
        if (request.requestFid.value != request.fid.value) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unsupported,
                "This custody authorizer supports self-attributed signer requests only; requestFid must equal the signed-in FID."));
        }

        auto farcasterDeadline = protocol::message::toFarcasterTimestamp(request.deadline);
        if (!farcasterDeadline) return std::unexpected(std::move(farcasterDeadline.error()));
        const protocol::key_authorization::KeyAddInput typedInput{
            .fid = request.fid,
            .publicKey = request.publicKey,
            .scopes = request.scopes,
            .ttlSeconds = request.ttlSeconds,
            .nonce = request.nonce,
            .deadline = *farcasterDeadline,
            .requestFid = request.requestFid,
        };
        auto documents = protocol::key_authorization::buildTypedData(typedInput);
        if (!documents) return std::unexpected(std::move(documents.error()));

        auto token = randomHexToken();
        auto nonce = randomHexToken();
        if (!token) return std::unexpected(std::move(token.error()));
        if (!nonce) return std::unexpected(std::move(nonce.error()));

        asio::io_context context{};
        boost::system::error_code ec{};
        tcp::acceptor acceptor{context};
        acceptor.open(tcp::v4(), ec);
        if (ec) return std::unexpected(unavailable("Could not open the local custody-approval listener."));
        acceptor.set_option(asio::socket_base::reuse_address(false), ec);
        acceptor.bind(tcp::endpoint{asio::ip::make_address_v4("127.0.0.1"), 0U}, ec);
        if (ec) return std::unexpected(unavailable("Could not bind the local custody-approval listener to loopback."));
        acceptor.listen(asio::socket_base::max_listen_connections, ec);
        if (ec) return std::unexpected(unavailable("Could not listen for the local custody-approval callback."));
        acceptor.non_blocking(true, ec);
        if (ec) return std::unexpected(unavailable("Could not configure the local custody-approval listener."));

        const auto port = acceptor.local_endpoint(ec).port();
        if (ec || port == 0U) return std::unexpected(unavailable("Could not resolve the local custody-approval port."));
        const std::string approvalPath = "/approve/" + *token;
        const std::string callbackPath = "/callback/" + *token;
        const std::string approvalUrl = "http://127.0.0.1:" + std::to_string(port) + approvalPath;

        if (auto launched = m_config.launch(approvalUrl); !launched) {
            return std::unexpected(std::move(launched.error()));
        }

        const auto deadline = std::chrono::steady_clock::now() + m_config.approvalTimeout;
        std::size_t handled{};
        while (std::chrono::steady_clock::now() < deadline && handled < MaximumHandledRequests) {
            tcp::socket socket{context};
            acceptor.accept(socket, ec);
            if (ec == asio::error::would_block || ec == asio::error::try_again) {
                ec.clear();
                std::this_thread::sleep_for(std::chrono::milliseconds{20});
                continue;
            }
            if (ec) {
                return std::unexpected(unavailable("The local custody-approval listener failed while accepting a callback."));
            }
            ++handled;

            beast::tcp_stream stream{std::move(socket)};
            stream.expires_after(m_config.requestTimeout);
            beast::flat_buffer buffer{};
            http::request_parser<http::string_body> parser{};
            parser.body_limit(m_config.maxRequestBytes);
            http::read(stream, buffer, parser, ec);
            if (ec) {
                beast::error_code ignored{};
                stream.socket().shutdown(tcp::socket::shutdown_both, ignored);
                continue;
            }
            auto requestMessage = parser.release();

            auto writeResponse = [&](const http::status status,
                                     std::string body,
                                     const std::string_view contentType) {
                http::response<http::string_body> response{status, requestMessage.version()};
                response.set(http::field::server, "Genycaster");
                response.set(http::field::cache_control, "no-store, max-age=0");
                response.set(http::field::pragma, "no-cache");
                response.set(http::field::content_type, contentType);
                response.keep_alive(false);
                response.body() = std::move(body);
                response.prepare_payload();
                http::write(stream, response, ec);
                beast::error_code ignored{};
                stream.socket().shutdown(tcp::socket::shutdown_both, ignored);
            };

            const std::string target{requestMessage.target()};
            if (requestMessage.method() == http::verb::get && target == approvalPath) {
                auto page = approvalPage(callbackPath, *nonce, *documents);
                http::response<http::string_body> response{http::status::ok, requestMessage.version()};
                response.set(http::field::server, "Genycaster");
                response.set(http::field::cache_control, "no-store, max-age=0");
                response.set(http::field::pragma, "no-cache");
                response.set(http::field::content_type, "text/html; charset=utf-8");
                response.set("Referrer-Policy", "no-referrer");
                response.set("X-Content-Type-Options", "nosniff");
                response.set("X-Frame-Options", "DENY");
                response.set(
                    "Content-Security-Policy",
                    "default-src 'none'; script-src 'nonce-" + *nonce +
                        "'; style-src 'nonce-" + *nonce +
                        "'; connect-src 'self'; img-src 'none'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'");
                response.keep_alive(false);
                response.body() = std::move(page);
                response.prepare_payload();
                http::write(stream, response, ec);
                beast::error_code ignored{};
                stream.socket().shutdown(tcp::socket::shutdown_both, ignored);
                continue;
            }

            if (requestMessage.method() == http::verb::post && target == callbackPath) {
                const auto contentType = requestMessage[http::field::content_type];
                if (!std::string_view{contentType.data(), contentType.size()}.starts_with("application/json")) {
                    writeResponse(http::status::unsupported_media_type,
                        "Expected application/json.", "text/plain; charset=utf-8");
                    continue;
                }
                auto wallet = parseWalletResponse(requestMessage.body());
                if (!wallet) {
                    writeResponse(http::status::bad_request,
                        wallet.error().message, "text/plain; charset=utf-8");
                    return std::unexpected(std::move(wallet.error()));
                }
                auto metadata = protocol::key_authorization::encodeSignedKeyRequestMetadata(
                    request.requestFid,
                    wallet->address,
                    wallet->signedKeyRequestSignature,
                    *farcasterDeadline);
                if (!metadata) {
                    writeResponse(http::status::bad_request,
                        metadata.error().message, "text/plain; charset=utf-8");
                    return std::unexpected(std::move(metadata.error()));
                }
                writeResponse(http::status::ok, "Authorization received.", "text/plain; charset=utf-8");
                return capability::signer::KeyAddAuthorization{
                    .custodySignature = std::move(wallet->custodySignature),
                    .signedKeyRequestMetadata = std::move(*metadata),
                };
            }

            writeResponse(http::status::not_found, "Not found.", "text/plain; charset=utf-8");
        }

        return std::unexpected(makeError(
            foundation::error::ErrorCode::Timeout,
            "Custody wallet approval did not complete before the one-shot authorization expired."));
    }

private:
    Config m_config;
};

}

foundation::result::Result<std::unique_ptr<capability::signer::ICustodyAuthorizer>>
makeAuthorizer(Config config)
{
    if (auto valid = validateConfig(config); !valid) {
        return std::unexpected(std::move(valid.error()));
    }
    std::unique_ptr<capability::signer::ICustodyAuthorizer> result =
        std::make_unique<BrowserWalletAuthorizer>(std::move(config));
    return result;
}

}
