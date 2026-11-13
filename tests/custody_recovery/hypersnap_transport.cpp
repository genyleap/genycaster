#ifdef NDEBUG
#undef NDEBUG
#endif
#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>

#include "../../ui/app/identity/hypersnap_registration_transport.hpp"
#include "../../ui/app/identity/native_registration.hpp"

namespace {
using Transport = HyperSnapRegistrationTransport;
constexpr auto Custody = "0xf278cf59f82edcf871d630f28ecc8056f25c1cdb";

QByteArray response(int code, QByteArray json)
{
    const QByteArray status = code == 200 ? "OK"
        : code == 404 ? "Not Found" : "Bad Request";
    return "HTTP/1.1 " + QByteArray::number(code) + " " + status
        + "\r\nContent-Type: application/json\r\nContent-Length: "
        + QByteArray::number(json.size())
        + "\r\nConnection: close\r\n\r\n" + json;
}

class FakeHub final {
public:
    QTcpServer server;
    QByteArray lastRequest;
    std::function<QByteArray(const QByteArray&)> reply;
    FakeHub()
    {
        assert(server.listen(QHostAddress::LocalHost));
        QObject::connect(&server, &QTcpServer::newConnection, &server,
            [this] {
                while (auto* socket = server.nextPendingConnection()) {
                    struct RequestState { QByteArray bytes; bool responded{false}; };
                    auto state = std::make_shared<RequestState>();
                    QObject::connect(socket, &QTcpSocket::readyRead, socket,
                        [this, socket, state] {
                            if (state->responded) return;
                            state->bytes.append(socket->readAll());
                            const qsizetype headersEnd = state->bytes.indexOf("\r\n\r\n");
                            if (headersEnd < 0) return;
                            qsizetype bodyBytes = 0;
                            const auto headers = state->bytes.left(headersEnd);
                            for (const auto& line : headers.split('\n')) {
                                if (line.toLower().startsWith("content-length:"))
                                    bodyBytes = line.mid(15).trimmed().toLongLong();
                            }
                            if (state->bytes.size() < headersEnd + 4 + bodyBytes) return;
                            state->responded = true;
                            lastRequest = state->bytes;
                            const auto payload = reply(lastRequest);
                            socket->write(payload);
                            socket->flush();
                            socket->disconnectFromHost();
                        });
                    QObject::connect(socket, &QTcpSocket::disconnected,
                                     socket, &QTcpSocket::deleteLater);
                }
            });
    }
    QUrl url() const
    {
        return QUrl{QStringLiteral("http://127.0.0.1:%1")
            .arg(server.serverPort())};
    }
};

template <typename Function>
void await(Function start)
{
    QEventLoop loop{};
    bool completed = false;
    QTimer deadline{};
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    deadline.start(4000);
    start([&] { completed = true; loop.quit(); });
    if (!completed) loop.exec();
    assert(completed && "localhost registration transport timed out");
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    FakeHub mock{};
    Transport client{mock.url()};
    assert(client.endpointIsSafe());
    constexpr auto ExpectedFid = (1ULL << 32);
    const auto address = QString::fromLatin1(Custody);

    mock.reply = [](const QByteArray&) {
        return response(404, "{\"error\":\"not_found\"}");
    };
    await([&](auto done) {
        client.lookupByCustody(address,[&, done](auto result) {
            assert(result.state == Transport::LookupState::NotFound);
            done();
        });
    });
    assert(mock.lastRequest.startsWith(
        "GET /v1/nativeAccount?custody=0xf278cf59f82edcf871d630f28ecc8056f25c1cdb "));

    mock.reply = [](const QByteArray&) {
        return response(200,
            "{\"fid\":\"4294967296\","
            "\"custodyAddress\":\"0xf278cf59f82edcf871d630f28ecc8056f25c1cdb\","
            "\"blockNumber\":54321,\"message\":{\"data\":{}}}");
    };
    await([&](auto done) {
        client.lookupByCustody(address,[&, done](auto result) {
            assert(result.state == Transport::LookupState::Confirmed);
            assert(result.fid == ExpectedFid);
            assert(result.blockNumber == 54321);
            done();
        });
    });
    mock.reply = [](const QByteArray&) {
        return response(200,
            "{\"fid\":\"4294967296\",\"custodyAddress\":"
            "\"0x0000000000000000000000000000000000000001\","
            "\"blockNumber\":54321,\"message\":{}}");
    };
    await([&](auto done) {
        client.lookupByCustody(address,[&, done](auto result) {
            assert(result.state == Transport::LookupState::InvalidResponse);
            done();
        });
    });

    mock.reply = [](const QByteArray&) {
        return response(200,
            "{\"fid\":\"4294967296\","
            "\"custodyAddress\":\"0xf278cf59f82edcf871d630f28ecc8056f25c1cdb\","
            "\"blockNumber\":54321,\"message\":{\"data\":{}}}");
    };
    NativeRegistrationStatusController status{mock.url().toString()};
    {
        QEventLoop loop{};
        bool finished = false;
        QObject::connect(&status, &NativeRegistrationStatusController::lookupChanged,
            &loop, [&] {
                if (status.accountLookupChecked() && !status.accountLookupBusy()) {
                    finished = true;
                    loop.quit();
                }
            });
        status.findExistingNativeAccount(address);
        QTimer::singleShot(4000, &loop, &QEventLoop::quit);
        if (!finished) loop.exec();
        assert(finished);
        assert(status.existingNativeFid() == QStringLiteral("4294967296"));
        assert(!status.accountLookupFailed());
        assert(!status.verifiedAbsentForCustody(address));
        status.clearExistingLookup();
        assert(status.existingNativeFid().isEmpty());
        assert(!status.accountLookupChecked());
        assert(!status.verifiedAbsentForCustody(address));
    }
    mock.reply = [](const QByteArray&) {
        return response(404, "{\"error\":\"not_found\"}");
    };
    {
        QEventLoop loop{};
        bool finished = false;
        QObject::connect(&status, &NativeRegistrationStatusController::lookupChanged,
            &loop, [&] {
                if (status.accountLookupChecked() && !status.accountLookupBusy()) {
                    finished = true;
                    loop.quit();
                }
            });
        status.findExistingNativeAccount(address);
        QTimer::singleShot(4000, &loop, &QEventLoop::quit);
        if (!finished) loop.exec();
        assert(finished);
        assert(status.verifiedAbsentForCustody(address));
        assert(!status.verifiedAbsentForCustody(
            QStringLiteral("0x0000000000000000000000000000000000000001")));
    }

    const QByteArray protobuf{"\x0a\x05\x00\x10\xff", 5};
    mock.reply = [](const QByteArray& bytes) {
        assert(bytes.startsWith("POST /v1/submitMessage "));
        assert(bytes.contains("Content-Type: application/octet-stream"));
        assert(bytes.endsWith(QByteArray{"\x0a\x05\x00\x10\xff", 5}));
        return response(200, "{\"hash\":\"0x1234\"}");
    };
    await([&](auto done) {
        client.submit(protobuf,[&, done](auto result) {
            assert(result.state == Transport::SubmitState::AcceptedPending);
            done();
        });
    });
    mock.reply = [](const QByteArray&) {
        return response(400, "{\"error\":\"invalid_proof\"}");
    };
    await([&](auto done) {
        client.submit(protobuf,[&, done](auto result) {
            assert(result.state == Transport::SubmitState::Rejected);
            done();
        });
    });
    mock.reply = [](const QByteArray&) {
        return response(503, "{\"error\":\"try_again\"}");
    };
    await([&](auto done) {
        client.submit(protobuf,[&, done](auto result) {
            assert(result.state == Transport::SubmitState::UnknownOutcome);
            done();
        });
    });

    Transport unsafe{QUrl{"http://example.com"}};
    assert(!unsafe.endpointIsSafe());
    bool rejected = false;
    unsafe.submit(protobuf,[&](auto result) {
        rejected = result.state == Transport::SubmitState::Rejected;
    });
    assert(rejected);
    std::cout << "HyperSnap native lookup and binary POST transport, no false success passed\n";
}
