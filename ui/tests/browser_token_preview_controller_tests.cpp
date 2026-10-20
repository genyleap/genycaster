#include "../mobile/browser_token_preview_controller.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>

#include <cassert>
#include <memory>

int main(int argc, char** argv)
{
    QCoreApplication app{argc, argv};
    QTcpServer server{};
    assert(server.listen(QHostAddress::LocalHost, 0));
    const QString valid = QStringLiteral("base:0x") + QString(40, QLatin1Char('a'));
    const QString wrong = QStringLiteral("eth:0x") + QString(40, QLatin1Char('b'));
    const QString oversize = QStringLiteral("base:0x") + QString(40, QLatin1Char('c'));
    int requests{0};
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&] {
        while (server.hasPendingConnections()) {
            auto* socket = server.nextPendingConnection();
            auto collected = std::make_shared<QByteArray>();
            QObject::connect(socket, &QTcpSocket::readyRead, socket,
                [&, socket, collected] {
                    collected->append(socket->readAll());
                    if (!collected->contains("\r\n\r\n")) return;
                    const QString path = QString::fromLatin1(collected->split(' ').at(1));
                    const QUrl url{QStringLiteral("http://localhost") + path};
                    const QUrlQuery query{url};
                    assert(url.path() == QStringLiteral("/v1/browser/tokens"));
                    const QString identity = query.queryItemValue(QStringLiteral("network"))
                        + QLatin1Char(':') + query.queryItemValue(QStringLiteral("address"));
                    ++requests;
                    QByteArray json{};
                    if (identity == valid) {
                        json = QJsonDocument{QJsonObject{
                            {QStringLiteral("available"), true},
                            {QStringLiteral("network"), QStringLiteral("base")},
                            {QStringLiteral("address"), valid.mid(5)},
                            {QStringLiteral("priceUsd"), 2.5}}}.toJson(QJsonDocument::Compact);
                    } else if (identity == wrong) {
                        json = QJsonDocument{QJsonObject{
                            {QStringLiteral("available"), true},
                            {QStringLiteral("network"), QStringLiteral("eth")},
                            {QStringLiteral("address"), valid.mid(5)}}}.toJson(QJsonDocument::Compact);
                    } else {
                        json = QByteArray(40 * 1024, 'x'); // Rejected oversize body.
                    }
                    const QByteArray response{
                        QByteArray{"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                            "Connection: close\r\nContent-Length: "}
                        + QByteArray::number(json.size()) + "\r\n\r\n" + json};
                    QTimer::singleShot(15, socket, [socket, response] {
                        socket->write(response);
                        socket->flush();
                        socket->disconnectFromHost();
                    });
                });
        }
    });
    const QUrl relay{QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())};
    BrowserTokenPreviewController controller{false, true, nullptr, relay};
    QVariantMap positive{}, mismatched{}, oversized{};
    int delivered{0};
    QObject::connect(&controller, &BrowserTokenPreviewController::tokenReady, &app,
        [&](const QString& key, const QVariantMap& snapshot) {
            ++delivered;
            if (key == valid) positive = snapshot;
            if (key == wrong) mismatched = snapshot;
            if (key == oversize) oversized = snapshot;
        });
    controller.requestToken(valid);
    controller.requestToken(valid.toUpper()); // Dedup canonical hash, network.
    controller.requestToken(wrong);
    controller.requestToken(oversize);
    controller.requestToken(QStringLiteral("base:not-a-token"));
    QEventLoop loop{};
    QTimer poll{};
    poll.setInterval(15);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (delivered == 3) loop.quit();
    });
    QTimer::singleShot(1500, &loop, &QEventLoop::quit);
    poll.start();
    loop.exec();
    poll.stop();
    assert(requests == 3 && delivered == 3);
    assert(positive.value(QStringLiteral("available")).toBool());
    assert(positive.value(QStringLiteral("priceUsd")).toDouble() == 2.5);
    assert(mismatched.isEmpty() && oversized.isEmpty());
    assert(controller.tokenFor(valid).value(QStringLiteral("priceUsd")).toDouble() == 2.5);

    // A second delegate reuses the verified cached snapshot and no HTTP.
    controller.requestToken(valid);
    QEventLoop cache{};
    QTimer::singleShot(60, &cache, &QEventLoop::quit);
    cache.exec();
    assert(requests == 3 && delivered == 4);
    // Failure cooldown is terminal to each consumer, not indefinite loading.
    controller.requestToken(wrong);
    QEventLoop denied{};
    QTimer::singleShot(60, &denied, &QEventLoop::quit);
    denied.exec();
    assert(requests == 3 && delivered == 5);
    assert(mismatched.isEmpty());

    // Saturation is a recoverable, explicit terminal outcome for every
    // queued cast, instead of a silently abandoned visible token.
    BrowserTokenPreviewController saturated{false, true, nullptr, relay};
    int unavailable{0};
    QObject::connect(&saturated, &BrowserTokenPreviewController::tokenReady,
        &app, [&](const QString&, const QVariantMap& data) {
            if (data.isEmpty()) ++unavailable;
        });
    for (int i = 0; i < 22; ++i) {
        const QString locator = QStringLiteral("base:0x")
            + QString::number(i + 1000, 16).rightJustified(40, QLatin1Char('0'));
        saturated.requestToken(locator);
    }
    QEventLoop saturatedRun{};
    QTimer::singleShot(70, &saturatedRun, &QEventLoop::quit);
    saturatedRun.exec();
    assert(unavailable >= 5); // One active, sixteen queued, five evicted.
    return 0;
}
