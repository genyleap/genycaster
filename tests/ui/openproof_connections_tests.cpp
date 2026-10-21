#include "openproof_connections_controller.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cassert>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const auto exact = OpenProofConnectionsController::parseProviderCatalog(R"JSON(
        {"providers":["github","x","github","google","bad provider","X","google"],
         "challenge_providers":["farcaster","ethereum-wallet","local","passkey"]}
    )JSON");
    assert((exact == QStringList{
        QStringLiteral("ethereum-wallet"), QStringLiteral("farcaster"),
        QStringLiteral("github"), QStringLiteral("google"), QStringLiteral("x")
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
    assert(controller.providers().contains(QStringLiteral("farcaster")));
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
    return 0;
}
