#include "../../ui/app/identity/native_registration.hpp"
#include "../../ui/app/identity/hypersnap_registration_transport.hpp"

#include <QByteArray>
#include <QHostAddress>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest/QtTest>

#include <memory>
#include <optional>
#include <array>

class NativeRegistrationRefreshTests final : public QObject {
    Q_OBJECT

private slots:
    void refreshDuringPendingReply();
    void refreshReentrantlyFromChanged();
    void nativeAccountLookupHttpSemantics();
};

void NativeRegistrationRefreshTests::refreshDuringPendingReply()
{
    // The first response is deliberately held open. Aborting it must not
    // dereference m_reply after finished() synchronously clears that member.
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    constexpr auto body =
        "{\"active\":true,\"minDifficultyBits\":12,"
        "\"firstNativeFid\":\"4294967296\","
        "\"anchorBlockHash\":\"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
        "\"anchorBlockHeight\":43,\"nextBlock\":44,\"anchorWindow\":10}";
    const QByteArray payload{body};
    const QByteArray response =
        QByteArray{"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "}
        + QByteArray::number(payload.size())
        + QByteArray{"\r\nConnection: close\r\n\r\n"} + payload;

    int receivedRequests = 0;
    QObject::connect(&server, &QTcpServer::newConnection, &server,
                     [&server, &receivedRequests, response] {
        while (server.hasPendingConnections()) {
            auto* const socket = server.nextPendingConnection();
            auto buffer = std::make_shared<QByteArray>();
            QObject::connect(socket, &QTcpSocket::readyRead, socket,
                             [socket, buffer, &receivedRequests, response] {
                buffer->append(socket->readAll());
                if (!buffer->contains("\r\n\r\n")) return;
                buffer->clear();
                ++receivedRequests;
                if (receivedRequests >= 2) {
                    socket->write(response);
                    socket->disconnectFromHost();
                }
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket,
                             &QObject::deleteLater);
        }
    });

    NativeRegistrationStatusController controller{
        QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())};
    controller.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(receivedRequests >= 1, 5000);
    QVERIFY(controller.checking());

    // This call crashed on macOS when abort() emitted finished() inline.
    controller.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(receivedRequests >= 2, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.checking(), 5000);
    QVERIFY(controller.active());
    QVERIFY(controller.error().isEmpty());
    QVERIFY(controller.snapshot().has_value());
}

void NativeRegistrationRefreshTests::refreshReentrantlyFromChanged()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    NativeRegistrationStatusController controller{
        QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())};
    bool nested = false;
    QObject::connect(&controller, &NativeRegistrationStatusController::changed,
                     &controller, [&] {
        if (!nested && controller.checking()) {
            nested = true;
            controller.refresh();
        }
    });

    controller.refresh();
    QVERIFY(nested);
    QVERIFY(controller.checking());
}

void NativeRegistrationRefreshTests::nativeAccountLookupHttpSemantics()
{
    using State = HyperSnapRegistrationTransport::LookupState;
    const QByteArray missing = R"json({"error":"Native registration lookup failed","error_detail":"status: NotFound, message: \"native account not found\", details: [], metadata: MetadataMap { headers: {} }"})json";
    const QByteArray invalid = R"json({"error":"Native registration lookup failed","error_detail":"status: InvalidArgument, message: \"specify one native fid or custody address\""})json";
    const std::array cases{
        std::tuple{400, missing, State::NotFound},
        std::tuple{400, invalid, State::Unavailable},
        std::tuple{400, QByteArray{"{}"}, State::Unavailable},
        std::tuple{404, QByteArray{"Not Found"}, State::Unavailable},
        std::tuple{503, missing, State::Unavailable},
    };
    for (const auto& [status, payload, expected] : cases) {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        QObject::connect(&server, &QTcpServer::newConnection, &server,
                         [&server, status, payload] {
            while (server.hasPendingConnections()) {
                auto* const socket = server.nextPendingConnection();
                QObject::connect(socket, &QTcpSocket::readyRead, socket,
                                 [socket, status, payload] {
                    socket->readAll();
                    const auto reason = status == 400 ? "Bad Request"
                                      : status == 404 ? "Not Found"
                                      : "Service Unavailable";
                    const QByteArray response =
                        QByteArray{"HTTP/1.1 "} + QByteArray::number(status)
                        + QByteArray{" "} + reason
                        + QByteArray{"\r\nContent-Type: application/json\r\nContent-Length: "}
                        + QByteArray::number(payload.size())
                        + QByteArray{"\r\nConnection: close\r\n\r\n"}
                        + payload;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
                QObject::connect(socket, &QTcpSocket::disconnected,
                                 socket, &QObject::deleteLater);
            }
        });
        HyperSnapRegistrationTransport transport{
            QUrl{QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())}};
        std::optional<HyperSnapRegistrationTransport::LookupResult> result;
        transport.lookupByCustody(
            QStringLiteral("0x1111111111111111111111111111111111111111"),
            [&](auto value) { result = value; });
        QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), 5000);
        QCOMPARE(static_cast<int>(result->state), static_cast<int>(expected));
    }
}

QTEST_GUILESS_MAIN(NativeRegistrationRefreshTests)
#include "native_registration_refresh_tests.moc"
