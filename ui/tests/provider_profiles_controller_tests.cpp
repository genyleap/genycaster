#include "../mobile/provider_profiles_controller.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QEventLoop>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QTemporaryDir>
#include <QVariantMap>

#include <cassert>

namespace {

[[nodiscard]] QJsonArray saved(QSettings& settings, const QString& name)
{
    const auto raw = settings.value(name).toString();
    const auto parsed = QJsonDocument::fromJson(raw.toUtf8());
    assert(parsed.isArray());
    return parsed.array();
}

void testTypedReadWriteProfilesAndPersistentPreferences()
{
    QTemporaryDir temp{};
    assert(temp.isValid());
    const QString filename = temp.filePath(QStringLiteral("profile-test.ini"));
    {
        QSettings settings{filename, QSettings::IniFormat};
        ProviderProfilesController controller{settings, {}, {}, {}, {}};
        assert(controller.rawReadSupported());
        assert(controller.indexedReadSupported());
        assert(controller.writeSupported());
        assert(controller.readProfiles().empty());
        assert(controller.writeProfiles().empty());
        assert(!controller.readRestartRequired() && !controller.writeRestartRequired());

        assert(controller.addReadProfile(
            QStringLiteral("reader-alpha"), QStringLiteral("My raw node"),
            QStringLiteral("snapchain-v1"), QStringLiteral("https://read.example.test/"),
            true));
        assert(controller.addReadProfile(
            QStringLiteral("reader-beta"), QStringLiteral("Managed index"),
            QStringLiteral("hypersnap-v2"), QStringLiteral("https://index.example.test"),
            false));
        assert(controller.addWriteProfile(
            QStringLiteral("writer-alpha"), QStringLiteral("My validator"),
            QStringLiteral("snapchain-v1"),
            QStringLiteral("https://validator.example.test/"),
            QStringLiteral("https://reconciler.example.test/"), true));

        assert(saved(settings, QStringLiteral("network/readProviderProfilesJson")).size() == 2);
        const auto writers = saved(settings, QStringLiteral("network/writeProviderProfilesJson"));
        assert(writers.size() == 1);
        const auto writer = writers.first().toObject();
        assert(writer.value(QStringLiteral("readEndpoint")).toString()
               == QStringLiteral("https://reconciler.example.test"));
        assert(writer.value(QStringLiteral("endpoint")).toString()
               == QStringLiteral("https://validator.example.test"));
        assert(writer.value(QStringLiteral("selfHosted")).toBool());

        assert(controller.selectReadProvider(QStringLiteral("reader-alpha")));
        assert(controller.selectWriteProvider(QStringLiteral("writer-alpha")));
        assert(controller.selectedReadProviderId() == QStringLiteral("reader-alpha"));
        assert(controller.selectedWriteProviderId() == QStringLiteral("writer-alpha"));
        assert(controller.readRestartRequired() && controller.writeRestartRequired());
        assert(controller.setCompatibleReadFallback(true));
        assert(controller.compatibleReadFallback());
        assert(settings.value(QStringLiteral("network/allowCompatibleReadFallback")).toBool());
        assert(controller.setCompatibleReadFallback(true));
        assert(settings.value(QStringLiteral("network/preferredReadProviderId"))
               == QStringLiteral("reader-alpha"));
        assert(settings.value(QStringLiteral("network/preferredWriteProviderId"))
               == QStringLiteral("writer-alpha"));
    }
    {
        QSettings settings{filename, QSettings::IniFormat};
        ProviderProfilesController controller{
            settings,
            settings.value(QStringLiteral("network/readProviderProfilesJson")).toString(),
            settings.value(QStringLiteral("network/writeProviderProfilesJson")).toString(),
            QStringLiteral("reader-alpha"), QStringLiteral("writer-alpha"), true};
        assert(!controller.readRestartRequired() && !controller.writeRestartRequired());
        assert(controller.activeReadProviderLabel() == QStringLiteral("My raw node"));
        assert(controller.activeReadContract() == QStringLiteral("snapchain-v1"));
        assert(controller.activeReadEndpoint() == QStringLiteral("https://read.example.test"));
        assert(controller.appliedCompatibleReadFallback());
        assert(controller.compatibleReadFallback());
        assert(controller.setCompatibleReadFallback(false));
        assert(!controller.compatibleReadFallback());
        assert(controller.readRestartRequired());
        assert(!controller.writeRestartRequired());
        assert(controller.readProfiles().size() == 2);
        assert(controller.writeProfiles().size() == 1);
        assert(controller.removeReadProfile(QStringLiteral("reader-alpha")));
        assert(controller.selectedReadProviderId().isEmpty());
        assert(controller.writeProfiles().size() == 1);
        assert(controller.readRestartRequired());
        assert(!controller.writeRestartRequired());
        assert(controller.removeWriteProfile(QStringLiteral("writer-alpha")));
        assert(controller.selectedWriteProviderId().isEmpty());
        assert(controller.writeRestartRequired());
        assert(controller.selectReadProvider(QStringLiteral("hypersnap-v2")));
        assert(controller.selectedReadProviderId() == QStringLiteral("hypersnap-v2"));
        assert(controller.selectReadProvider({}));
        settings.setValue(QStringLiteral("network/preferredReadProviderId"),
                          QStringLiteral("reader-beta"));
        settings.sync();
        controller.refresh();
        assert(controller.selectedReadProviderId() == QStringLiteral("reader-beta"));
        assert(!controller.selectReadProvider(QStringLiteral("unknown-provider")));
        assert(controller.lastError() == QStringLiteral("unknown_provider"));
    }
}

void testNoCredentialOrProtocolSpoofing()
{
    QTemporaryDir temp{};
    assert(temp.isValid());
    QSettings settings{temp.filePath(QStringLiteral("invalid.ini")), QSettings::IniFormat};
    ProviderProfilesController controller{settings, {}, {}, {}, {}};

    const auto addRead = [&](const QString& id, const QString& endpoint,
                             const QString& contract = QStringLiteral("snapchain-v1")) {
        return controller.addReadProfile(id, QStringLiteral("node"), contract, endpoint, true);
    };
    assert(!addRead(QStringLiteral("bad id"), QStringLiteral("https://read.example")));
    assert(controller.lastError() == QStringLiteral("invalid_id"));
    assert(!addRead(QStringLiteral("hypersnap-v2"), QStringLiteral("https://read.example")));
    assert(!addRead(QStringLiteral("node"), QStringLiteral("https://token@read.example")));
    assert(!addRead(QStringLiteral("node"), QStringLiteral("https://read.example?key=secret")));
    assert(!addRead(QStringLiteral("node"), QStringLiteral("http://read.example#fragment")));
    assert(!addRead(QStringLiteral("node"), QStringLiteral("file:///etc/passwd")));
    assert(!addRead(QStringLiteral("node"), QStringLiteral("https://read.example"),
                    QStringLiteral("made-up-v5")));
    assert(controller.lastError() == QStringLiteral("unsupported_contract"));
    assert(addRead(QStringLiteral("node"), QStringLiteral("https://read.example")));
    assert(!addRead(QStringLiteral("node"), QStringLiteral("https://read-two.example")));
    assert(controller.lastError() == QStringLiteral("duplicate_id"));
    assert(!controller.addWriteProfile(
        QStringLiteral("node"), QStringLiteral("write"), QStringLiteral("snapchain-v1"),
        QStringLiteral("https://writer.example"), {}, true));
    assert(controller.lastError() == QStringLiteral("duplicate_id"));
    assert(!controller.addWriteProfile(
        QStringLiteral("writer"), QStringLiteral("write"), QStringLiteral("snapchain-v1"),
        QStringLiteral("https://writer.example"),
        QStringLiteral("https://name:token@reconciler.example"), false));
    assert(controller.lastError() == QStringLiteral("invalid_endpoint"));
    assert(!controller.selectWriteProvider(QStringLiteral("node")));
    assert(!controller.removeWriteProfile(QStringLiteral("not-registered")));
    assert(saved(settings, QStringLiteral("network/readProviderProfilesJson")).size() == 1);
    assert(!settings.contains(QStringLiteral("network/writeProviderProfilesJson")));
}


void testLiveReadProbeDistinguishesValidProtocolFromAnyHttp200()
{
    QTemporaryDir temp{};
    assert(temp.isValid());
    QSettings settings{temp.filePath(QStringLiteral("health.ini")), QSettings::IniFormat};
    ProviderProfilesController controller{settings, {}, {}, {}, {}};
    QTcpServer server{};
    assert(server.listen(QHostAddress::LocalHost));
    bool compatible = true;
    bool profileAuthorized = true;
    int castRequests = 0;
    int profileRequests = 0;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            const auto request = socket->readAll();
            const bool casts = request.startsWith(
                "GET /v1/castsByFid?fid=3&pageSize=1&reverse=true ");
            const bool profile = request.startsWith(
                "GET /v1/userDataByFid?fid=3&pageSize=100 ");
            assert(casts || profile);
            if (casts) ++castRequests;
            else ++profileRequests;
            const int status = profile && !profileAuthorized ? 402 : 200;
            const QByteArray body = status == 402
                ? QByteArray{"{\"error\":\"X-PAYMENT header or API key required\"}"}
                : !compatible ? QByteArray{"{\"greeting\":\"not a raw API\"}"}
                : casts ? QByteArray{R"({"messages":[{"hash":"0x123","data":{"fid":3,"castAddBody":{"text":"ok"}}}]})"}
                : QByteArray{R"({"messages":[{"data":{"fid":3,"userDataBody":{"type":"USER_DATA_TYPE_USERNAME","value":"dwr"}}}]})"};
            const auto response = QByteArray{"HTTP/1.1 "}
                + QByteArray::number(status) + (status == 200
                    ? " OK\r\nConnection: close\r\n"
                    : " Payment Required\r\nConnection: close\r\n")
                + "Content-Type: application/json\r\nContent-Length: "
                + QByteArray::number(body.size()) + "\r\n\r\n" + body;
            socket->write(response);
            socket->flush();
            socket->disconnectFromHost();
        });
    });
    const QString endpoint = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    auto check = [&] {
        QEventLoop eventLoop{};
        QObject::connect(&controller, &ProviderProfilesController::probeChanged,
                         &eventLoop, [&] {
            if (controller.probeState() != QStringLiteral("checking"))
                eventLoop.quit();
        });
        assert(controller.probeDraftNode(QStringLiteral("snapchain-v1"), endpoint));
        QTimer::singleShot(4500, &eventLoop, &QEventLoop::quit);
        if (controller.probeState() == QStringLiteral("checking")) eventLoop.exec();
        assert(controller.probeState() != QStringLiteral("checking"));
        assert(controller.probeLatencyMs() >= 0);
    };
    check();
    assert(controller.probeState() == QStringLiteral("healthy"));
    assert(castRequests == 1 && profileRequests == 1);
    profileAuthorized = false;
    check();
    assert(controller.probeState() == QStringLiteral("auth_required"));
    assert(controller.probeHttpStatus() == 402);
    assert(castRequests == 2 && profileRequests == 2);
    profileAuthorized = true;
    compatible = false;
    check();
    assert(controller.probeState() == QStringLiteral("incompatible"));
    assert(castRequests == 3 && profileRequests == 2);
    assert(!controller.probeDraftNode(
        QStringLiteral("snapchain-v1"), QStringLiteral("file:///etc/shadow")));
    assert(controller.probeState() == QStringLiteral("invalid"));
    assert(!settings.contains(QStringLiteral("network/readProviderApiKeysJson")));
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app{argc, argv};
    testTypedReadWriteProfilesAndPersistentPreferences();
    testNoCredentialOrProtocolSpoofing();
    testLiveReadProbeDistinguishesValidProtocolFromAnyHttp200();
    return 0;
}
