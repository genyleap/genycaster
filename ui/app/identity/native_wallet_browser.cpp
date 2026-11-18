/*!
 * @file        ui/app/identity/native_wallet_browser.cpp
 * @brief       Browser-only, fail-closed native custody compatibility surface.
 *
 * The Qt/WebAssembly UI exposes the same QML type names as desktop, but no OS
 * secret vault or native custody signer exists in the browser sandbox. It is
 * forbidden to generate or accept mnemonic material, or silently substitute
 * browser-persistent storage as an authority. Browser Mini App EIP-1193 wallets
 * are a separate, origin-bound JS SDK path.
 *
 * This is not an implementation of protocol signing or release feature parity.
 * The all-platform Release gate must remain blocked until an independently
 * audited browser custody + account/chain/consent model is implemented.
 */
#include "native_wallet.hpp"

#include <QPromise>
#include <QChar>
#include <QString>
#include <memory>
#include <optional>

import farcaster.capability.signer;
#include "native_wallet_signer.hpp"

namespace {
[[nodiscard]] QFuture<std::optional<QString>> unavailableSignature()
{
    // Synchronous, completed failure: no browser workers, no sensitive JS,
    // no implicit signer fallback or request that could outlive the UI.
    QPromise<std::optional<QString>> promise;
    promise.start();
    promise.addResult(std::optional<QString>{});
    promise.finish();
    return promise.future();
}
}

struct NativeCustodyWalletController::Private {
    QString error{};
};

NativeCustodyWalletController::NativeCustodyWalletController(QObject* parent)
    : QObject{parent}, m_private{std::make_unique<Private>()}
{}

NativeCustodyWalletController::~NativeCustodyWalletController() = default;

bool NativeCustodyWalletController::hasWallet() const noexcept { return false; }
bool NativeCustodyWalletController::recoveryVisible() const noexcept { return false; }
bool NativeCustodyWalletController::backupVerified() const noexcept { return false; }
int NativeCustodyWalletController::firstChallengeIndex() const noexcept { return -1; }
int NativeCustodyWalletController::secondChallengeIndex() const noexcept { return -1; }
QString NativeCustodyWalletController::address() const { return {}; }
QString NativeCustodyWalletController::error() const { return m_private->error; }

bool NativeCustodyWalletController::generate()
{
    // Localized failure for the missing secure-signer boundary. Do not claim
    // that a wallet exists or that a browser-only OpenProof link was verified.
    m_private->error = qtTrId(
        "genycaster.runtime.the_secure_signer_wallet_or_snapchain_endpoint_is_currently_");
    emit changed();
    return false;
}

bool NativeCustodyWalletController::restoreForRegistration(QString recoveryWords)
{
    // Never retain a phrase supplied by an obsolete caller or a forged QML
    // invocation. Keep the native recovery path entirely outside the browser.
    recoveryWords.fill(QChar{});
    return generate();
}

bool NativeCustodyWalletController::startBackupChallenge() { return false; }

bool NativeCustodyWalletController::verifyBackup(QString first, QString second)
{
    first.fill(QChar{});
    second.fill(QChar{});
    return false;
}

void NativeCustodyWalletController::revealForBackup() {}
bool NativeCustodyWalletController::copyRecoveryWords() { return false; }

void NativeCustodyWalletController::clearCopiedRecoveryWords() {}

void NativeCustodyWalletController::discard()
{
    if (m_private->error.isEmpty()) return;
    m_private->error.clear();
    emit changed();
}

std::optional<QByteArray> NativeCustodyWalletController::sealNativeOnboard(
    std::uint64_t, const QString&, const QString&, std::uint32_t) const
{
    return std::nullopt;
}

std::optional<QString> NativeCustodyWalletController::signReservedFarcasterTransfer(
    std::uint64_t, std::uint64_t, std::uint64_t) const
{
    return std::nullopt;
}

QFuture<std::optional<QString>>
NativeCustodyWalletController::signOpenProofFarcasterChallenge(
    std::uint64_t, const QString&) const
{
    return unavailableSignature();
}

QFuture<std::optional<QString>>
NativeCustodyWalletController::signOpenProofWalletChallenge(
    std::uint64_t, const QString&) const
{
    return unavailableSignature();
}

std::shared_ptr<farcaster::capability::signer::ICustodyAuthorizer>
NativeCustodySignerAccess::forConfirmedNativeFid(
    const NativeCustodyWalletController&, std::uint64_t, const QString&)
{
    return {};
}

std::shared_ptr<farcaster::capability::signer::ICustodyAuthorizer>
NativeCustodySignerAccess::forConfirmedFarcasterFid(
    const NativeCustodyWalletController&, std::uint64_t, const QString&)
{
    return {};
}

NativeRecoveryWordsItem::NativeRecoveryWordsItem(QQuickItem* parent)
    : QQuickPaintedItem{parent}
{
    setOpaquePainting(false);
}

void NativeRecoveryWordsItem::paint(QPainter*)
{
    // No native mnemonic can exist or be painted on WebAssembly.
}

NativeCustodyWalletController* NativeRecoveryWordsItem::wallet() const noexcept
{
    return m_wallet.data();
}

void NativeRecoveryWordsItem::setWallet(NativeCustodyWalletController* wallet)
{
    if (m_wallet == wallet) return;
    QObject::disconnect(m_connection);
    m_wallet = wallet;
    if (m_wallet)
        m_connection = QObject::connect(m_wallet,
            &NativeCustodyWalletController::changed, this, [this] { update(); });
    update();
    emit walletChanged();
}

void NativeRecoveryWordsItem::setInkColor(QColor color)
{
    if (m_inkColor == color) return;
    m_inkColor = color;
    update();
    emit inkColorChanged();
}
