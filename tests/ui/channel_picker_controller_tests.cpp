#include "channel_picker_controller.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>

#include <functional>
#include <memory>
#include <print>

namespace {
int failures = 0;

void check(const bool predicate, const char* explanation)
{
    if (!predicate) {
        std::println("FAIL: {}", explanation);
        ++failures;
    }
}

class ChannelApi final : public QObject {
public:
    explicit ChannelApi(QObject* parent = nullptr) : QObject(parent)
    {
        check(m_server.listen(QHostAddress::LocalHost, 0), "localhost test API listens");
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (m_server.hasPendingConnections()) {
                auto* socket = m_server.nextPendingConnection();
                auto request = std::make_shared<QByteArray>();
                connect(socket, &QTcpSocket::readyRead, socket, [socket, request] {
                    if (*request == "REQDONE") return;
                    request->append(socket->readAll());
                    if (!request->contains("\r\n\r\n")) return;
                    const QByteArray target = request->split('\n').first();
                    QByteArray payload{"{\"channels\":[],\"next\":{}}"};
                    if (target.contains("/user/memberships/list")) {
                        payload = R"({"channels":[{"id":"genyleap","name":"genyleap","parent_url":"https://warpcast.com/~/channel/genyleap","member_count":3}],"next":{}})";
                    } else if (target.contains("/channel/search")) {
                        payload = R"({"channels":[{"id":"genyleap","name":"genyleap","parent_url":"https://warpcast.com/~/channel/genyleap","member_count":3},{"id":"art","name":"art","parent_url":"https://warpcast.com/~/channel/art","member_count":100}],"next":{}})";
                    }
                    QByteArray response{"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "};
                    response += QByteArray::number(payload.size()) + "\r\n\r\n" + payload;
                    socket->write(response);
                    *request = "REQDONE";
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    [[nodiscard]] QString baseUrl() const
    {
        return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort());
    }

private:
    QTcpServer m_server{};
};

bool pumpUntil(const std::function<bool()>& done, int maxMs = 3000)
{
    QElapsedTimer clock;
    clock.start();
    while (!done() && clock.elapsed() < maxMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(5);
    }
    return done();
}

QVariantMap findRow(const QVariantList& rows, const QString& id)
{
    for (const QVariant& item : rows) {
        const QVariantMap row = item.toMap();
        if (row.value(QStringLiteral("id")).toString() == id)
            return row;
    }
    return {};
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    check(temp.isValid(), "QSettings fixture exists");
    QSettings settings{temp.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    ChannelApi api;
    ChannelPickerController controller(settings, api.baseUrl(), {}, true, true);
    controller.openForViewer(42);
    check(pumpUntil([&] { return controller.membershipReady(); }),
          "Farcaster viewer membership loads from configured node");
    const QVariantList initial = controller.rows();
    check(!initial.isEmpty() && initial.first().toMap().value(QStringLiteral("isHome")).toBool(),
          "Home/no-channel option always comes first");
    const QVariantMap genyleap = findRow(initial, QStringLiteral("genyleap"));
    check(genyleap.value(QStringLiteral("selectable")).toBool(),
          "joined channel is selectable");
    check(genyleap.value(QStringLiteral("parentUrl")).toString()
              == QStringLiteral("https://warpcast.com/~/channel/genyleap"),
          "canonical URL comes from selected read node");

    int selections = 0;
    QVariantMap selected{};
    QObject::connect(&controller, &ChannelPickerController::channelChosen, &app,
        [&](const QVariantMap& value) {
            ++selections;
            selected = value;
        });
    const QVariantMap immediate = controller.chooseChannel(QStringLiteral("genyleap"));
    check(immediate.value(QStringLiteral("selectable")).toBool(),
          "selectChannel accepts the authorized channel");
    check(selections == 0, "selection signal is deferred until delegate tap finishes");
    check(pumpUntil([&] { return selections == 1; }),
          "selection signal is delivered on next event-loop iteration");
    check(selected.value(QStringLiteral("id")).toString() == QStringLiteral("genyleap"),
          "selected channel details survive the model update");

    controller.search(QStringLiteral("art"));
    check(pumpUntil([&] {
        return !findRow(controller.rows(), QStringLiteral("art")).isEmpty();
    }), "search returns channels matching the user query");
    const QVariantMap other = findRow(controller.rows(), QStringLiteral("art"));
    check(!other.value(QStringLiteral("selectable")).toBool(),
          "channels without verified membership remain disabled");
    check(controller.chooseChannel(QStringLiteral("art")).isEmpty(),
          "nonmember cannot be chosen even by directly calling C++");

    controller.chooseChannel({});
    check(pumpUntil([&] { return selections == 2; }), "Home emits selection");
    check(selected.value(QStringLiteral("isHome")).toBool()
              && selected.value(QStringLiteral("parentUrl")).toString().isEmpty(),
          "Home clears the channel without publishing a parent_url");

    if (failures == 0) std::println("PASS: channel selection lifecycle");
    return failures == 0 ? 0 : 1;
}
