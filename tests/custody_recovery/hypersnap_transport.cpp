#ifdef NDEBUG
#undef NDEBUG
#endif
#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QNetworkReply>
#include <QSettings>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>

#include "../../ui/app/identity/hypersnap_registration_transport.hpp"
#include "../../ui/app/identity/native_registration.hpp"
#include "../../ui/app/identity/native_publish.hpp"

namespace {
using Transport = HyperSnapRegistrationTransport;
constexpr auto Custody = "0xf278cf59f82edcf871d630f28ecc8056f25c1cdb";

QByteArray response(int code, QByteArray json)
{
    const QByteArray status = code == 200 ? "OK"
        : code == 404 ? "Not Found" : "Bad Request";
    return "HTTP/1.1 " + QByteArray::number(code) + " " + status
        + "\r\nContent-Type: application/json\r\nContent-Length: "
        + QByteArray::number(json.size())
        + "\r\nConnection: close\r\n\r\n" + json;
}

class FakeHub final {
public:
    QTcpServer server;
    QByteArray lastRequest;
    std::function<QByteArray(const QByteArray&)> reply;
    FakeHub()
    {
        assert(server.listen(QHostAddress::LocalHost));
        QObject::connect(&server, &QTcpServer::newConnection, &server,
            [this] {
                while (auto* socket = server.nextPendingConnection()) {
                    struct RequestState { QByteArray bytes; bool responded{false}; };
                    auto state = std::make_shared<RequestState>();
                    QObject::connect(socket, &QTcpSocket::readyRead, socket,
                        [this, socket, state] {
                            if (state->responded) return;
                            state->bytes.append(socket->readAll());
                            const qsizetype headersEnd = state->bytes.indexOf("\r\n\r\n");
                            if (headersEnd < 0) return;
                            qsizetype bodyBytes = 0;
                            const auto headers = state->bytes.left(headersEnd);
                            for (const auto& line : headers.split('\n')) {
                                if (line.toLower().startsWith("content-length:"))
                                    bodyBytes = line.mid(15).trimmed().toLongLong();
                            }
                            if (state->bytes.size() < headersEnd + 4 + bodyBytes) return;
                            state->responded = true;
                            lastRequest = state->bytes;
                            const auto payload = reply(lastRequest);
                            socket->write(payload);
                            socket->flush();
                            socket->disconnectFromHost();
                        });
                    QObject::connect(socket, &QTcpSocket::disconnected,
                                     socket, &QTcpSocket::deleteLater);
                }
            });
    }
    QUrl url() const
    {
        return QUrl{QStringLiteral("http://127.0.0.1:%1")
            .arg(server.serverPort())};
    }
};

template <typename Function>
void await(Function start)
{
    QEventLoop loop{};
    bool completed = false;
    QTimer deadline{};
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    deadline.start(4000);
    start([&] { completed = true; loop.quit(); });
    if (!completed) loop.exec();
    assert(completed && "localhost registration transport timed out");
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Genycaster-RegistrationTest"));
    QCoreApplication::setApplicationName(QStringLiteral("HyperSnapNativeTest"));
    // The pending-attempt journal is real QSettings state. Keep the
    // regression fully isolated from a user's or a previous CI run's data.
    QTemporaryDir settingsHome{};
    assert(settingsHome.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsHome.path());
    FakeHub mock{};
    Transport client{mock.url()};
    assert(client.endpointIsSafe());
    constexpr auto ExpectedFid = (1ULL << 32);
    const auto address = QString::fromLatin1(Custody);

    mock.reply = [](const QByteArray&) {
        return response(400,
            R"json({"error":"Native registration lookup failed","error_detail":"status: NotFound, message: \"native account not found\""})json");
    };
    await([&](auto done) {
        client.lookupByCustody(address,[&, done](auto result) {
            assert(result.state == Transport::LookupState::NotFound);
            assert(result.httpStatus == 400);
            done();
        });
    });
    assert(mock.lastRequest.startsWith(
        "GET /v1/nativeAccount?custody=0xf278cf59f82edcf871d630f28ecc8056f25c1cdb "));
    // An arbitrary 404 is not authoritative proof of an unregistered wallet.
    mock.reply = [](const QByteArray&) {
        return response(404, "{\"error\":\"not_found\"}");
    };
    await([&](auto done) {
        client.lookupByCustody(address,[&, done](auto result) {
            assert(result.state == Transport::LookupState::Unavailable);
            assert(result.httpStatus == 404);
            done();
        });
    });

