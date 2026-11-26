#include "native_wallet.hpp"

#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QGuiApplication>
#include <QPainter>
#include <QtConcurrent>
#include <QTimer>
#include <QFontMetricsF>
#include <QStringList>
#include <openssl/crypto.h>
#include <openssl/rand.h>

#include <array>
#include <atomic>
#include <algorithm>
#include <optional>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

import farcaster.platform.custody_recovery;
import farcaster.protocol.message;
import farcaster.capability.signer;

#include "native_wallet_signer.hpp"

namespace custody = farcaster::platform::custody_recovery;

struct NativeCustodyWalletController::Private {
    std::optional<custody::GeneratedCustodyWallet> secret{};
    bool revealed{false};
    bool verified{false};
    int indexA{-1};
    int indexB{-1};
    unsigned failures{};
    QString error{};
    QByteArray clipboardFingerprint{}; // SHA-256, never the recovery phrase.
    qsizetype clipboardLength{};
    quint64 clipboardRevision{};
};

NativeCustodyWalletController::NativeCustodyWalletController(QObject* parent)
    : QObject{parent}, m_private{std::make_unique<Private>()}
{}

NativeCustodyWalletController::~NativeCustodyWalletController()
{
    clearCopiedRecoveryWords();
}

bool NativeCustodyWalletController::hasWallet() const noexcept
{
    return m_private->secret.has_value();
}
bool NativeCustodyWalletController::recoveryVisible() const noexcept
{
    return hasWallet() && m_private->revealed && !m_private->verified;
}
bool NativeCustodyWalletController::backupVerified() const noexcept
{
    return hasWallet() && m_private->verified;
}
int NativeCustodyWalletController::firstChallengeIndex() const noexcept
{
    return m_private->indexA;
}
int NativeCustodyWalletController::secondChallengeIndex() const noexcept
{
    return m_private->indexB;
}
QString NativeCustodyWalletController::address() const
{
    return hasWallet() ? QString::fromStdString(m_private->secret->address) : QString{};
}
QString NativeCustodyWalletController::error() const
{
    return m_private->error;
}
bool NativeCustodyWalletController::generate()
{
    if (hasWallet()) return false; // Never silently replace an unbacked wallet.
    auto generated = custody::generateCustodyWallet();
    if (!generated) {
        m_private->error = QStringLiteral("Unable to generate secure custody wallet.");
        emit changed();
        return false;
    }
    m_private->secret.emplace(std::move(*generated));
    m_private->verified = false;
    m_private->revealed = true;
    m_private->failures = 0;
    m_private->indexA = -1;
    m_private->indexB = -1;
    m_private->error.clear();
    emit changed();
    return true;
}


bool NativeCustodyWalletController::restoreForRegistration(QString recoveryWords)
{
    const auto erase = [&recoveryWords] {
        if (!recoveryWords.isEmpty())
            recoveryWords.fill(QChar{char16_t{0}});
    };
    if (hasWallet() || recoveryWords.size() > 4095) {
        erase();
        return false;
    }
    QByteArray bytes = recoveryWords.toUtf8();
    erase();
    if (bytes.size() < 32 || bytes.size() > 512) {
        bytes.fill(char{0});
        m_private->error = QStringLiteral("Invalid custody recovery phrase.");
        emit changed();
        return false;
    }
    std::string phrase{bytes.constData(), static_cast<std::size_t>(bytes.size())};
    bytes.fill(char{0});
    const auto address = custody::derivePublicCustodyAddress(phrase);
    if (!address || address->size() != 42) {
        if (!phrase.empty()) OPENSSL_cleanse(phrase.data(), phrase.size());
        m_private->error = QStringLiteral("Invalid BIP39 custody recovery phrase.");
        emit changed();
        return false;
    }
    custody::GeneratedCustodyWallet restored{};
    restored.recoveryWords = std::move(phrase);
    restored.address = *address;
    if (!phrase.empty()) OPENSSL_cleanse(phrase.data(), phrase.size());
    m_private->secret.emplace(std::move(restored));
    // An exact BIP39 recovery establishes offline backup possession, not
    // network ownership. The protocol registry must confirm identity later.
    m_private->verified = true;
    m_private->revealed = false;
    m_private->indexA = -1;
    m_private->indexB = -1;
    m_private->failures = 0;
    m_private->error.clear();
    emit changed();
    return true;
}

