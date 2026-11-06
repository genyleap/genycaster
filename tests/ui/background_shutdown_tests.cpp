#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThreadPool>
#include <QTimer>
#include <QWindow>
#include <QtConcurrent/QtConcurrentRun>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <utility>

import farcaster.foundation.error;
import farcaster.transport.http;
import farcaster.transport.qtnetwork;
import genycaster.ui.shutdown;

namespace {

namespace http = farcaster::transport::http;
namespace network = farcaster::transport::qtnetwork;
using farcaster::foundation::error::ErrorCode;

class ShutdownTestApplication final : public QGuiApplication {
public:
    ShutdownTestApplication(int& argc, char** argv) : QGuiApplication(argc, argv) {}

    std::function<void()> beforeDispatch{};

    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::MetaCall && beforeDispatch) {
            auto callback = std::move(beforeDispatch);
            beforeDispatch = {};
            callback();
        }
        return QGuiApplication::event(event);
    }
};

int runDispatchScenario(ShutdownTestApplication& application, const bool stopBeforeDispatch)
{
    genycaster::ui::shutdown::ShutdownScope shutdown{};
    int operations{};
    std::atomic_bool result{};
    std::atomic_bool completed{};
    if (stopBeforeDispatch) {
        // Intercept the queued GUI callback before dispatch. stop() must wake
        // its waiting worker even though the GUI cannot deliver that callback.
        application.beforeDispatch = [&] {
            shutdown.stop();
            application.quit();
        };
    }
    (void)QtConcurrent::run([&] {
        result = genycaster::ui::shutdown::invokeOnApplication([&operations] {
            ++operations;
            return true;
        });
        completed = true;
        QMetaObject::invokeMethod(&application, &QCoreApplication::quit, Qt::QueuedConnection);
    });
    application.exec();
    shutdown.stop();
    QCoreApplication::processEvents();
    const bool passed = completed && result == !stopBeforeDispatch
        && operations == (stopBeforeDispatch ? 0 : 1);
    std::println("GUI dispatch: stopped={}, completed={}, executed={}",
        stopBeforeDispatch, completed.load(), operations);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

int runScenario(QGuiApplication& application, const std::string_view mode)
{
    QTcpServer server{};
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        return EXIT_FAILURE;
    }
    const std::string url = "http://127.0.0.1:" + std::to_string(server.serverPort()) + "/pending";
    const bool normal = mode == "success";
    const bool timeout = mode == "timeout";
    const bool earlyReturn = mode == "early-return";
    const bool closeWindow = mode == "close-window";
    const int jobCount = normal || timeout ? 1 : 6;
    const int activeCount = normal || timeout ? 1 : 2;
    QThreadPool::globalInstance()->setMaxThreadCount(activeCount);
    QWindow window{};
    if (closeWindow) {
        window.show();
    }
    QEventLoop startupLoop{};
    int acceptedRequests{};
    bool watchdogExpired{};
    std::atomic_int completed{};
    std::atomic_int failures{};
    std::atomic_int followupsRejected{};
    QElapsedTimer shutdownTimer{};
    QTimer watchdog{};
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &application, [&] {
        watchdogExpired = true;
        startupLoop.quit();
        application.quit();
    });
    watchdog.start(3'000);

    QObject::connect(&server, &QTcpServer::newConnection, &application, [&] {
        while (server.hasPendingConnections()) {
            auto* const socket = server.nextPendingConnection();
            auto received = std::make_shared<QByteArray>();
            auto counted = std::make_shared<bool>(false);
            QObject::connect(socket, &QTcpSocket::readyRead, &application,
                [&, socket, received, counted] {
                    received->append(socket->readAll());
                    if (*counted || !received->contains("\r\n\r\n")) {
                        return;
                    }
                    *counted = true;
                    ++acceptedRequests;
                    if (normal) {
                        socket->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
                        socket->disconnectFromHost();
                    } else if (!timeout && acceptedRequests == activeCount) {
                        // Quit only after both workers reached the server. The
                        // other four jobs remain queued behind these requests.
                        shutdownTimer.start();
                        if (earlyReturn) {
                            startupLoop.quit();
                        } else if (closeWindow) {
                            window.close();
                        } else {
                            application.quit();
                        }
                    }
                });
        }
    });

    {
        genycaster::ui::shutdown::ShutdownScope shutdown{};
        for (int index{}; index < jobCount; ++index) {
            (void)QtConcurrent::run([&, index] {
                auto transport = network::makeTransport(network::ProxyPolicy::Direct);
                if (!transport) {
                    ++failures;
                    ++completed;
                    return;
                }
                const auto deadline = std::chrono::milliseconds{timeout ? 100 : 30'000};
                auto result = index % 2 == 0
                    ? (*transport)->get(http::HttpRequest{.url = url, .timeout = deadline})
                    : (*transport)->post(http::HttpPostRequest{
                        .url = url, .headers = {"Content-Type: text/plain"},
                        .body = "payload", .timeout = deadline});
                if (normal) {
                    if (!result || result->statusCode != 200 || result->body != "OK") {
                        ++failures;
                    }
                } else if (result || result.error().code != (timeout ? ErrorCode::Timeout : ErrorCode::Unavailable)) {
                    ++failures;
                }
                if (!normal && !timeout) {
                    // Models the market adapter continuing to the next asset,
                    // and a new transport made by a previously queued job.
                    auto next = network::makeTransport(network::ProxyPolicy::Direct);
                    if (next) {
                        const auto followup = (*next)->get(http::HttpRequest{.url = url});
                        if (!followup && followup.error().code == ErrorCode::Unavailable) {
                            ++followupsRejected;
                        }
                    }
                }
                ++completed;
                if (normal || timeout) {
                    QMetaObject::invokeMethod(&application, &QCoreApplication::quit, Qt::QueuedConnection);
                }
            });
        }
        if (earlyReturn) {
            startupLoop.exec();
            // No aboutToQuit signal: destruction must still stop active work.
        } else {
            application.exec();
            shutdown.stop(); // Repeated stop must remain safe.
        }
    }
    const auto shutdownMs = shutdownTimer.isValid() ? shutdownTimer.elapsed() : 0;
    const bool passed = !watchdogExpired && failures == 0 && completed == jobCount
        && acceptedRequests == activeCount && shutdownMs < 1'500
        && (normal || timeout || followupsRejected == jobCount)
        && QThreadPool::globalInstance()->activeThreadCount() == 0;
    std::println("{}: completed={}, failures={}, requests={}, rejected followups={}, shutdown={} ms",
        mode, completed.load(), failures.load(), acceptedRequests,
        followupsRejected.load(), shutdownMs);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

}

int main(int argc, char** argv)
{
    ShutdownTestApplication application{argc, argv};
    const std::string_view mode = argc > 1 ? argv[1] : "quit";
    if (mode == "gui-stop" || mode == "gui-success") {
        return runDispatchScenario(application, mode == "gui-stop");
    }
    if (mode == "scope-reuse") {
        if (runScenario(application, "success") != EXIT_SUCCESS) {
            return EXIT_FAILURE;
        }
        return runScenario(application, "quit");
    }
    return runScenario(application, mode);
}
