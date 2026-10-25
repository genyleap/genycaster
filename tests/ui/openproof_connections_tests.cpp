#include "openproof_connections_controller.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QSettings>
#include <QTemporaryDir>

#include <cassert>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const auto exact = OpenProofConnectionsController::parseProviderCatalog(R"JSON(
        {"providers":["github","x","github","google","bad provider","X","google"],
         "challenge_providers":["farcaster","ethereum-wallet","local","passkey"]}
    )JSON");
    assert((exact == QStringList{
        QStringLiteral("github"), QStringLiteral("x")
    }));
    assert(OpenProofConnectionsController::parseProviderCatalog("{}").isEmpty());
    assert(OpenProofConnectionsController::parseProviderCatalog("null").isEmpty());
    assert(OpenProofConnectionsController::parseProviderCatalog(
        R"({"providers":["x"]})") == QStringList{QStringLiteral("x")});
    assert(OpenProofConnectionsController::parseProviderCatalog(
        R"({"providers":["x"],"challenge_providers":42})").isEmpty());
    assert(OpenProofConnectionsController::parseProviderCatalog(
        QByteArray(33 * 1024, 'a')).isEmpty());
    assert(OpenProofConnectionsController::parseProviderCatalog(
        R"({"providers":["x"],"challenge_providers":[]})")
        == QStringList{QStringLiteral("x")});

    OpenProofConnectionsController controller{};
    assert(controller.providers().contains(QStringLiteral("x")));
    assert(!controller.providers().contains(QStringLiteral("farcaster")));
    assert(!controller.providers().contains(QStringLiteral("local")));
    controller.setViewerFid(42);
    assert(controller.recoveryGatewayOrigin().startsWith(QStringLiteral("https://")));
    controller.acceptRecoveryGrant(QByteArrayLiteral("invalid-ticket"),
        QStringLiteral("subject-42"), 42, 2700);
    controller.acceptRecoveryGrant(QByteArrayLiteral("recovery.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
        QStringLiteral("subject-42"), 999, 2700);
    assert(!controller.authorized());
    assert(controller.connections().isEmpty());
    controller.connectProvider(QStringLiteral("unknown-provider"));
    assert(!controller.actionBusy());
    controller.setViewerFid(0);
    assert(!controller.authorized());

    // A bounded owner-local snapshot must survive reopening without making
    // cached data an account grant, and must never leak to a different FID.
    QTemporaryDir temp;
    assert(temp.isValid());
    QSettings settings(temp.filePath(QStringLiteral("social.ini")), QSettings::IniFormat);
    const QJsonArray entries{QJsonObject{
        {QStringLiteral("provider"), QStringLiteral("github")},
        {QStringLiteral("subject"), QStringLiteral("developer-123")},
        {QStringLiteral("username"), QStringLiteral("developer")},
        {QStringLiteral("displayName"), QStringLiteral("Developer")}}};
    const QJsonObject snapshot{
        {QStringLiteral("fid"), QStringLiteral("42")},
        {QStringLiteral("verifiedAt"), QDateTime::currentMSecsSinceEpoch()},
        {QStringLiteral("connections"), entries}};
    settings.setValue(QStringLiteral("openProofConnections/lastKnown/42"),
        QString::fromUtf8(QJsonDocument(snapshot).toJson(QJsonDocument::Compact)));
    settings.sync();
    OpenProofConnectionsController restored(nullptr, &settings);
    restored.setViewerFid(42);
    assert(restored.cachedSnapshot());
    assert(!restored.authorized());
    assert(restored.connections().size() == 1);
    restored.setViewerFid(43);
    assert(!restored.cachedSnapshot());
    assert(restored.connections().isEmpty());
    return 0;
}