bool NativeCustodyWalletController::startBackupChallenge()
{
    if (!hasWallet() || m_private->verified) return false;
    std::array<unsigned char, 2> entropy{};
    if (RAND_priv_bytes(entropy.data(), static_cast<int>(entropy.size())) != 1) {
        m_private->error = QStringLiteral("Unable to generate backup verification challenge.");
        emit changed();
        return false;
    }
    m_private->indexA = static_cast<int>(entropy[0] % 24);
    m_private->indexB = static_cast<int>(entropy[1] % 23);
    if (m_private->indexB >= m_private->indexA) ++m_private->indexB;
    m_private->revealed = false;
    m_private->error.clear();
    emit changed();
    return true;
}

namespace {
std::string normalizedAscii(const QString& text)
{
    const auto bytes = text.trimmed().toLower().toLatin1();
    return std::string{bytes.constData(), static_cast<std::size_t>(bytes.size())};
}
bool wordMatches(const std::string_view words, const int index,
                 const std::string_view submitted)
{
    if (index < 0 || index >= 24 || submitted.size() > 16) return false;
    std::size_t start{};
    for (int i{}; i < index; ++i) {
        const auto delimiter = words.find(' ', start);
        if (delimiter == std::string_view::npos) return false;
        start = delimiter + 1;
    }
    const auto end = words.find(' ', start);
    const auto expected = words.substr(start, end == std::string_view::npos
                                      ? end : end - start);
    return expected.size() == submitted.size()
        && CRYPTO_memcmp(expected.data(), submitted.data(), expected.size()) == 0;
}
}

bool NativeCustodyWalletController::verifyBackup(QString first, QString second)
{
    const auto wipeInputs = [&] { first.fill(QChar{}); second.fill(QChar{}); };
    if (!hasWallet() || m_private->verified
        || m_private->indexA < 0 || m_private->indexB < 0) {
        wipeInputs();
        return false;
    }
    auto a = normalizedAscii(first);
    auto b = normalizedAscii(second);
    struct Scrub final {
        std::string& first;
        std::string& second;
        ~Scrub() {
            if (!first.empty()) OPENSSL_cleanse(first.data(), first.size());
            if (!second.empty()) OPENSSL_cleanse(second.data(), second.size());
        }
    } scrub{a, b};
    const bool matches = wordMatches(m_private->secret->recoveryWords,
                                    m_private->indexA, a)
        && wordMatches(m_private->secret->recoveryWords, m_private->indexB, b);
    wipeInputs();
    if (!matches) {
        if (++m_private->failures >= 5) {
            discard(); // Fail closed; never expose reusable backup secret.
            return false;
        }
        m_private->error = QStringLiteral(
            "Backup words did not match. Check the saved phrase and retry.");
        emit changed();
        return false;
    }
    m_private->verified = true;
    m_private->revealed = false;
    m_private->indexA = -1;
    m_private->indexB = -1;
    m_private->error.clear();
    emit changed();
    return true;
}

void NativeCustodyWalletController::clearCopiedRecoveryWords()
{
    ++m_private->clipboardRevision; // Cancel any earlier delayed clearance.
    if (m_private->clipboardFingerprint.isEmpty()) return;
    auto expected = std::exchange(m_private->clipboardFingerprint, QByteArray{});
    const auto length = std::exchange(m_private->clipboardLength, qsizetype{});

    // Clipboard history and third-party managers are beyond our control.
    // Never erase a newer, unrelated clipboard value owned by the user.
    if (QGuiApplication::instance()) {
        auto* clipboard = QGuiApplication::clipboard();
        if (!clipboard) {
            expected.fill(char{0});
            return;
        }
        auto current = clipboard->text();
        if (current.size() == length) {
            auto bytes = current.toLatin1();
            const auto actual = QCryptographicHash::hash(
                QByteArrayView{bytes}, QCryptographicHash::Sha256);
            bytes.fill(char{0});
            current.fill(QChar{});
            if (actual == expected) clipboard->clear();
        } else {
            current.fill(QChar{});
        }
    }
    expected.fill(char{0});
}

