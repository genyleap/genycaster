module;

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

export module farcaster.capability.wallet;

import farcaster.foundation.result;

export namespace farcaster::capability::wallet {

/** @brief Provider-neutral Ethereum JSON-RPC request owned by a Mini App wallet session. */
struct EthereumRpcRequest final {
    std::uint64_t id{};
    std::string method{};
    std::string paramsJson{"[]"};
};

/** @brief EIP-1193 compatible error returned by an Ethereum wallet provider. */
struct EthereumRpcError final {
    std::int32_t code{4200};
    std::string message{};
    std::string details{};
};

/** @brief JSON value response or structured EIP-1193 error for one request. */
struct EthereumRpcResponse final {
    std::uint64_t id{};
    std::string resultJson{"null"};
    std::optional<EthereumRpcError> error{};
};

/** @brief Ethereum provider events forwarded by the host into the Mini App SDK. */
enum class EthereumWalletEventKind {
    AccountsChanged,
    ChainChanged,
    Connect,
    Disconnect,
    Message,
};

/** @brief Event parameters are encoded as one JSON array, matching EIP-1193 listener arguments. */
struct EthereumWalletEvent final {
    EthereumWalletEventKind kind{EthereumWalletEventKind::Message};
    std::string paramsJson{"[]"};
};

using EthereumRpcCompletion = std::function<void(EthereumRpcResponse)>;
using EthereumWalletEventSink = std::function<void(EthereumWalletEvent)>;

/**
 * @brief Isolated EIP-1193 wallet transport for one currently executing Mini App.
 *
 * Implementations must not expose private keys. `request()` is asynchronous and
 * may invoke its completion on a provider-owned worker thread. `setEventSink()`
 * installs the same threading contract for provider events. Callers therefore
 * marshal both callbacks before touching a GUI or other thread-confined object.
 */
class IEthereumWalletBridge {
public:
    virtual ~IEthereumWalletBridge() = default;

    /** @brief Returns true only when requests can be routed to a real wallet transport. */
    [[nodiscard]] virtual bool available() const noexcept = 0;

    /** @brief Returns CAIP-2 EVM chains this host is willing to route for Mini Apps. */
    [[nodiscard]] virtual std::vector<std::string> supportedChains() const = 0;

    /** @brief Starts a fresh isolated session for one Mini App origin. */
    [[nodiscard]] virtual foundation::result::Status beginSession(
        std::string appName,
        std::string origin) = 0;

    /** @brief Ends the active session and rejects/cancels all outstanding requests. */
    virtual void endSession() noexcept = 0;

    /** @brief Replaces the provider-event callback; an empty sink disables forwarding. */
    virtual void setEventSink(EthereumWalletEventSink sink) = 0;

    /** @brief Routes one EIP-1193 request through the active wallet session. */
    virtual void request(EthereumRpcRequest request, EthereumRpcCompletion completion) = 0;
};

} // namespace farcaster::capability::wallet
