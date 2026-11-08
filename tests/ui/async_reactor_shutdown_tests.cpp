// Regression for macOS SIGSEGV on GenycasterNetworkReactor during shutdown.
// A delayed Feed HTTP completion must not run after its owning runtime exits.
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

import farcaster.transport.async_http;
import farcaster.transport.qtnetwork;
import farcaster.transport.qtnetwork.async;

int main(int argc, char** argv)
{
    QCoreApplication app{argc, argv};
    QTcpServer server{};
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        std::cerr << "Unable to create pending local HTTP server\n";
        return EXIT_FAILURE;
    }

    namespace transport = farcaster::transport;
    auto network = transport::qtnetwork::async::makeAsyncTransport(
        transport::qtnetwork::ProxyPolicy::Direct,
        transport::qtnetwork::ProtocolPolicy::Automatic,
        transport::qtnetwork::async::NetworkLimits{
            .maximumActiveRequests = 1U,
            .maximumActiveRequestsPerOrigin = 1U,
            .maximumBackgroundActiveRequests = 1U,
            .maximumBackgroundActiveRequestsPerOrigin = 1U,
            .maximumAcceptedRequests = 4U,
        });
    if (!network) {
        std::cerr << "Unable to construct asynchronous Qt transport\n";
        return EXIT_FAILURE;
    }
    std::atomic_int callbacks{0};
    const auto url = "http://127.0.0.1:" + std::to_string(server.serverPort()) + "/feed";
    for (int index = 0; index < 3; ++index) {
        auto started = (*network)->start(
            transport::async_http::Request{
                .method = transport::async_http::Method::Get,
                .url = url,
                .timeout = std::chrono::seconds{30},
            },
            [&callbacks](auto) { ++callbacks; });
        if (!started) {
            std::cerr << "Asynchronous request admission failed\n";
            return EXIT_FAILURE;
        }
    }

    // One HTTP operation is in-flight, two remain admitted but queued.
    // The application deliberately does not respond to the first request.
    QElapsedTimer watchdog{};
    watchdog.start();
    while (!server.hasPendingConnections() && watchdog.elapsed() < 3'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(5);
    }
    if (!server.hasPendingConnections()) {
        std::cerr << "Pending request did not reach local test server\n";
        return EXIT_FAILURE;
    }

    // Before this fix, ReactorWorker::shutdown invoked arbitrary completion
    // functions while the Runtime's ReadService was being destroyed.
    // Shutdown must abandon those callbacks and join its thread promptly.
    watchdog.restart();
    network->reset();
    if (watchdog.elapsed() > 3'000 || callbacks.load() != 0) {
        std::cerr << "Shutdown dispatched a stale callback or blocked the GUI\n";
        return EXIT_FAILURE;
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    if (callbacks.load() != 0) {
        std::cerr << "A stale callback survived network teardown\n";
        return EXIT_FAILURE;
    }
    std::cout << "Reactor shutdown abandoned all in-flight and queued callbacks\n";
    return EXIT_SUCCESS;
}