bool NativeCustodyWalletController::copyRecoveryWords()
{
    // A user gesture in the backup ceremony is required. This is never
    // called by automatic onboarding, recovery, network requests or signing.
    if (!recoveryVisible() || !m_private->secret || !QGuiApplication::instance())
        return false;
    auto* clipboard = QGuiApplication::clipboard();
    if (!clipboard) return false;

    const auto& words = m_private->secret->recoveryWords;
    if (words.empty() || words.size() > 512) return false;
    m_private->clipboardFingerprint = QCryptographicHash::hash(
        QByteArrayView{words.data(), static_cast<qsizetype>(words.size())},
        QCryptographicHash::Sha256);
    m_private->clipboardLength = static_cast<qsizetype>(words.size());
    const auto revision = ++m_private->clipboardRevision;

    // Only the native Qt clipboard sees the phrase. Never expose the phrase
    // as a QString property, signal argument, URL, log or QML return value.
    auto text = QString::fromLatin1(words.data(), static_cast<qsizetype>(words.size()));
    clipboard->setText(text);
    text.fill(QChar{});
    QTimer::singleShot(60'000, this, [this, revision] {
        if (revision == m_private->clipboardRevision)
            clearCopiedRecoveryWords();
    });
    return true;
}

void NativeCustodyWalletController::revealForBackup()
{
    if (!hasWallet() || m_private->verified) return;
    m_private->indexA = -1;
    m_private->indexB = -1;
    m_private->revealed = true;
    m_private->error.clear();
    emit changed();
}

std::optional<QByteArray> NativeCustodyWalletController::sealNativeOnboard(
    const std::uint64_t anchorHeight, const QString& anchorHash,
    const QString& nonceHex, const std::uint32_t difficultyBits) const
{
    if (!backupVerified() || anchorHeight == 0 || difficultyBits == 0
        || difficultyBits > 256)
        return std::nullopt;
    auto signedProof = custody::signNativeOnboard(
        m_private->secret->recoveryWords, anchorHeight,
        anchorHash.toStdString(), nonceHex.toStdString(), difficultyBits);
    if (!signedProof || signedProof->address != m_private->secret->address)
        return std::nullopt;

    constexpr qint64 FarcasterEpoch = 1609459200LL;
    const qint64 timestamp = QDateTime::currentSecsSinceEpoch() - FarcasterEpoch;
    if (timestamp < 1 || timestamp > std::numeric_limits<std::uint32_t>::max())
        return std::nullopt;

    const auto encoded = farcaster::protocol::message::encodeNativeOnboard({
        .custodyAddress = std::move(signedProof->address),
        .custodySignature = std::move(signedProof->signature),
        .anchorHeight = anchorHeight,
        .anchorHash = anchorHash.toStdString(),
        .powNonce = nonceHex.toStdString(),
        .difficultyBits = difficultyBits,
        .timestamp = static_cast<std::uint32_t>(timestamp),
        .network = farcaster::protocol::message::Network::Mainnet
    });
    if (!encoded || encoded->empty() || encoded->size() > 8192)
        return std::nullopt;
    return QByteArray{encoded->data(), static_cast<qsizetype>(encoded->size())};
}

std::optional<QString> NativeCustodyWalletController::signReservedFarcasterTransfer(
    const std::uint64_t fid, const std::uint64_t onchainNonce,
    const std::uint64_t deadlineUnixSeconds) const
{
    if (!backupVerified() || !m_private->secret || fid == 0)
        return std::nullopt;
    // This uses the separate, chain-10 IdRegistry EIP-712 domain, not
    // HyperSnap native onboarding. No private key or mnemonic crosses the
    // native-wallet boundary or enters Qt/QML properties.
    auto result = custody::signFarcasterIdTransfer(
        m_private->secret->recoveryWords, fid, onchainNonce,
        deadlineUnixSeconds);
    if (!result || result->custodyAddress != m_private->secret->address)
        return std::nullopt;
    return QString::fromStdString(result->signature);
}

