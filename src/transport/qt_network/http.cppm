/*!
 * @file        src/transport/qt_network/http.cppm
 * @brief       Qt Network HTTP transport factory.
 *
 * @details
 * Defines Qt-specific proxy and protocol policy when composing the
 * provider-neutral HTTP interface.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstdint>
#include <memory>

export module farcaster.transport.qtnetwork;

import farcaster.foundation.result;
import farcaster.transport.http;

export namespace farcaster::transport::qtnetwork {

/**
 * @brief Controls whether Qt Network connects directly or follows OS proxy settings.
 */
enum class ProxyPolicy : std::uint8_t {
    Direct,
    System,
};

/**
 * @brief Controls HTTP protocol negotiation for Qt Network requests.
 */
enum class ProtocolPolicy : std::uint8_t {
    Automatic,
    Http1Only,
};

/**
 * @brief Owns application-wide admission and cancellation of blocking Qt HTTP.
 *
 * Construct on the application thread before creating transports; only one scope
 * may be active (overlap throws std::logic_error). Transports retain their scope's
 * stopped state. Destroy before QCoreApplication, after joining request workers.
 * A scope does not own or join executors. Without a scope, standalone transports
 * retain their ordinary per-request deadline behavior.
 */
class RequestShutdownScope final {
public:
    /** @brief Opens request admission for subsequently created transports. */
    RequestShutdownScope();

    /** @brief Stops admission and requests cancellation; does not join workers. */
    ~RequestShutdownScope();

    RequestShutdownScope(const RequestShutdownScope&) = delete;
    auto operator=(const RequestShutdownScope&) -> RequestShutdownScope& = delete;

    /**
     * @brief Idempotently rejects new requests and queues active reply cancellation.
     *
     * Thread-safe and non-blocking with respect to network completion. Cancellation
     * runs on each request thread while Qt event delivery is still available.
     * Requests return Unavailable; an interrupted POST may have reached the server.
     */
    void stop();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @brief Creates a Qt Network-backed blocking HTTP transport for graphical runtimes.
 *
 * Each worker thread reuses one QNetworkAccessManager per proxy policy while
 * each request owns its reply/event-loop state on that same worker thread.
 * Cookie persistence is disabled at the QNetworkAccessManager boundary; callers
 * that require cookies must carry them explicitly through IHttpTransport headers.
 * The application must already have a QCoreApplication instance. TLS certificate
 * verification remains enabled and Qt uses the platform trust facilities. Proxy
 * participation is an explicit composition policy.
 * If a RequestShutdownScope is active, the transport retains that scope and
 * rejects calls after it stops, including calls from previously queued jobs.
 *
 * @param proxyPolicy Whether requests honor the system proxy or connect directly.
 * @param protocolPolicy Whether Qt may negotiate HTTP/2 or must use HTTP/1.x.
 * @return Owned transport, or a structured construction failure.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<http::IHttpTransport>>
makeTransport(
    ProxyPolicy proxyPolicy = ProxyPolicy::System,
    ProtocolPolicy protocolPolicy = ProtocolPolicy::Automatic);

}
