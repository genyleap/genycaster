#include "native_wallet.hpp"

#include <QDateTime>
#include <QGuiApplication>
#include <QMetaProperty>
#include <QPainter>
#include <QImage>
#include <cassert>
#include <iostream>

int main(int argc, char** argv)
{
    QGuiApplication application(argc, argv);
    NativeCustodyWalletController wallet{};
    const auto* meta = wallet.metaObject();
    // A wallet is a native secret, never an inspectable Qt/QML property.
    assert(meta->indexOfProperty("recoveryWords") == -1);
    assert(meta->indexOfMethod("recoveryWords()") == -1);
    assert(meta->indexOfMethod("signReservedFarcasterTransfer(quint64,quint64,quint64)") == -1);
    assert(meta->indexOfMethod("signOpenProofWalletChallenge(quint64,QString)") == -1);
    assert(meta->indexOfMethod("signOpenProofFarcasterChallenge(quint64,QString)") == -1);
    assert(!wallet.hasWallet() && !wallet.backupVerified());
    assert(wallet.address().isEmpty());
    assert(wallet.generate());
    assert(wallet.hasWallet() && wallet.recoveryVisible());
    // A newly generated but not backed-up wallet is never signing authority.
    assert(!wallet.signReservedFarcasterTransfer(1234567, 0, 1794560000));
    assert(wallet.address().startsWith("0x") && wallet.address().size() == 42);
    const QString custody = wallet.address();
    assert(!wallet.generate()); // A second click must not silently replace wallet.
    assert(wallet.startBackupChallenge());
    assert(!wallet.recoveryVisible());
    assert(wallet.firstChallengeIndex() >= 0 && wallet.firstChallengeIndex() < 24);
    assert(wallet.secondChallengeIndex() >= 0 && wallet.secondChallengeIndex() < 24);
    assert(wallet.firstChallengeIndex() != wallet.secondChallengeIndex());
    assert(!wallet.verifyBackup("wrong", "wrong"));
    wallet.revealForBackup();
    assert(wallet.recoveryVisible());
    assert(wallet.startBackupChallenge());
    assert(!wallet.verifyBackup("wrong", "wrong"));
    assert(!wallet.verifyBackup("wrong", "wrong"));
    assert(!wallet.verifyBackup("wrong", "wrong"));
    assert(!wallet.verifyBackup("wrong", "wrong"));
    assert(!wallet.hasWallet()); // Fifth failed attempt discards wallet.
    assert(wallet.address().isEmpty());
    assert(wallet.generate());
    assert(wallet.hasWallet() && wallet.address() != custody);
    wallet.discard();
    assert(!wallet.hasWallet() && !wallet.backupVerified());
    // A process restart never copies the mnemonic into settings. The owner
    // explicitly re-enters the offline BIP39 phrase to resume one paid FID.
    assert(!wallet.restoreForRegistration(QStringLiteral("abandon abandon abandon")));
    assert(!wallet.hasWallet());
    const QString validPhrase = QStringLiteral(
        "abandon abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon abandon abandon art");
    assert(wallet.restoreForRegistration(validPhrase));
    assert(wallet.backupVerified());
    assert(!wallet.recoveryVisible());
    assert(wallet.address().size() == 42);
    assert(!wallet.restoreForRegistration(validPhrase));
    const QString trustedSiwe = QStringLiteral(
        "genyleap.com wants you to sign in with your Ethereum account:\n")
        + wallet.address()
        + QStringLiteral("\n\nSign in to OpenProof with your Ethereum account.\n\n"
                         "URI: https://genyleap.com/account\n"
                         "Version: 1\nChain ID: 1\nNonce: abcdefgh123456\n"
                         "Issued At: 2026-11-15T00:00:00Z\n"
                         "Expiration Time: 2026-11-15T00:05:00Z");
    auto signedWallet = wallet.signOpenProofWalletChallenge(1ULL << 32, trustedSiwe);
    assert(signedWallet.result().has_value());
    assert(signedWallet.result()->size() == 132);
    const auto invalidFid = wallet.signOpenProofWalletChallenge(123, trustedSiwe);
    assert(!invalidFid.result());
    const auto invalidChain = wallet.signOpenProofWalletChallenge(
        1ULL << 32, QString{trustedSiwe}.replace(QStringLiteral("Chain ID: 1"),
                                                   QStringLiteral("Chain ID: 10")));
    assert(!invalidChain.result()); // No overwrite.
    const auto signedTransfer=wallet.signReservedFarcasterTransfer(
        1234567, 0, static_cast<std::uint64_t>(QDateTime::currentSecsSinceEpoch()+180));
    assert(signedTransfer && signedTransfer->size() == 132);
    assert(meta->indexOfProperty("recoveryWords") == -1);
    wallet.discard();

    NativeRecoveryWordsItem display{};
    display.setWallet(&wallet);
    display.setInkColor(Qt::black);
    display.setWidth(720);
    display.setHeight(280);
    QImage offscreen{720,280,QImage::Format_ARGB32_Premultiplied};
    offscreen.fill(Qt::transparent);
    QPainter painter{&offscreen};
    display.paint(&painter); // Empty wallet never paints any mnemonic.
    painter.end();

    std::cout << "Native Qt wallet lifecycle, backup gate and private properties passed\n";
}
