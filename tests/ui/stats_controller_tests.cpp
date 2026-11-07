#include "../../ui/app/social/stats.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <memory>

namespace {

void check(bool condition, const char* message) {
    if (!condition) qFatal("%s", message);
}

QJsonObject cast(const int daysOld, const bool reply, const int likes) {
    const QJsonObject reactions{
        {QStringLiteral("likes_count"), likes},
        {QStringLiteral("recasts_count"), likes / 3}
    };
    const QJsonObject replies{{QStringLiteral("count"), likes / 3}};
    QJsonObject cast{
        {QStringLiteral("hash"), QString(40, reply ? QLatin1Char('b') :
                                      (daysOld == 12 ? QLatin1Char('c') : QLatin1Char('a')))},
        {QStringLiteral("author"), QJsonObject{{QStringLiteral("fid"), 42}}},
        {QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc()
            .addDays(-daysOld).toString(Qt::ISODateWithMs)},
        {QStringLiteral("type"), reply ? QStringLiteral("reply") : QStringLiteral("cast")},
        {QStringLiteral("text"), QStringLiteral("test cast")},
        {QStringLiteral("reactions"), reactions},
        {QStringLiteral("replies"), replies}
    };
    if (reply) cast.insert(QStringLiteral("parent_hash"), QString(40, QLatin1Char('f')));
    return cast;
}

void waitFor(StatsController& controller) {
    QEventLoop loop{};
    bool finished = false;
    QObject::connect(&controller, &StatsController::changed, &loop, [&] {
        if (controller.state() != QLatin1String("loading")) {
            finished = true;
            loop.quit();
        }
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    check(finished, "Stats request must finish within local fixture deadline");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app{argc, argv};
    QTcpServer server{};
    check(server.listen(QHostAddress::LocalHost), "Local fixture must listen");
    int requestCount = 0;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&] {
        while (server.hasPendingConnections()) {
            auto* socket = server.nextPendingConnection();
            auto input = std::make_shared<QByteArray>();
            QObject::connect(socket, &QTcpSocket::readyRead, socket,
                [&, socket, input] {
                    input->append(socket->readAll());
                    const int header = input->indexOf("\r\n\r\n");
                    if (header < 0) return;
                    const QByteArray line = input->left(input->indexOf("\r\n"));
                    const auto words = line.split(' ');
                    check(words.size() >= 2, "HTTP request line");
                    const QUrl url{QStringLiteral("http://127.0.0.1")
                        + QString::fromUtf8(words.at(1))};
                    const QUrlQuery query{url};
                    ++requestCount;
                    QJsonObject output{};
                    if (url.path() == QLatin1String("/v2/farcaster/user")) {
                        const int fid = query.queryItemValue(QStringLiteral("fid")).toInt();
                        output = QJsonObject{{QStringLiteral("user"),
                            QJsonObject{{QStringLiteral("fid"), fid},
                                        {QStringLiteral("follower_count"), 18},
                                        {QStringLiteral("following_count"), 9}}}};
                    } else if (url.path() == QLatin1String("/v2/farcaster/feed/user/casts")) {
                        const int fid = query.queryItemValue(QStringLiteral("fid")).toInt();
                        output = QJsonObject{
                            {QStringLiteral("casts"), fid == 42
                                ? QJsonArray{cast(2, false, 6), cast(5, true, 20),
                                             cast(12, false, 9), cast(40, false, 40)}
                                : QJsonArray{}},
                            {QStringLiteral("next"), QJsonObject{}}
                        };
                    } else if (url.path() == QLatin1String("/v1/castsByFid")) {
                        const QJsonObject raw{
                            {QStringLiteral("hash"), QString(40, QLatin1Char('e'))},
                            {QStringLiteral("data"), QJsonObject{
                                {QStringLiteral("fid"), 42},
                                {QStringLiteral("timestamp"),
                                    (QDateTime::currentDateTimeUtc().toSecsSinceEpoch()
                                      - 1609459200LL - 3600)},
                                {QStringLiteral("castAddBody"), QJsonObject{{QStringLiteral("text"), "raw"}}}
                            }}
                        };
                        output = QJsonObject{{QStringLiteral("messages"), QJsonArray{raw}}};
                    } else {
                        qFatal("Unexpected URL in local Stats fixture");
                    }
                    const QByteArray content = QJsonDocument(output).toJson(QJsonDocument::Compact);
                    const QByteArray response =
                        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                        "Connection: close\r\nContent-Length: "
                        + QByteArray::number(content.size()) + "\r\n\r\n" + content;
                    socket->write(response);
                    socket->flush();
                    socket->disconnectFromHost();
                });
        }
    });

    StatsController stats{false, true};
    const QString origin = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    stats.load(QStringLiteral("42"), QStringLiteral("hypersnap-v2"), origin, 7);
    waitFor(stats);
    auto summary = stats.summary();
    check(stats.state() == QLatin1String("ready"), "7-day window complete");
    check(summary.value(QStringLiteral("casts")).toInt() == 1, "7-day post count");
    check(summary.value(QStringLiteral("replies")).toInt() == 1, "7-day reply count");
    check(summary.value(QStringLiteral("engagement")).toLongLong() == 10,
          "Only published posts count toward received engagement");
    check(summary.value(QStringLiteral("followers")).toLongLong() == 18,
          "Current followers from the matching FID");
    check(stats.topCasts().size() == 1, "7-day top cast is scoped");

    stats.load(QStringLiteral("42"), QStringLiteral("hypersnap-v2"), origin, 30);
    waitFor(stats);
    summary = stats.summary();
    check(summary.value(QStringLiteral("casts")).toInt() == 2, "30-day cast count");
    check(summary.value(QStringLiteral("replies")).toInt() == 1, "30-day reply count");
    check(summary.value(QStringLiteral("engagement")).toLongLong() == 25,
          "30-day published cast engagement");
    check(stats.topCasts().size() == 2, "Top casts are period scoped");

    stats.load(QStringLiteral("43"), QStringLiteral("hypersnap-v2"), origin, 30);
    waitFor(stats);
    check(stats.summary().value(QStringLiteral("casts")).toInt() == 0,
          "Switching FID must not reuse previous account stats");
    check(stats.topCasts().isEmpty(), "Top posts must be FID scoped");

    stats.load(QStringLiteral("42"), QStringLiteral("snapchain-v1"), origin, 30);
    waitFor(stats);
    check(stats.summary().value(QStringLiteral("casts")).toInt() == 1,
          "Raw Snapchain casts count");
    check(!stats.summary().value(QStringLiteral("hasEngagement")).toBool(),
          "Raw Snapchain must not invent engagement numbers");
    check(!stats.summary().value(QStringLiteral("hasAudience")).toBool(),
          "Raw Snapchain must not invent follower totals");
    check(requestCount >= 7, "All providers and periods must have been requested");
    return 0;
}