QFuture<std::optional<QString>>
NativeCustodyWalletController::signOpenProofFarcasterChallenge(
    const std::uint64_t confirmedFid, const QString& challenge) const
{
    if (!backupVerified() || !m_private->secret
        || confirmedFid == 0U
        || confirmedFid > 9'007'199'254'740'991ULL
        || challenge.size() < 80 || challenge.size() > 8192) {
        return QtConcurrent::run([] { return std::optional<QString>{}; });
    }
    // A sensitive copy is confined to the C++ worker, then zeroized on the
    // final reference release. Neither QML nor HTTP receives the words.
    // The BIP39 KDF and secp256k1 signing never block the Qt render thread.
    auto erase = [](QByteArray* bytes) {
        bytes->fill(char{0});
        delete bytes;
    };
    const auto& words = m_private->secret->recoveryWords;
    auto privateWords = std::shared_ptr<QByteArray>(
        new QByteArray{words.data(), static_cast<qsizetype>(words.size())},
        erase);
    const auto expectedAddress = m_private->secret->address;
    return QtConcurrent::run(
        [privateWords = std::move(privateWords), expectedAddress,
         confirmedFid, challenge]() -> std::optional<QString> {
            const auto proof = custody::signOneTimeIdentityProof(
                std::string_view{privateWords->constData(),
                                 static_cast<std::size_t>(privateWords->size())},
                confirmedFid, challenge.toStdString());
            if (!proof || proof->address != expectedAddress)
                return std::nullopt;
            return QString::fromStdString(proof->signature);
        });
}

QFuture<std::optional<QString>>
NativeCustodyWalletController::signOpenProofWalletChallenge(
    const std::uint64_t confirmedNativeFid, const QString& challenge) const
{
    if (!backupVerified() || !m_private->secret
        || confirmedNativeFid < (1ULL << 32)
        || challenge.size() < 80 || challenge.size() > 8192) {
        return QtConcurrent::run([] { return std::optional<QString>{}; });
    }
    auto erase = [](QByteArray* bytes) {
        bytes->fill(char{0});
        delete bytes;
    };
    const auto& words = m_private->secret->recoveryWords;
    auto privateWords = std::shared_ptr<QByteArray>(
        new QByteArray{words.data(), static_cast<qsizetype>(words.size())},
        erase);
    const auto address = m_private->secret->address;
    return QtConcurrent::run(
        [privateWords = std::move(privateWords), address,
         challenge]() -> std::optional<QString> {
            const auto proof = custody::signOpenProofWalletChallenge(
                std::string_view{privateWords->constData(),
                                 static_cast<std::size_t>(privateWords->size())},
                address, challenge.toStdString());
            if (!proof || proof->address != address) return std::nullopt;
            return QString::fromStdString(proof->signature);
        });
}

void NativeCustodyWalletController::discard()
{
    clearCopiedRecoveryWords();
    m_private->revealed = false;
    m_private->verified = false;
    m_private->indexA = -1;
    m_private->indexB = -1;
    m_private->failures = 0;
    m_private->secret.reset(); // GeneratedCustodyWallet zeroizes on destruction.
    m_private->error.clear();
    emit changed();
}

NativeCustodySignerAccess::CloudEnrollmentPermit
NativeCustodySignerAccess::forCloudEnrollment(const NativeCustodyWalletController& wallet,
    const std::uint64_t fid, const QString& custodyAddress)
{
    if (fid < (1ULL<<32) || !wallet.backupVerified() ||
        wallet.address().toLower()!=custodyAddress.toLower()) return {};
    const auto& words=wallet.m_private->secret->recoveryWords;
    auto secret=std::shared_ptr<QByteArray>(
        new QByteArray{words.data(),static_cast<qsizetype>(words.size())},
        [](QByteArray* value){value->fill(char{0});delete value;});
    auto consumed=std::make_shared<std::atomic_bool>(false);
    return [secret=std::move(secret),consumed,fid,address=custodyAddress.toLower().toStdString()](
        const QString& key,const QString& nonce)->std::optional<QString> {
        if(consumed->exchange(true))return std::nullopt;
        auto proof=custody::signCloudDeviceEnrollment(
            {secret->constData(),static_cast<std::size_t>(secret->size())},fid,
            address,key.toStdString(),nonce.toStdString());
        secret->fill(char{0});
        if(!proof)return std::nullopt;
        return QString::fromStdString(proof->signature);
    };
}

