#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../ui/app/identity/neynar_registration_controller.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <cassert>
#include <memory>
#include <iostream>
namespace {
constexpr auto Owner = "0xf278cf59f82edcf871d630f28ecc8056f25c1cdb";
struct Hub {
    QTcpServer server{};
    QString reservation{};
    int reservations{}, transfers{}, checks{};
    bool confirmed{};
    bool serviceDisabled{};
    Hub() {
        assert(server.listen(QHostAddress::LocalHost));
        QObject::connect(&server,&QTcpServer::newConnection,&server,[this] {
            while(auto* sock=server.nextPendingConnection()) {
                auto data=std::make_shared<QByteArray>();
                QObject::connect(sock,&QTcpSocket::readyRead,sock,[this,sock,data] {
                    data->append(sock->readAll());
                    const qsizetype end=data->indexOf("\r\n\r\n");
                    if(end<0)return;
                    qsizetype bytes{};
                    for(const auto& line:data->left(end).split('\n'))
                        if(line.toLower().startsWith("content-length:"))
                            bytes=line.mid(15).trimmed().toLongLong();
                    if(data->size()<end+4+bytes)return;
                    const auto body=QJsonDocument::fromJson(data->mid(end+4,bytes)).object();
                    const auto path=data->left(data->indexOf("\r\n"));
                    // Real paid relay challenges unauthenticated callers
                    // BEFORE touching the funded FID allocator.
                    if(path.contains("/reserve") && !data->left(end).toLower()
                           .contains("x-genycaster-paid-enrollment:")) {
                        const QByteArray error=R"({"error":"paid_enrollment_required"})";
                        sock->write("HTTP/1.1 401 Unauthorized\r\n"
                            "Content-Type: application/json\r\nContent-Length: "
                            +QByteArray::number(error.size())
                            +"\r\nConnection: close\r\n\r\n"+error);
                        sock->flush();
                        sock->disconnectFromHost();
                        return;
                    }
                    if(path.contains("/reserve") && serviceDisabled) {
                        const QByteArray error =
                            R"({"error":"registration_not_configured"})";
                        sock->write("HTTP/1.1 503 Service Unavailable\r\n"
                            "Content-Type: application/json\r\nContent-Length: "
                            +QByteArray::number(error.size())
                            +"\r\nConnection: close\r\n\r\n"+error);
                        sock->flush();
                        sock->disconnectFromHost();
                        return;
                    }
                    QString status;
                    if(path.contains("/reserve")) {
                        ++reservations;
                        reservation=body.value("idempotency_key").toString();
                        status="reserved";
                    }else if(path.contains("/submit")) {
                        ++transfers;
                        assert(body.value("fname").toString()=="genycaster");
                        assert(body.value("signature").toString().size()==132);
                        status="submitted";
                    }else {
                        ++checks;
                        status=confirmed?"confirmed":"submitted";
                    }
                    const auto response=QJsonDocument{QJsonObject{
                        {"registration",QJsonObject{
                            {"reservation_id",reservation},
                            {"fid",QStringLiteral("123456")},
                            {"custody_address",QString::fromLatin1(Owner)},
                            {"reserved_at",double(QDateTime::currentMSecsSinceEpoch())},
                            {"expires_at",double(QDateTime::currentMSecsSinceEpoch()+240000)},
                            {"id_registry_nonce",QStringLiteral("3")},
                            {"status",status}
                        }}
                    }}.toJson(QJsonDocument::Compact);
                    sock->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                        "Content-Length: "+QByteArray::number(response.size())
                        +"\r\nConnection: close\r\n\r\n"+response);
                    sock->flush();
                    sock->disconnectFromHost();
                });
                QObject::connect(sock,&QTcpSocket::disconnected,
                                 sock,&QTcpSocket::deleteLater);
            }
        });
    }
    QUrl url()const {
        return QUrl{QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())};
    }
};
void untilAvailable(NeynarRegistrationController& c) {
    if(c.available() && !c.checkingAvailability()) return;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer,&QTimer::timeout,&loop,&QEventLoop::quit);
    QObject::connect(&c,&NeynarRegistrationController::changed,
                     &loop,[&]{
        if(c.available() && !c.checkingAvailability())loop.quit();
    });
    timer.start(5000);
    loop.exec();
    assert(c.available() && !c.checkingAvailability());
}
void until(NeynarRegistrationController& c, const QString& phase) {
    if(c.phase()==phase&&!c.busy())return;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer,&QTimer::timeout,&loop,&QEventLoop::quit);
    QObject::connect(&c,&NeynarRegistrationController::changed,
                     &loop,[&]{if(c.phase()==phase&&!c.busy())loop.quit();});
    timer.start(5000);
    loop.exec();
    assert(c.phase()==phase&&!c.busy());
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    Hub hub{};
    bool verified=false;
    int signatures=0;
    QString custody=QString::fromLatin1(Owner);
    NeynarRegistrationController controller{
        hub.url(),
        FarcasterRegistrationPorts{
            .backupVerified=[&]{return verified;},
            .custodyAddress=[&]{return custody;},
            .signTransfer=[&](std::uint64_t fid,std::uint64_t nonce,std::uint64_t deadline)
                ->std::optional<QString>{
                assert(fid==123456 && nonce==3);
                assert(deadline>std::uint64_t(QDateTime::currentSecsSinceEpoch()));
                ++signatures;
                return QStringLiteral("0x")+QString(130,QLatin1Char('a'));
            }
        },nullptr,true};
    const QString voucher=QString(90,QLatin1Char('A'))+"."
                         +QString(43,QLatin1Char('B'));
    assert(!controller.useEnrollmentVoucher(voucher));
    verified=true;
    assert(!controller.useEnrollmentVoucher(voucher)); // No signer runtime.
    controller.setIdentityRuntimeReady(true);
    untilAvailable(controller); // Distinguish protected 401 from missing relay.
    assert(controller.useEnrollmentVoucher(voucher));
    assert(!controller.submit("genycaster"));
    hub.serviceDisabled=true;
    assert(controller.reserveFid());
    until(controller,"ready");
    assert(!controller.pending());
    assert(controller.error()=="registration_not_configured");
    assert(hub.reservations==0);
    hub.serviceDisabled=false;
    assert(controller.reserveFid());
    assert(controller.pending()); // A cost-bearing reservation is irreversible.
    assert(!controller.reset());
    until(controller,"reserved");
    assert(hub.reservations==1);
    assert(controller.canSubmit());
    assert(!controller.reserveFid());
    assert(controller.submit("genycaster"));
    until(controller,"submitted");
    assert(signatures==1 && hub.transfers==1);
    assert(!controller.reset());
    controller.stopPolling();
    assert(controller.pending());
    hub.confirmed=true;
    QString confirmedFid;
    QObject::connect(&controller,&NeynarRegistrationController::farcasterFidConfirmed,
                     &controller,[&](QString fid,QString address){
                         assert(address==custody);confirmedFid=fid;
                     });
    assert(controller.checkConfirmation());
    until(controller,"confirmed");
    assert(confirmedFid=="123456" && !controller.hasVoucher());
    assert(hub.transfers==1 && hub.checks>=1);
    std::cout<<"Neynar native registration flow passed\n";
}
