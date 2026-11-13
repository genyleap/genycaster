#include "native_wallet.hpp"

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