std::shared_ptr<farcaster::capability::signer::ICustodyAuthorizer>
NativeCustodySignerAccess::forConfirmedNativeFid(
    const NativeCustodyWalletController& wallet,
    const std::uint64_t fid, const QString& custodyAddress)
{
    if (fid < (1ULL << 32) || !wallet.backupVerified()
        || custodyAddress.toLower() != wallet.address().toLower())
        return {};
    // The phrase is accessed only inside this trusted native C++ TU.
    // It is never copied to QML or to a network request.
    // The resulting signer authorizer is single-use and zeroizing.
    auto proof = custody::makeOneShotRecoveryAuthorizer(
        wallet.m_private->secret->recoveryWords,
        wallet.m_private->secret->address, fid);
    return proof ? std::move(*proof) : nullptr;
}

std::shared_ptr<farcaster::capability::signer::ICustodyAuthorizer>
NativeCustodySignerAccess::forConfirmedFarcasterFid(
    const NativeCustodyWalletController& wallet,
    const std::uint64_t fid, const QString& custodyAddress)
{
    if (fid == 0 || fid > 9'007'199'254'740'991ULL
        || !wallet.backupVerified()
        || custodyAddress.toLower() != wallet.address().toLower())
        return {};
    // Farcaster authorization is bound to a separately verified Optimism
    // IdRegistry custody record, never to HyperSnap shard-0 identity.
    auto proof = custody::makeOneShotRecoveryAuthorizer(
        wallet.m_private->secret->recoveryWords,
        wallet.m_private->secret->address, fid);
    return proof ? std::move(*proof) : nullptr;
}

NativeRecoveryWordsItem::NativeRecoveryWordsItem(QQuickItem* parent)
    : QQuickPaintedItem{parent}
{
    setAntialiasing(true);
    setOpaquePainting(false);
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
        m_connection = QObject::connect(m_wallet, &NativeCustodyWalletController::changed,
                                         this, [this] { update(); });
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
void NativeRecoveryWordsItem::paint(QPainter* painter)
{
    if (!m_wallet || !m_wallet->recoveryVisible()) return;
    const auto& phrase = m_wallet->m_private->secret->recoveryWords;
    const int columns = width() < 470 ? 2 : 4;
    const int rows = 24 / columns;
    const qreal cellWidth = width() / columns;
    const qreal cellHeight = height() / rows;
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->setPen(m_inkColor);
    auto font = painter->font();
    font.setPixelSize(15);
    painter->setFont(font);
    std::size_t start{};
    for (int i{}; i < 24; ++i) {
        const auto end = phrase.find(' ', start);
        if (end == std::string::npos && i != 23) return;
        const auto token = std::string_view{phrase}.substr(
            start, end == std::string::npos ? end : end - start);
        QString word = QString::fromLatin1(token.data(),
                                          static_cast<qsizetype>(token.size()));
        QRectF cell{(i % columns) * cellWidth + 8,
                    (i / columns) * cellHeight + 2,
                    cellWidth - 16, cellHeight - 4};
        auto ordinal = QString::number(i + 1) + QStringLiteral(".");
        painter->drawText(QRectF{cell.x(), cell.y(), 32, cell.height()},
                          Qt::AlignLeft | Qt::AlignVCenter, ordinal);
        painter->drawText(QRectF{cell.x() + 32, cell.y(),
                                 std::max(0.0, cell.width() - 32), cell.height()},
                          Qt::AlignLeft | Qt::AlignVCenter, word);
        word.fill(QChar{});
        start = end + 1;
    }
}
