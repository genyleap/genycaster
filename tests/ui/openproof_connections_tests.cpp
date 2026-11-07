#include "../../ui/app/identity/connections.hpp"

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
    // Reading a selected topic while the existing account grant restores
    // is pending, not a request to sign a new protocol message.
    controller.selectCastTopic(QStringLiteral("art"), QStringLiteral("42"));
    assert(controller.activeCastTopic() == QStringLiteral("art"));
    assert(controller.topicCastsBusy());
    assert(controller.topicCastsError().isEmpty());
    controller.clearCastTopic();
    assert(!controller.topicCastsBusy());
    controller.selectCastTopic(QStringLiteral("ART"), QStringLiteral("42"));
    assert(controller.activeCastTopic().isEmpty());
    assert(controller.recoveryGatewayOrigin().startsWith(QStringLiteral("https://")));
    controller.acceptRecoveryGrant(QByteArrayLiteral("invalid-ticket"),
        QStringLiteral("subject-42"), 42, 2700);
    controller.acceptRecoveryGrant(QByteArrayLiteral("recovery.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
        QStringLiteral("subject-42"), 999, 2700);
    assert(!controller.authorized());
    assert(controller.connections().isEmpty());
    // Regression: setViewerFid() starts a GET for an existing cloud session.
    // A successful local recovery grant immediately supersedes it with POST.
    // Qt can emit the cancelled GET's finished() synchronously from
    // abort(); previously this reset m_sessionRequest and then caused a null
    // dereference at m_sessionRequest->deleteLater().
    controller.acceptRecoveryGrant(
        QByteArrayLiteral("recovery.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
        QStringLiteral("subject-42"), 42, 2700);
    controller.setViewerFid(43); // Cancel POST while switching accounts.
    controller.acceptRecoveryGrant(
        QByteArrayLiteral("recovery.bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"),
        QStringLiteral("subject-43"), 43, 2700);
    controller.setViewerFid(42); // Cancel second POST and restore original FID.
    assert(!controller.authorized());
    controller.connectProvider(QStringLiteral("unknown-provider"));
    assert(!controller.actionBusy());
    controller.setViewerFid(0);
    assert(!controller.authorized());

    // A bounded owner-local snapshot must survive reopening without making
    // cached data an account grant, and must never leak to a different FID.
    QTemporaryDir temp;
    assert(temp.isValid());
    QSettings settings(temp.filePath(QStringLiteral("social.ini")), QSettings::IniFormat);
    const QJsonArray entries{
        QJsonObject{
            {QStringLiteral("provider"), QStringLiteral("github")},
            {QStringLiteral("subject"), QStringLiteral("developer-123")},
            {QStringLiteral("username"), QStringLiteral("developer")},
            {QStringLiteral("displayName"), QStringLiteral("Developer")}},
        QJsonObject{
            {QStringLiteral("provider"), QStringLiteral("linkedin")},
            {QStringLiteral("subject"), QStringLiteral("linkedin-oidc-456")},
            {QStringLiteral("displayName"), QStringLiteral("Member")}}};
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
    assert(restored.connections().size() == 2);
    restored.setViewerFid(43);
    assert(!restored.cachedSnapshot());
    assert(restored.connections().isEmpty());
    return 0;
}
