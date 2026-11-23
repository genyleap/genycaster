#include "../../ui/app/browser/link_metadata.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>
#include <cassert>
#include <memory>

int main(int argc, char** argv)
{
    QCoreApplication application{argc, argv};
    QTcpServer server{};
    assert(server.listen(QHostAddress::LocalHost, 0));
    int requests{0};
    int activeUpstream{0};
    int peakUpstream{0};
    QObject::connect(&server, &QTcpServer::newConnection, &application, [&] {
        while (server.hasPendingConnections()) {
            QTcpSocket* socket = server.nextPendingConnection();
            auto collected = std::make_shared<QByteArray>();
            QObject::connect(socket, &QTcpSocket::readyRead, socket,
                [&, socket, collected] {
                    collected->append(socket->readAll());
                    if (!collected->contains("\r\n\r\n")) return;
                    const auto path = collected->split(' ').at(1);
                    const QUrl url{QStringLiteral("http://localhost") + QString::fromLatin1(path)};
                    const QString page = QUrlQuery{url}.queryItemValue(QStringLiteral("url"));
                    ++requests;
                    ++activeUpstream;
                    peakUpstream = std::max(peakUpstream, activeUpstream);
                    QByteArray body{};
                    if (url.path() == QStringLiteral("/v1/browser/miniapp-preview")) {
                        // Preserve the full cast URL for per-post Mini App metadata.
                        assert(page == QStringLiteral("https://example.com/cast"));
                        // First response attempts to redirect the domain; this
                        // must not be accepted as a verified Mini App preview.
                        body = R"({"resolved":true,"providerKind":"miniapp","domain":"evil.example","title":"Mismatched"})";
                    } else {
                        assert(url.path() == QStringLiteral("/v1/browser/link-preview"));
                        if (page == QStringLiteral("https://example.com/cast")) {
                            body = R"({"resolved":true,"providerKind":"web","domain":"example.com","title":"Verified"})";
                        } else if (page == QStringLiteral("https://bad.example/")) {
                            body = R"({"resolved":true,"providerKind":"web","domain":"unrelated.example","title":"Wrong"})";
                        } else {
                            // The body exceeds the 96-KiB reply budget.
                            body = QByteArray(110 * 1024, 'x');
                        }
                    }
                    const QByteArray response = QByteArray{
                        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                        "Connection: close\r\nContent-Length: "}
                        + QByteArray::number(body.size()) + "\r\n\r\n" + body;
                    QTimer::singleShot(30, socket,
                        [socket, response, &activeUpstream] {
                            --activeUpstream;
                            socket->write(response);
                            socket->flush();
                            socket->disconnectFromHost();
                        });
                });
        }
    });
    const QUrl relay{QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())};
    BrowserLinkMetadataController controller{false, true, nullptr, relay};
    QVariantMap link{}, miniapp{}, wrong{}, oversize{};
    int completions{0};
    QObject::connect(&controller, &BrowserLinkMetadataController::previewReady,
        &application, [&](const QString& page, const QString& kind, const QVariantMap& data) {
            ++completions;
            if (page == QStringLiteral("https://example.com/cast") && kind == QStringLiteral("link"))
                link = data;
            if (page == QStringLiteral("https://example.com/cast") && kind == QStringLiteral("miniapp"))
                miniapp = data;
            if (page == QStringLiteral("https://bad.example/"))
                wrong = data;
            if (page == QStringLiteral("https://large.example/"))
                oversize = data;
        });
    controller.requestPreview(QStringLiteral("https://example.com/cast"), QStringLiteral("link"));
    controller.requestPreview(QStringLiteral("https://example.com/cast"), QStringLiteral("link"));
    controller.requestPreview(QStringLiteral("https://example.com/cast"), QStringLiteral("miniapp"));
    controller.requestPreview(QStringLiteral("https://bad.example/"), QStringLiteral("link"));
    controller.requestPreview(QStringLiteral("https://large.example/"), QStringLiteral("link"));
    controller.requestPreview(QStringLiteral("http://example.com/not-https"), QStringLiteral("link"));
    controller.requestPreview(QStringLiteral("https://localhost/private"), QStringLiteral("link"));
    controller.requestPreview(QStringLiteral("https://example.com/cast"), QStringLiteral("other"));
    // Image-only cast embeds must never request an HTML/Mini App preview.
    controller.requestPreview(QStringLiteral("https://quilcast.com/m/a8b2f916360fe7de52fe7fe8.jpg"), QStringLiteral("link"));
    controller.requestPreview(QStringLiteral("https://quilcast.com/m/a8b2f916360fe7de52fe7fe8.jpg"), QStringLiteral("miniapp"));
    controller.requestPreview(QStringLiteral("https://gateway.pinata.cloud/ipfs/bafybeicdae7bmjnkp3aiwdemgduv6bwfmqf4xnbhfsjiz73k5fg375yneq/eye512.webp"), QStringLiteral("link"));
    controller.requestPreview(QStringLiteral("https://imagedelivery.net/BXIuQx4ige9GuW0la56BHw/350c100b-e026-42cd-3b41-1f2657ebe300/original"), QStringLiteral("miniapp"));
    controller.requestPreview(QStringLiteral("https://imagedelivery.net/BXIuQx4ige9GuW0la56BHw/350c100b-e026-42cd-3b41-1f2657ebe300/original"), QStringLiteral("link"));

    QEventLoop loop{};
    QTimer watcher{};
    watcher.setInterval(20);
    QObject::connect(&watcher, &QTimer::timeout, &loop, [&] {
        if (completions >= 4) loop.quit();
    });
    QTimer::singleShot(2200, &loop, &QEventLoop::quit);
    watcher.start();
    loop.exec();
    watcher.stop();
    assert(requests == 4 && completions == 4); // Duplicates shared.
    assert(peakUpstream <= 2); // Global admission, shared across types.
    assert(link.value(QStringLiteral("title")).toString() == QStringLiteral("Verified"));
    assert(miniapp.isEmpty()); // Mismatched source domains are rejected.
    assert(wrong.isEmpty());
    assert(oversize.isEmpty());
    assert(controller.previewFor(QStringLiteral("https://example.com/cast"),
        QStringLiteral("link")).value(QStringLiteral("title")).toString()
        == QStringLiteral("Verified"));

    // A second consumer can reuse verified evidence without network traffic.
    controller.requestPreview(QStringLiteral("https://example.com/cast"), QStringLiteral("link"));
    QEventLoop cached{};
    QTimer::singleShot(100, &cached, &QEventLoop::quit);
    cached.exec();
    assert(requests == 4 && completions == 5);

    // The bounded queue reports evictions as explicit empty completions.
    // Offscreen metadata can be reacquired without retaining a failed value.
    BrowserLinkMetadataController saturated{false, true, nullptr, relay};
    int rejected{0};
    QObject::connect(&saturated, &BrowserLinkMetadataController::previewReady,
        &application, [&](const QString&, const QString&, const QVariantMap& preview) {
            if (preview.isEmpty()) ++rejected;
        });
    for (int index = 0; index < 38; ++index) {
        saturated.requestPreview(QStringLiteral("https://example.com/item-%1").arg(index),
            QStringLiteral("link"));
    }
    QEventLoop backpressure{};
    QTimer::singleShot(125, &backpressure, &QEventLoop::quit);
    backpressure.exec();
    assert(rejected >= 4); // 2 active + 32 queued, then explicit overflow.
    return 0;
}
