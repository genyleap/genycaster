#include "../app/browser_reaction_controller.hpp"

#include <QCoreApplication>
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
    QCoreApplication app{argc, argv};
    const QString firstId = QStringLiteral("0x") + QString(40, QLatin1Char('a'));
    const QString secondId = QStringLiteral("0x") + QString(40, QLatin1Char('b'));
    const QString thirdId = QStringLiteral("0x") + QString(40, QLatin1Char('c'));

    QTcpServer server{};
    assert(server.listen(QHostAddress::LocalHost, 0));
    int requests{};
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&] {
        while (server.hasPendingConnections()) {
            auto* socket = server.nextPendingConnection();
            auto collected = std::make_shared<QByteArray>();
            QObject::connect(socket, &QTcpSocket::readyRead, socket,
                [&, socket, collected] {
                    collected->append(socket->readAll());
                    const auto headerEnd = collected->indexOf("\r\n\r\n");
                    if (headerEnd < 0) return;
                    int length = -1;
                    for (const auto& line : collected->left(headerEnd).split('\n')) {
                        if (line.trimmed().toLower().startsWith("content-length:")) {
                            length = line.trimmed().mid(15).trimmed().toInt();
                        }
                    }
                    if (length < 0 || collected->size() < headerEnd + 4 + length) return;
                    const auto document = QJsonDocument::fromJson(
                        collected->mid(headerEnd + 4, length));
                    assert(document.isObject());
                    const auto payload = document.object();
                    const auto casts = payload.value(QStringLiteral("casts")).toArray();
                    assert(casts.size() >= 1 && casts.size() <= 8);
                    ++requests;
                    QJsonObject reactions{};
                    if (requests == 1) {
                        assert(payload.value(QStringLiteral("viewerFid")).toInteger() == 42);
                        assert(casts.size() == 2);
                        reactions.insert(firstId, QJsonObject{{QStringLiteral("liked"), true}});
                        // An unrelated hash must not contaminate the target.
                        reactions.insert(thirdId,
                            QJsonObject{{QStringLiteral("recasted"), true}});
                    } else if (requests == 2) {
                        assert(payload.value(QStringLiteral("viewerFid")).toInteger() == 43);
                        assert(casts.size() == 1 && casts.at(0).toString() == firstId);
                    } else if (requests == 3) {
                        assert(payload.value(QStringLiteral("viewerFid")).toInteger() == 42);
                        assert(casts.size() == 1 && casts.at(0).toString() == thirdId);
                        reactions.insert(thirdId,
                            QJsonObject{{QStringLiteral("recasted"), true}});
                    } else {
                        assert(false && "Unexpected unbounded reaction request");
                    }
                    const auto json = QJsonDocument{
                        QJsonObject{{QStringLiteral("reactions"), reactions}}}
                        .toJson(QJsonDocument::Compact);
                    const auto response = QByteArray{
                        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                        "Connection: close\r\nContent-Length: "}
                        + QByteArray::number(json.size()) + "\r\n\r\n" + json;
                    socket->write(response);
                    socket->flush();
                    socket->disconnectFromHost();
                });
        }
    });

    const QUrl endpoint{QStringLiteral("http://127.0.0.1:%1/reactions")
        .arg(server.serverPort())};
    BrowserReactionController controller{false, true, nullptr, endpoint, 50};
    assert(!controller.reacted(firstId, 42, QStringLiteral("like")));
    assert(!controller.localValue(firstId, 42, QStringLiteral("like")).isValid());
    controller.retain(firstId.toUpper(), 42);
    controller.retain(firstId, 42); // Two consumers, one upstream target.
    controller.retain(secondId, 42);
    controller.ensure(firstId, 42);
    controller.ensure(secondId, 42);
    QEventLoop wait{};
    QTimer deadline{};
    deadline.setSingleShot(true);
    deadline.setInterval(1500);
    QObject::connect(&deadline, &QTimer::timeout, &wait, &QEventLoop::quit);
    QObject::connect(&controller, &BrowserReactionController::revisionChanged,
        &wait, &QEventLoop::quit);
    deadline.start();
    wait.exec();
    assert(requests == 1);
    assert(controller.reacted(firstId, 42, QStringLiteral("like")));
    assert(!controller.reacted(secondId, 42, QStringLiteral("like")));
    assert(!controller.reacted(thirdId, 42, QStringLiteral("recast")));

    controller.setLocal(firstId, 42, QStringLiteral("like"), false);
    assert(controller.localValue(firstId, 42, QStringLiteral("like")).isValid());
    assert(!controller.reacted(firstId, 42, QStringLiteral("like")));
    controller.setLocal(firstId, 42, QStringLiteral("like"), true);
    assert(controller.reacted(firstId, 42, QStringLiteral("like")));
    controller.release(firstId, 42);
    controller.release(firstId, 42);
    controller.release(secondId, 42);

    // Identity isolation: viewer 43 cannot inherit viewer 42's verified like.
    controller.retain(firstId, 43);
    controller.ensure(firstId, 43);
    QEventLoop otherViewer{};
    QTimer::singleShot(300, &otherViewer, &QEventLoop::quit);
    otherViewer.exec();
    assert(requests == 2);
    assert(!controller.reacted(firstId, 43, QStringLiteral("like")));
    controller.release(firstId, 43);

    // An unsent pooled delegate can be reacquired without cooldown.
    controller.retain(thirdId, 42);
    controller.ensure(thirdId, 42);
    controller.release(thirdId, 42);
    QEventLoop noDispatch{};
    QTimer::singleShot(130, &noDispatch, &QEventLoop::quit);
    noDispatch.exec();
    assert(requests == 2);
    controller.retain(thirdId, 42);
    controller.ensure(thirdId, 42);
    QEventLoop reacquired{};
    QTimer::singleShot(1000, &reacquired, &QEventLoop::quit);
    QObject::connect(&controller, &BrowserReactionController::revisionChanged,
        &reacquired, &QEventLoop::quit);
    reacquired.exec();
    assert(requests == 3);
    assert(controller.reacted(thirdId, 42, QStringLiteral("recast")));
    controller.release(thirdId, 42);

    assert(!controller.localValue(firstId, 43, QStringLiteral("like")).isValid());
    assert(!controller.reacted(firstId, 0, QStringLiteral("like")));
    assert(!controller.reacted(QStringLiteral("invalid"), 42, QStringLiteral("like")));

    // A maliciously large JSON response cannot consume unbounded UI memory
    // or become positive evidence, even when it advertises a JSON mime type.
    QTcpServer oversizeServer{};
    assert(oversizeServer.listen(QHostAddress::LocalHost, 0));
    QObject::connect(&oversizeServer, &QTcpServer::newConnection, &app, [&] {
        while (oversizeServer.hasPendingConnections()) {
            auto* socket = oversizeServer.nextPendingConnection();
            const QByteArray tooLarge{70 * 1024, 'X'};
            const QByteArray header{
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                "Connection: close\r\nContent-Length: "};
            socket->write(header + QByteArray::number(tooLarge.size())
                + "\r\n\r\n" + tooLarge);
            socket->disconnectFromHost();
        }
    });
    const QUrl largeEndpoint{QStringLiteral("http://127.0.0.1:%1/large")
        .arg(oversizeServer.serverPort())};
    BrowserReactionController oversized{false, true, nullptr, largeEndpoint, 0};
    oversized.retain(firstId, 91);
    oversized.ensure(firstId, 91);
    QEventLoop drainOversize{};
    QTimer::singleShot(250, &drainOversize, &QEventLoop::quit);
    drainOversize.exec();
    assert(!oversized.reacted(firstId, 91, QStringLiteral("like")));
    assert(oversized.revision() == 0);
    oversized.release(firstId, 91);
    return 0;
}
