#include "../mobile/cast_view_controller.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <cassert>
#include <memory>

int main(int argc, char** argv)
{
    QCoreApplication application{argc, argv};
    const QString firstId = QStringLiteral("0x") + QString(40, QLatin1Char('a'));
    const QString secondId = QStringLiteral("0x") + QString(40, QLatin1Char('b'));
    QTcpServer server{};
    assert(server.listen(QHostAddress::LocalHost, 0));
    int requestCount = 0;
    QObject::connect(&server, &QTcpServer::newConnection, &application, [&] {
        while (server.hasPendingConnections()) {
            QTcpSocket* socket = server.nextPendingConnection();
            auto collected = std::make_shared<QByteArray>();
            QObject::connect(socket, &QTcpSocket::readyRead, socket,
                [&, collected, socket] {
                    collected->append(socket->readAll());
                    const auto headerEnd = collected->indexOf("\r\n\r\n");
                    if (headerEnd < 0) {
                        return;
                    }
                    int length = -1;
                    for (const QByteArray& line : collected->left(headerEnd).split('\n')) {
                        const QByteArray trimmed = line.trimmed();
                        if (trimmed.toLower().startsWith("content-length:")) {
                            length = trimmed.mid(15).trimmed().toInt();
                        }
                    }
                    if (length < 0 || collected->size() < headerEnd + 4 + length) {
                        return;
                    }
                    const auto requestBody = collected->mid(headerEnd + 4, length);
                    const auto parsed = QJsonDocument::fromJson(requestBody);
                    assert(parsed.isObject());
                    const auto casts = parsed.object().value(QStringLiteral("casts")).toArray();
                    assert(casts.size() == 2); // One deduplicated shared viewport batch.
                    ++requestCount;
                    const QByteArray json = QStringLiteral(
                        "{\"counts\":{\"%1\":123}}").arg(firstId).toUtf8();
                    const QByteArray headers = QByteArray{
                        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                        "Connection: close\r\nContent-Length: "}
                        + QByteArray::number(json.size()) + "\r\n\r\n";
                    socket->write(headers + json);
                    socket->flush();
                    socket->disconnectFromHost();
                });
        }
    });
    const QUrl localEndpoint{QStringLiteral("http://127.0.0.1:%1/views")
                                .arg(server.serverPort())};
    CastViewController controller{false, true, nullptr, localEndpoint};
    assert(controller.entryFor(QStringLiteral("invalid-hash")) == nullptr);
    auto* first = qobject_cast<CastViewEntry*>(controller.entryFor(firstId));
    auto* second = qobject_cast<CastViewEntry*>(controller.entryFor(secondId));
    assert(first && second);
    controller.retain(firstId);
    controller.retain(firstId); // Two independent interested consumers.
    controller.retain(secondId);
    controller.ensure(firstId, 42, QStringLiteral("First Cast"));
    controller.ensure(firstId, 42, QStringLiteral("First Cast")); // Deduplicated.
    controller.ensure(secondId, 43, QStringLiteral("Second Cast"));
    QEventLoop loop{};
    QTimer watchdog{};
    watchdog.setSingleShot(true);
    watchdog.setInterval(4000);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(first, &CastViewEntry::countChanged, &loop, &QEventLoop::quit);
    watchdog.start();
    loop.exec();
    assert(first->count().isValid() && first->count().toULongLong() == 123);
    assert(!second->count().isValid()); // Unavailable is not an invented zero.
    assert(requestCount == 1);
    controller.release(firstId);
    controller.release(firstId);
    controller.release(secondId);
    const QString obsoleteId = QStringLiteral("0x") + QString(40, QLatin1Char('c'));
    controller.retain(obsoleteId);
    controller.ensure(obsoleteId, 44, QStringLiteral("Obsolete offscreen Cast"));
    controller.release(obsoleteId); // Cancel before queued dispatch.
    controller.ensure(firstId, 42, QStringLiteral("First Cast"));
    QEventLoop quiet{};
    QTimer::singleShot(300, &quiet, &QEventLoop::quit);
    quiet.exec();
    assert(requestCount == 1);
    return 0;
}
