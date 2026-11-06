#include "../app/cast_view_controller.hpp"

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
    const QString retryId = QStringLiteral("0x") + QString(40, QLatin1Char('d'));
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
                    if (requestCount == 0) {
                        assert(casts.size() == 2); // Shared viewport batch.
                    } else {
                        assert(!casts.isEmpty() && casts.size() <= 16);
                        const auto metadata = casts.at(0).toObject();
                        if (requestCount == 1) {
                            assert(metadata.value(QStringLiteral("authorFid")).toInteger() == 46);
                            assert(metadata.value(QStringLiteral("text")).toString()
                                == QStringLiteral("Updated metadata for an archived short cast"));
                        } else if (requestCount < 4) {
                            assert(metadata.value(QStringLiteral("castId")).toString() == retryId);
                        } else {
                            assert(!casts.isEmpty() && casts.size() <= 16);
                        }
                    }
                    ++requestCount;
                    QByteArray json = QByteArray{"{\"counts\":{}}"};
                    if (requestCount == 1) {
                        json = QStringLiteral("{\"counts\":{\"%1\":123}}").arg(firstId).toUtf8();
                    } else if (requestCount == 2) {
                        json = QStringLiteral("{\"counts\":{\"%1\":0}}").arg(
                            QStringLiteral("0x") + QString(40, QLatin1Char('c'))).toUtf8();
                    } else if (requestCount == 4) {
                        json = QStringLiteral("{\"counts\":{\"%1\":6}}").arg(retryId).toUtf8();
                    } else if (requestCount >= 5) {
                        QJsonObject counts{};
                        for (const auto& value : casts) {
                            counts.insert(value.toObject().value(QStringLiteral("castId")).toString(), 5);
                        }
                        json = QJsonDocument(QJsonObject{{QStringLiteral("counts"), counts}})
                                   .toJson(QJsonDocument::Compact);
                    }
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
    // Fast scheduler only for deterministic retry fixture; production default
    // remains 90 seconds.
    CastViewController controller{false, true, nullptr, localEndpoint, 125};
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
    // Unsent cancellation has no 90-second failure cooldown. Reacquisition
    // with a more complete author/text descriptor must be admitted normally.
    auto* archived = qobject_cast<CastViewEntry*>(controller.entryFor(obsoleteId));
    assert(archived && !archived->count().isValid());
    controller.retain(obsoleteId);
    controller.ensure(obsoleteId, 45, {});
    controller.ensure(obsoleteId, 46,
        QStringLiteral("Updated metadata for an archived short cast"));
    QEventLoop reacquired{};
    QTimer::singleShot(3000, &reacquired, &QEventLoop::quit);
    QObject::connect(archived, &CastViewEntry::countChanged,
        &reacquired, &QEventLoop::quit);
    reacquired.exec();
    assert(requestCount == 2);
    assert(archived->count().isValid() && archived->count().toULongLong() == 0);
    controller.release(obsoleteId);
    // First legitimate unavailable response must schedule an interested retry
    // without any external ensure() call. Unknown remains distinct from zero.
    auto* retryEntry = qobject_cast<CastViewEntry*>(controller.entryFor(retryId));
    assert(retryEntry && !retryEntry->count().isValid());
    controller.retain(retryId);
    controller.ensure(retryId, 47, QStringLiteral("Retry after unavailable"));
    QEventLoop retries{};
    QTimer::singleShot(4000, &retries, &QEventLoop::quit);
    QObject::connect(retryEntry, &CastViewEntry::countChanged, &retries,
                     &QEventLoop::quit);
    retries.exec();
    assert(requestCount == 4); // Initial unknown, then bounded automatic retry.
    assert(retryEntry->count().isValid() && retryEntry->count().toULongLong() == 6);
    controller.release(retryId);
    // Capacity pressure: a 97th live cast is not abandoned when 96
    // operations occupy the bounded admission queue. Each HTTP batch is
    // capped at 16; queued work resumes after preceding batches finish.
    CastViewController saturation{false, true, nullptr, localEndpoint};
    QStringList retained{};
    CastViewEntry* finalEntry = nullptr;
    for (int i = 0; i < 97; ++i) {
        const auto id = QStringLiteral("0x") + QString::number(i + 1000, 16)
            .rightJustified(40, QLatin1Char('0'));
        retained.append(id);
        auto* entry = qobject_cast<CastViewEntry*>(saturation.entryFor(id));
        assert(entry);
        if (i == 96) {
            finalEntry = entry;
        }
        saturation.retain(id);
        saturation.ensure(id, 90, QStringLiteral("Viewport saturation fixture"));
    }
    QEventLoop saturated{};
    QTimer poll{};
    poll.setInterval(20);
    QObject::connect(&poll, &QTimer::timeout, &saturated, [&] {
        for (const auto& id : retained) {
            const auto* entry = qobject_cast<CastViewEntry*>(saturation.entryFor(id));
            if (!entry || !entry->count().isValid()) {
                return;
            }
        }
        saturated.quit();
    });
    QTimer::singleShot(5000, &saturated, &QEventLoop::quit);
    poll.start();
    saturated.exec();
    poll.stop();
    assert(finalEntry->count().isValid() && finalEntry->count().toULongLong() == 5);
    assert(requestCount >= 11 && requestCount <= 16);
    for (const auto& id : retained) {
        saturation.release(id);
    }
    return 0;
}
