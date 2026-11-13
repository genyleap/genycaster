#pragma once

#include <QObject>
#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QString>
#include <memory>

class QPainter;
class NativeRecoveryWordsItem;

/**
 * Single-use, in-memory native custody wallet for the initial onboarding flow.
 * QML receives only an address, challenge positions and boolean state; never
 * receives mnemonic, seed, or private key through a property, signal or return.
 */
class NativeCustodyWalletController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasWallet READ hasWallet NOTIFY changed)
    Q_PROPERTY(bool recoveryVisible READ recoveryVisible NOTIFY changed)
    Q_PROPERTY(bool backupVerified READ backupVerified NOTIFY changed)
    Q_PROPERTY(int firstChallengeIndex READ firstChallengeIndex NOTIFY changed)
    Q_PROPERTY(int secondChallengeIndex READ secondChallengeIndex NOTIFY changed)
    Q_PROPERTY(QString address READ address NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit NativeCustodyWalletController(QObject* parent = nullptr);
    ~NativeCustodyWalletController() override;
    [[nodiscard]] bool hasWallet() const noexcept;
    [[nodiscard]] bool recoveryVisible() const noexcept;
    [[nodiscard]] bool backupVerified() const noexcept;
    [[nodiscard]] int firstChallengeIndex() const noexcept;
    [[nodiscard]] int secondChallengeIndex() const noexcept;
    [[nodiscard]] QString address() const;
    [[nodiscard]] QString error() const;

    Q_INVOKABLE bool generate();
    Q_INVOKABLE bool startBackupChallenge();
    Q_INVOKABLE bool verifyBackup(QString first, QString second);
    Q_INVOKABLE void revealForBackup();
    Q_INVOKABLE void discard();
signals:
    void changed();

private:
    struct Private;
    std::unique_ptr<Private> m_private;
    friend class NativeRecoveryWordsItem;
};

/**
 * Native-only rendering surface for the mnemonic: no QML Text/text property,
 * clipboard or serializable data model exists for the recovery phrase.
 * The mnemonic is visible in pixels only while the backup ceremony permits it.
 */
class NativeRecoveryWordsItem final : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(NativeCustodyWalletController* wallet READ wallet WRITE setWallet
               NOTIFY walletChanged)
    Q_PROPERTY(QColor inkColor READ inkColor WRITE setInkColor NOTIFY inkColorChanged)
public:
    explicit NativeRecoveryWordsItem(QQuickItem* parent = nullptr);
    void paint(QPainter* painter) override;
    [[nodiscard]] NativeCustodyWalletController* wallet() const noexcept;
    void setWallet(NativeCustodyWalletController* wallet);
    [[nodiscard]] QColor inkColor() const { return m_inkColor; }
    void setInkColor(QColor color);
signals:
    void walletChanged();
    void inkColorChanged();
private:
    QPointer<NativeCustodyWalletController> m_wallet;
    QColor m_inkColor{Qt::black};
    QMetaObject::Connection m_connection;
};
