/*!
 * @file        ui/app/identity/pro_checkout.cpp
 * @brief       Verified, first-party Pro payment wallet workflow.
 *
 * @details
 * User approval is mandatory for EIP-1193 account access, network switching
 * and the ERC-20 transfer. Backend verification is the sole entitlement gate.
 *
 * @author      Kambiz Asadzadeh
 * @since       09 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
#include "pro_checkout.hpp"
#include "connections.hpp"

#include <QDateTime>
#include <QJsonDocument>
#include <QMetaObject>
#include <QRegularExpression>
#include <QVariantList>

#include <algorithm>
#include <utility>

namespace {
constexpr auto GenyToken = "0x2a3d6f8c1fc4acdcf3a75d19b445bae02f03676b";
constexpr auto UsdcToken = "0x833589fcd6edb6e08f4c7c32d4f71b54bda02913";
const QRegularExpression Address{QStringLiteral("^0x[0-9a-fA-F]{40}$")};
const QRegularExpression Hash{QStringLiteral("^0x[0-9a-fA-F]{64}$")};
const QRegularExpression Decimal{QStringLiteral("^[1-9][0-9]{0,75}$")};

// EIP-1193 transaction fields must be uint256-accurate. Qt's number types and
// JavaScript Number cannot represent token base units with 18 decimals.
[[nodiscard]] QString decimalToUint256(const QString& text)
{
    if (!Decimal.match(text).hasMatch()) return {};
    QString current = text;
    QString hex;
    constexpr char digits[] = "0123456789abcdef";
    while (current != QLatin1String("0")) {
        QString quotient;
        int remainder = 0;
        for (const QChar digit : current) {
            const int value = remainder * 10 + digit.digitValue();
            const int next = value / 16;
            remainder = value % 16;
            if (!quotient.isEmpty() || next > 0)
                quotient.append(QLatin1Char(static_cast<char>('0' + next)));
        }
        hex.prepend(QLatin1Char(digits[remainder]));
        if (hex.size() > 64) return {};
        current = quotient.isEmpty() ? QStringLiteral("0") : quotient;
    }
    return hex.rightJustified(64, QLatin1Char('0'));
}
[[nodiscard]] bool sameAddress(const QString& a, const QString& b)
{
    return a.compare(b, Qt::CaseInsensitive) == 0;
}
} // namespace

ProCheckoutController::ProCheckoutController(OpenProofConnectionsController& account,
                                               QObject* parent)
    : QObject(parent), m_account(&account)
{
    connect(&account, &OpenProofConnectionsController::stateChanged, this, [this] {
        if (m_session && (!m_account || m_account->activeAccountFid() != m_fid
            || !m_account->cloudAuthorizationReady()))
            cancel();
    });
}

ProCheckoutController::~ProCheckoutController()
{
    cancel();
}

void ProCheckoutController::setWalletTransport(std::function<bool()> begin,
                                                RpcRequest request,
                                                std::function<void()> end)
{
    if (m_session) cancel();
    m_begin = std::move(begin);
    m_request = std::move(request);
    m_end = std::move(end);
    emit changed();
}

bool ProCheckoutController::validQuote(const QVariantMap& quote) const
{
    if (!m_account || !m_account->activeAccountFid() ||
        !m_account->cloudAuthorizationReady() ||
        quote.isEmpty() || !Address.match(quote.value(QStringLiteral("treasury")).toString()).hasMatch() ||
        !Address.match(quote.value(QStringLiteral("token")).toString()).hasMatch() ||
        quote.value(QStringLiteral("chain_id")).toInt() != 8453 ||
        quote.value(QStringLiteral("id")).toString().size() != 36 ||
        quote.value(QStringLiteral("verified_payers")).toList().isEmpty())
        return false;

    const QString symbol = quote.value(QStringLiteral("asset")).toString();
    const QString token = quote.value(QStringLiteral("token")).toString();
    if ((symbol != QStringLiteral("GENY") || !sameAddress(token, QString::fromLatin1(GenyToken))) &&
        (symbol != QStringLiteral("USDC") || !sameAddress(token, QString::fromLatin1(UsdcToken))))
        return false;
    const auto expiresAt = QDateTime::fromString(
        quote.value(QStringLiteral("expires_at")).toString(), Qt::ISODateWithMs);
    if (!expiresAt.isValid() || expiresAt.toMSecsSinceEpoch() -
        QDateTime::currentMSecsSinceEpoch() < 45'000)
        return false;
    return !decimalToUint256(quote.value(QStringLiteral("amount_base_units")).toString()).isEmpty();
}

void ProCheckoutController::connectWallet()
{
    if (m_busy || m_session) return;
    if (!m_account || !m_account->activeAccountFid()
        || !m_account->cloudAuthorizationReady()) {
        finishError(QStringLiteral("authentication_required"));
        return;
    }
    if (!m_begin || !m_request || !m_end) {
        finishError(QStringLiteral("wallet_unavailable"));
        return;
    }
    // This phase only requests account selection; it never switches networks,
    // grants a subscription or constructs an ERC-20 transfer.
    m_fid = m_account->activeAccountFid();
    m_payer.clear();
    m_quote.clear();
    m_quoteId.clear();
    m_txHash.clear();
    m_error.clear();
    ++m_generation;
    m_busy = true;
    m_phase = QStringLiteral("connecting");
    emit changed();
    if (!m_begin()) {
        finishError(QStringLiteral("wallet_unavailable"));
        return;
    }
    m_session = true;
    request(QStringLiteral("eth_requestAccounts"), {}, [this](QJsonValue accounts, QString error) {
        if (!error.isEmpty() || !accounts.isArray() || accounts.toArray().isEmpty()) {
            finishError(error.isEmpty() ? QStringLiteral("wallet_connect_failed") : error);
            return;
        }
        const QString payer = accounts.toArray().first().toString().toLower();
        if (!Address.match(payer).hasMatch()) {
            finishError(QStringLiteral("wallet_address_invalid"));
            return;
        }
        m_payer = payer;
        m_busy = false;
        m_phase = QStringLiteral("connected");
        emit changed();
        emit walletConnectedSuccessfully();
    });
}

void ProCheckoutController::pay()
{
    if (m_busy || (m_session && !walletConnected())) return;
    m_error.clear();
    m_txHash.clear();
    if (!m_begin || !m_request || !m_end) {
        finishError(QStringLiteral("wallet_unavailable"));
        return;
    }
    m_quote = m_account ? m_account->proPaymentQuote() : QVariantMap{};
    if (!validQuote(m_quote)) {
        finishError(QStringLiteral("invalid_payment_quote"));
        return;
    }
    const bool alreadyConnected = walletConnected();
    if (alreadyConnected && m_fid != m_account->activeAccountFid()) {
        finishError(QStringLiteral("wallet_not_verified_for_account"));
        return;
    }
    m_fid = m_account->activeAccountFid();
    m_quoteId = m_quote.value(QStringLiteral("id")).toString();
    ++m_generation;
    m_busy = true;
    m_phase = alreadyConnected ? QStringLiteral("checking_chain") : QStringLiteral("connecting");
    emit changed();
    if (alreadyConnected) {
        const QVariantList payers = m_quote.value(QStringLiteral("verified_payers")).toList();
        const bool owned = std::any_of(payers.begin(), payers.end(), [this](const QVariant& v) {
            return sameAddress(v.toString(), m_payer);
        });
        if (!owned) {
            finishError(QStringLiteral("wallet_not_verified_for_account"));
            return;
        }
        requestChain();
        return;
    }
    if (!m_begin()) {
        finishError(QStringLiteral("wallet_unavailable"));
        return;
    }
    m_session = true;
    request(QStringLiteral("eth_requestAccounts"), {}, [this](QJsonValue accounts, QString error) {
        if (!error.isEmpty() || !accounts.isArray() || accounts.toArray().isEmpty()) {
            finishError(error.isEmpty() ? QStringLiteral("wallet_connect_failed") : error);
            return;
        }
        const QString payer = accounts.toArray().first().toString().toLower();
        if (!Address.match(payer).hasMatch()) {
            finishError(QStringLiteral("wallet_address_invalid"));
            return;
        }
        const QVariantList payers = m_quote.value(QStringLiteral("verified_payers")).toList();
        const bool owned = std::any_of(payers.begin(), payers.end(), [&payer](const QVariant& v) {
            return sameAddress(v.toString(), payer);
        });
        if (!owned) {
            finishError(QStringLiteral("wallet_not_verified_for_account"));
            return;
        }
        m_payer = payer;
        requestChain();
    });
}

void ProCheckoutController::request(const QString& method, QJsonArray parameters,
                                    RpcComplete callback)
{
    if (!m_request || !m_session || !m_account) {
        finishError(QStringLiteral("wallet_unavailable"));
        return;
    }
    const int generation = m_generation;
    const QPointer<ProCheckoutController> guarded{this};
    m_request(method, std::move(parameters),
        [guarded, generation, callback = std::move(callback)]
            (QJsonValue result, QString error) mutable {
            if (!guarded) return;
            QMetaObject::invokeMethod(guarded, [guarded, generation, callback = std::move(callback),
                                                 result = std::move(result), error = std::move(error)]() mutable {
                if (!guarded || guarded->m_generation != generation ||
                    !guarded->m_session || !guarded->m_account ||
                    guarded->m_account->activeAccountFid() != guarded->m_fid)
                    return;
                callback(std::move(result), std::move(error));
            }, Qt::QueuedConnection);
        });
}

void ProCheckoutController::requestChain()
{
    m_phase = QStringLiteral("checking_chain");
    emit changed();
    request(QStringLiteral("eth_chainId"), {}, [this](QJsonValue chain, QString error) {
        if (!error.isEmpty() || !chain.isString()) {
            finishError(error.isEmpty() ? QStringLiteral("wallet_chain_unavailable") : error);
            return;
        }
        if (chain.toString().compare(QStringLiteral("0x2105"), Qt::CaseInsensitive) != 0) {
            m_phase = QStringLiteral("switching_chain");
            emit changed();
            request(QStringLiteral("wallet_switchEthereumChain"),
                    QJsonArray{QJsonObject{{QStringLiteral("chainId"), QStringLiteral("0x2105")}}},
                    [this](QJsonValue, QString switchError) {
                if (!switchError.isEmpty()) {
                    finishError(QStringLiteral("wallet_switch_base_required"));
                    return;
                }
                // A successful RPC is not proof that the wallet switched.
                request(QStringLiteral("eth_chainId"), {}, [this](QJsonValue actual, QString retryError) {
                    if (!retryError.isEmpty() || !actual.isString() ||
                        actual.toString().compare(QStringLiteral("0x2105"), Qt::CaseInsensitive) != 0) {
                        finishError(QStringLiteral("wallet_wrong_chain"));
                        return;
                    }
                    sendTransfer();
                });
            });
            return;
        }
        sendTransfer();
    });
}

QString ProCheckoutController::checkedTransferData() const
{
    const QString recipient = m_quote.value(QStringLiteral("treasury")).toString().toLower();
    if (!Address.match(recipient).hasMatch()) return {};
    const QString amount = decimalToUint256(
        m_quote.value(QStringLiteral("amount_base_units")).toString());
    if (amount.isEmpty()) return {};
    return QStringLiteral("0xa9059cbb") +
        recipient.mid(2).rightJustified(64, QLatin1Char('0')) + amount;
}

void ProCheckoutController::sendTransfer()
{
    if (!validQuote(m_quote) || m_account->activeAccountFid() != m_fid) {
        finishError(QStringLiteral("invalid_payment_quote"));
        return;
    }
    const QString data = checkedTransferData();
    if (data.size() != 138) {
        finishError(QStringLiteral("invalid_payment_quote"));
        return;
    }
    m_phase = QStringLiteral("wallet_approval");
    emit changed();
    const QJsonObject transaction{
        {QStringLiteral("from"), m_payer},
        {QStringLiteral("to"), m_quote.value(QStringLiteral("token")).toString()},
        {QStringLiteral("value"), QStringLiteral("0x0")},
        {QStringLiteral("data"), data}
    };
    request(QStringLiteral("eth_sendTransaction"), QJsonArray{transaction},
            [this](QJsonValue hash, QString error) {
        if (!error.isEmpty() || !hash.isString() ||
            !Hash.match(hash.toString()).hasMatch()) {
            finishError(error.isEmpty() ? QStringLiteral("wallet_transaction_failed") : error);
            return;
        }
        finishSubmitted(hash.toString().toLower());
    });
}

void ProCheckoutController::finishSubmitted(const QString& hash)
{
    m_busy = false;
    m_txHash = hash;
    m_phase = QStringLiteral("submitted");
    m_error.clear();
    if (m_session && m_end) m_end();
    m_session = false;
    emit changed();
    emit transactionSubmitted(m_quoteId,hash);
}

void ProCheckoutController::finishError(const QString& error)
{
    ++m_generation;
    m_busy = false;
    m_phase = QStringLiteral("error");
    m_error = error.left(250);
    if (m_session && m_end) m_end();
    m_session = false;
    emit changed();
}

void ProCheckoutController::cancel()
{
    ++m_generation;
    const bool hadSession = m_session;
    m_session = false;
    m_busy = false;
    m_phase.clear();
    m_error.clear();
    if (hadSession && m_end) m_end();
    if (hadSession) emit changed();
}