    mock.reply = [](const QByteArray&) {
        return response(200,
            "{\"fid\":\"4294967296\","
            "\"custodyAddress\":\"0xf278cf59f82edcf871d630f28ecc8056f25c1cdb\","
            "\"blockNumber\":54321,\"message\":{\"data\":{}}}");
    };
    await([&](auto done) {
        client.lookupByCustody(address,[&, done](auto result) {
            assert(result.state == Transport::LookupState::Confirmed);
            assert(result.httpStatus == 200);
            assert(result.fid == ExpectedFid);
            assert(result.blockNumber == 54321);
            done();
        });
    });
    mock.reply = [](const QByteArray&) {
        return response(200,
            "{\"fid\":\"4294967296\",\"custodyAddress\":"
            "\"0x0000000000000000000000000000000000000001\","
            "\"blockNumber\":54321,\"message\":{}}");
    };
    await([&](auto done) {
        client.lookupByCustody(address,[&, done](auto result) {
            assert(result.state == Transport::LookupState::InvalidResponse);
            done();
        });
    });

    mock.reply = [](const QByteArray&) {
        return response(200,
            "{\"fid\":\"4294967296\","
            "\"custodyAddress\":\"0xf278cf59f82edcf871d630f28ecc8056f25c1cdb\","
            "\"blockNumber\":54321,\"message\":{\"data\":{}}}");
    };
    NativeRegistrationStatusController status{mock.url().toString()};
    {
        QEventLoop loop{};
        bool finished = false;
        QObject::connect(&status, &NativeRegistrationStatusController::lookupChanged,
            &loop, [&] {
                if (status.accountLookupChecked() && !status.accountLookupBusy()) {
                    finished = true;
                    loop.quit();
                }
            });
        status.findExistingNativeAccount(address);
        QTimer::singleShot(4000, &loop, &QEventLoop::quit);
        if (!finished) loop.exec();
        assert(finished);
        assert(status.existingNativeFid() == QStringLiteral("4294967296"));
        assert(!status.accountLookupFailed());
        assert(!status.verifiedAbsentForCustody(address));
        status.clearExistingLookup();
        assert(status.existingNativeFid().isEmpty());
        assert(!status.accountLookupChecked());
        assert(!status.verifiedAbsentForCustody(address));
    }
    mock.reply = [](const QByteArray&) {
        return response(400,
            R"json({"error":"Native registration lookup failed","error_detail":"status: NotFound, message: \"native account not found\""})json");
    };
    {
        QEventLoop loop{};
        bool finished = false;
        QObject::connect(&status, &NativeRegistrationStatusController::lookupChanged,
            &loop, [&] {
                if (status.accountLookupChecked() && !status.accountLookupBusy()) {
                    finished = true;
                    loop.quit();
                }
            });
        status.findExistingNativeAccount(address);
        QTimer::singleShot(4000, &loop, &QEventLoop::quit);
        if (!finished) loop.exec();
        assert(finished);
        assert(status.verifiedAbsentForCustody(address));
        assert(!status.verifiedAbsentForCustody(
            QStringLiteral("0x0000000000000000000000000000000000000001")));
    }

    const QByteArray protobuf{"\x0a\x05\x00\x10\xff", 5};
    mock.reply = [](const QByteArray& bytes) {
        assert(bytes.startsWith("POST /v1/submitMessage "));
        assert(bytes.contains("Content-Type: application/octet-stream"));
        assert(bytes.endsWith(QByteArray{"\x0a\x05\x00\x10\xff", 5}));
        return response(200, "{\"hash\":\"0x1234\"}");
    };
    await([&](auto done) {
        client.submit(protobuf,[&, done](auto result) {
            assert(result.state == Transport::SubmitState::AcceptedPending);
            assert(result.httpStatus == 200);
            done();
        });
    });
    mock.reply = [](const QByteArray&) {
        return response(400, "{\"error\":\"invalid_proof\"}");
    };
    await([&](auto done) {
        client.submit(protobuf,[&, done](auto result) {
            assert(result.state == Transport::SubmitState::Rejected);
            assert(result.httpStatus == 400);
            done();
        });
    });
    mock.reply = [](const QByteArray&) {
        return response(503, "{\"error\":\"try_again\"}");
    };
    await([&](auto done) {
        client.submit(protobuf,[&, done](auto result) {
            assert(result.state == Transport::SubmitState::UnknownOutcome);
            assert(result.httpStatus == 503);
            done();
        });
    });

    Transport unsafe{QUrl{"http://example.com"}};
    assert(!unsafe.endpointIsSafe());
    bool rejected = false;
    unsafe.submit(protobuf,[&](auto result) {
        rejected = result.state == Transport::SubmitState::Rejected;
    });
    assert(rejected);
    // Full state machine: no automatic replay, cannot publish without the
    // offline recovery ceremony, and HTTP 200 never implies a registered FID.
    {
        FakeHub hub{};
        bool backedUp = false;
        bool proofReady = true;
        bool checkedAbsent = true;
        bool anchorFresh = true;
        int postCount = 0;
        bool confirmedRecord = false;
        hub.reply = [&](const QByteArray& request) {
            if (request.startsWith("POST ")) {
                ++postCount;
                assert(request.contains("application/octet-stream"));
                return response(200, "{\"hash\":\"0x1234\"}");
            }
            if (confirmedRecord)
                return response(200,
                    "{\"fid\":\"4294967296\","
                    "\"custodyAddress\":\"0xf278cf59f82edcf871d630f28ecc8056f25c1cdb\","
                    "\"blockNumber\":54321,\"message\":{\"data\":{}}}");
            return response(400,
            R"json({"error":"Native registration lookup failed","error_detail":"status: NotFound, message: \"native account not found\""})json");
        };
        HyperSnapRegistrationPublisher publisher{
            hub.url(),
            NativePublicationPorts{
                .backupVerified = [&] { return backedUp; },
                .custodyAddress = [&] { return address; },
                .anchorIsFresh = [&] { return anchorFresh; },
                .walletIsUnregistered = [&](const QString& value) {
                    return checkedAbsent && value == address;
                },
                .proofPrepared = [&] { return proofReady; },
                .takeProof = [&](const QString& value) {
                    assert(value == address);
                    proofReady = false;
                    return protobuf;
                },
                .cancelProof = [&] { proofReady = false; }
            }};
        assert(!publisher.submitPrepared());
        assert(postCount == 0);
        backedUp = true;
        assert(!publisher.submitPrepared()); // No authenticated runtime.
        publisher.setIdentityRuntimeReady(true);
        assert(publisher.submitPrepared());
        await([&](auto done) {
            auto connection = std::make_shared<QMetaObject::Connection>();
            *connection = QObject::connect(
                &publisher, &HyperSnapRegistrationPublisher::changed,
                &publisher, [&, connection, done] {
                    if (!publisher.busy() && publisher.pending()) {
                        QObject::disconnect(*connection);
                        done();
                    }
                });
        });
        assert(publisher.pending());
        assert(publisher.lastSubmissionHttpStatus() == 200);
        assert(!publisher.checkingConfirmation());
        assert(!publisher.confirmed());
        assert(publisher.registeredFid().isEmpty());
        assert(postCount == 1);
        assert(!publisher.submitPrepared()); // Replay protection.
        publisher.clear(); // UI dismissal cannot cancel an on-chain write.
        assert(publisher.pending() && publisher.outcomeUnknown());
        assert(!publisher.submitPrepared());
        assert(postCount == 1);
        confirmedRecord = true;
        assert(publisher.checkConfirmation());
        assert(publisher.checkingConfirmation());
        await([&](auto done) {
            auto connection = std::make_shared<QMetaObject::Connection>();
            *connection = QObject::connect(
                &publisher, &HyperSnapRegistrationPublisher::changed,
                &publisher, [&, connection, done] {
                    if (publisher.confirmed()) {
                        QObject::disconnect(*connection);
                        done();
                    }
                });
        });
        assert(publisher.registeredFid() == QStringLiteral("4294967296"));
        assert(!publisher.checkingConfirmation());
        assert(postCount == 1);
        publisher.clear();
        assert(!publisher.confirmed() && !publisher.pending());
    }
    // Simulate a full process restart after an ambiguous native registration.
    // The recovered offline wallet is used only for read-only shard-0 queries.
    {
        FakeHub hub{};
        int postCount = 0;
        bool included = false;
        hub.reply = [&](const QByteArray& request) {
            if (request.startsWith("POST ")) {
                ++postCount;
                return response(500, "{\"error\":\"unexpected_write\"}");
            }
            if (included)
                return response(200,
                    "{\"fid\":\"4294967296\","
                    "\"custodyAddress\":\"0xf278cf59f82edcf871d630f28ecc8056f25c1cdb\","
                    "\"blockNumber\":54321,\"message\":{\"data\":{}}}");
            return response(400,
            R"json({"error":"Native registration lookup failed","error_detail":"status: NotFound, message: \"native account not found\""})json");
        };
        bool verified = true;
        HyperSnapRegistrationPublisher recovered{
            hub.url(),
            NativePublicationPorts{
                .backupVerified = [&] { return verified; },
                .custodyAddress = [&] { return address; }
            }};
        recovered.setIdentityRuntimeReady(true);
        assert(recovered.recoverInterruptedRegistration());
        assert(recovered.pending());
        assert(!recovered.submitPrepared());
        await([&](auto done) {
            auto connection = std::make_shared<QMetaObject::Connection>();
            *connection = QObject::connect(
                &recovered, &HyperSnapRegistrationPublisher::changed,
                &recovered, [&, connection, done] {
                    if (!recovered.busy()) {
                        QObject::disconnect(*connection);
                        done();
                    }
                });
        });
        assert(!recovered.confirmed());
        assert(recovered.pending());
        assert(recovered.confirmationLookupState() == QStringLiteral("not_found"));
        assert(recovered.confirmationLookupHttpStatus() == 400);
        assert(recovered.confirmationLookupChecks() >= 1);
        assert(!recovered.submitPrepared());
        assert(postCount == 0);
        included = true;
        assert(recovered.checkConfirmation());
        await([&](auto done) {
            auto connection = std::make_shared<QMetaObject::Connection>();
            *connection = QObject::connect(
                &recovered, &HyperSnapRegistrationPublisher::changed,
                &recovered, [&, connection, done] {
                    if (recovered.confirmed()) {
                        QObject::disconnect(*connection);
                        done();
                    }
                });
        });
        assert(recovered.confirmed());
        assert(recovered.confirmationLookupState() == QStringLiteral("confirmed"));
        assert(recovered.confirmationLookupHttpStatus() == 200);
        assert(recovered.registeredFid() == QStringLiteral("4294967296"));
        assert(postCount == 0);
    }
    // An earlier unknown submission may be released only when the SAME
    // shard-0 head advances beyond the onboard anchor window plus margin.
    // This release never POSTs or creates another wallet.
    {
        FakeHub hub{};
        int postCount = 0;
        std::uint64_t head = 8'000;
        bool validNode = true;
        hub.reply = [&](const QByteArray& request) {
            if (request.startsWith("POST ")) {
                ++postCount;
                return response(500, R"json({"error":"unexpected_write"})json");
            }
            return response(400,
                R"json({"error":"Native registration lookup failed","error_detail":"status: NotFound, message: \"native account not found\""})json");
        };
        HyperSnapRegistrationPublisher recovered{
            hub.url(),
            NativePublicationPorts{
                .backupVerified = [] { return true; },
                .custodyAddress = [&] { return address; },
                .registrationWindow = [&]() -> std::optional<
                    std::pair<std::uint64_t, std::uint32_t>> {
                    if (!validNode) return std::nullopt;
                    return std::pair{head, std::uint32_t{4}};
                }
            }};
        recovered.setIdentityRuntimeReady(true);
        assert(recovered.recoverInterruptedRegistration());
        await([&](auto done) {
            auto connection = std::make_shared<QMetaObject::Connection>();
            *connection = QObject::connect(
                &recovered, &HyperSnapRegistrationPublisher::changed,
                &recovered, [&, connection, done] {
                    if (!recovered.busy()) {
                        QObject::disconnect(*connection);
                        done();
                    }
                });
        });
        assert(!recovered.retryReady());
        assert(recovered.retryRemainingBlocks() == 20);
        assert(!recovered.releaseExpiredAttempt());
        // The first observed head is saved without any mnemonic, signature
        // or proof material. Back/navigation or process restart must not
        // reset the 1,040-block safety window.
        recovered.stopPolling();
        head = 8'002;
        {
            HyperSnapRegistrationPublisher resumed{
                hub.url(),
                NativePublicationPorts{
                    .backupVerified = [] { return true; },
                    .custodyAddress = [&] { return address; },
                    .registrationWindow = [&]() -> std::optional<
                        std::pair<std::uint64_t, std::uint32_t>> {
                        if (!validNode) return std::nullopt;
                        return std::pair{head, std::uint32_t{4}};
                    }
                }};
            resumed.setIdentityRuntimeReady(true);
            assert(resumed.recoverInterruptedRegistration());
            await([&](auto done) {
                auto connection = std::make_shared<QMetaObject::Connection>();
                *connection = QObject::connect(
                    &resumed, &HyperSnapRegistrationPublisher::changed,
                    &resumed, [&, connection, done] {
                        if (!resumed.busy()) {
                            QObject::disconnect(*connection);
                            done();
                        }
                    });
            });
            assert(resumed.retryRemainingBlocks() == 18);
            assert(!resumed.retryReady());
            resumed.stopPolling();
        }
        head = 8'019; // Still inside the conservative 4 + 16 window.
        assert(recovered.checkConfirmation());
        await([&](auto done) {
            auto connection = std::make_shared<QMetaObject::Connection>();
            *connection = QObject::connect(
                &recovered, &HyperSnapRegistrationPublisher::changed,
                &recovered, [&, connection, done] {
                    if (!recovered.busy()) {
                        QObject::disconnect(*connection);
                        done();
                    }
                });
        });
        assert(recovered.retryRemainingBlocks() == 1);
        assert(!recovered.releaseExpiredAttempt());
        head = 8'020;
        assert(recovered.checkConfirmation());
        await([&](auto done) {
            auto connection = std::make_shared<QMetaObject::Connection>();
            *connection = QObject::connect(
                &recovered, &HyperSnapRegistrationPublisher::changed,
                &recovered, [&, connection, done] {
                    if (!recovered.busy()) {
                        QObject::disconnect(*connection);
                        done();
                    }
                });
        });
        assert(recovered.retryReady());
        validNode = false;
        assert(!recovered.releaseExpiredAttempt()); // Never bypass a stale node.
        validNode = true;
        assert(recovered.releaseExpiredAttempt());
        assert(!recovered.pending());
        assert(!recovered.retryReady());
        assert(postCount == 0);
    }
    // Switching to a different local wallet must never POST again, discard
    // the old wallet's pending protection or change that wallet's on-chain ID.
    {
        FakeHub hub{};
        int postCount = 0;
        std::uint64_t head = 9'000;
        hub.reply = [&](const QByteArray& request) {
            if (request.startsWith("POST ")) {
                ++postCount;
                return response(500, R"json({"error":"unexpected_write"})json");
            }
            return response(400,
                R"json({"error":"Native registration lookup failed","error_detail":"status: NotFound, message: \"native account not found\""})json");
        };
        auto ports = [&]() {
            return NativePublicationPorts{
                .backupVerified = [] { return true; },
                .custodyAddress = [&] { return address; },
                .registrationWindow = [&]() -> std::optional<
                    std::pair<std::uint64_t, std::uint32_t>> {
                    return std::pair{head, std::uint32_t{4}};
                }
            };
        };
        HyperSnapRegistrationPublisher oldWallet{hub.url(), ports()};
        oldWallet.setIdentityRuntimeReady(true);
        assert(oldWallet.recoverInterruptedRegistration());
        assert(!oldWallet.detachWalletForNewRegistration()); // Busy lookup.
        await([&](auto done) {
            auto connection = std::make_shared<QMetaObject::Connection>();
            *connection = QObject::connect(
                &oldWallet, &HyperSnapRegistrationPublisher::changed,
                &oldWallet, [&, connection, done] {
                    if (!oldWallet.busy()) {
                        QObject::disconnect(*connection);
                        done();
                    }
                });
        });
        assert(oldWallet.pending());
        assert(oldWallet.retryRemainingBlocks() == 20);
        assert(oldWallet.detachWalletForNewRegistration());
        assert(!oldWallet.pending());
        assert(!oldWallet.outcomeUnknown());
        assert(postCount == 0);
        head = 9'002;
        HyperSnapRegistrationPublisher restoredOldWallet{hub.url(), ports()};
        restoredOldWallet.setIdentityRuntimeReady(true);
        assert(restoredOldWallet.recoverInterruptedRegistration());
        await([&](auto done) {
            auto connection = std::make_shared<QMetaObject::Connection>();
            *connection = QObject::connect(
                &restoredOldWallet, &HyperSnapRegistrationPublisher::changed,
                &restoredOldWallet, [&, connection, done] {
                    if (!restoredOldWallet.busy()) {
                        QObject::disconnect(*connection);
                        done();
                    }
                });
        });
        assert(restoredOldWallet.pending());
        assert(restoredOldWallet.retryRemainingBlocks() == 18);
        assert(postCount == 0);
        restoredOldWallet.stopPolling();
    }
    std::cout << "HyperSnap native lookup, restart recovery, no replay passed\n";
}
