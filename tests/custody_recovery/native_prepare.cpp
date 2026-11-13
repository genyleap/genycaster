#ifdef NDEBUG
#undef NDEBUG
#endif
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <cassert>
#include <iostream>
#include <optional>

#include "../../ui/app/identity/native_prepare.hpp"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using Challenge = NativeRegistrationStatusController::Challenge;
    std::optional<Challenge> challenge{Challenge{
        .anchorHeight = 100,
        .nextBlock = 101,
        .anchorHash = QStringLiteral("0x") + QString(64, QLatin1Char('a')),
        .window = 1024,
        .difficultyBits = 12,
    }};
    bool backedUp = false;
    bool walletCheckPassed = false;
    QString address = QStringLiteral(
        "0xf278cf59f82edcf871d630f28ecc8056f25c1cdb");
    unsigned seals = 0;
    NativeRegistrationPreparationController prep{
        NativePreparationPorts{
            .backupVerified = [&] { return backedUp; },
            .custodyAddress = [&] { return address; },
            .walletIsUnregistered = [&](const QString& checked) {
                return walletCheckPassed
                    && checked == QStringLiteral("0xf278cf59f82edcf871d630f28ecc8056f25c1cdb");
            },
            .challenge = [&] { return challenge; },
            .seal = [&](const Challenge& snapshot, const QString& nonce) {
                ++seals;
                assert(snapshot.anchorHeight == 100);
                assert(nonce.startsWith(QStringLiteral("0x")));
                assert(nonce.size() == 18);
                return QByteArray{"sealed-public-envelope"};
            }
        }
    };
    assert(!prep.prepare());
    assert(prep.failed() && !prep.prepared());
    backedUp = true;
    assert(!prep.prepare()); // Wallet backup alone is insufficient.
    walletCheckPassed = true;
    auto finish = [&] {
        QEventLoop loop{};
        QTimer watchdog{};
        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout,
                         &loop, &QEventLoop::quit);
        QObject::connect(&prep, &NativeRegistrationPreparationController::changed,
                         &loop, [&] { if (!prep.busy()) loop.quit(); });
        watchdog.start(5000);
        if (prep.busy()) loop.exec();
        assert(!prep.busy() && "PoW preparation exceeded test deadline");
    };
    assert(prep.prepare());
    assert(prep.busy());
    finish();
    assert(prep.prepared() && !prep.failed());
    assert(prep.preparedEnvelope() == QByteArray{"sealed-public-envelope"});
    assert(seals == 1);
    const auto take = prep.takeForSubmission(address);
    assert(take == QByteArray{"sealed-public-envelope"});
    assert(!prep.prepared());
    assert(prep.takeForSubmission(address).isEmpty()); // One-shot, no replay.
    prep.cancel();
    assert(prep.preparedEnvelope().isEmpty());

    // Changing the wallet during hashing invalidates the result.
    assert(prep.prepare());
    address = QStringLiteral(
        "0x0000000000000000000000000000000000000001");
    finish();
    assert(!prep.prepared() && prep.failed());
    address = QStringLiteral(
        "0xf278cf59f82edcf871d630f28ecc8056f25c1cdb");

    // Cancellation is checked during hashing; no stale envelope can emerge.
    challenge->difficultyBits = 256;
    assert(prep.prepare());
    prep.cancel();
    finish();
    assert(!prep.prepared() && !prep.failed());
    assert(seals == 1);
    std::cout << "Bounded asynchronous native PoW and custody isolation passed\n";
}
