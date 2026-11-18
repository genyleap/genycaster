/*!
 * @file        ui/app/miniapp/swap_quote.hpp
 * @brief       Validated Base Swap quote and exact-allowance transaction builder.
 *
 * @details
 * Parses the account-authenticated 0x quote broker response, enforces the
 * audited Base AllowanceHolder, short expiry, caller ownership and exact token
 * base-unit semantics. Constructs unsigned wallet transactions; no private
 * keys, automatic approvals or transaction broadcast occur in this component.
 *
 * @author      Kambiz Asadzadeh
 * @since       18 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
#pragma once

#include "swap_intent.hpp"

#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QString>
#include <QVariantMap>

#include <optional>
#include <utility>

namespace farcaster::platform::miniapp {
inline constexpr auto baseAllowanceHolder =
    "0x0000000000001ff3684f28c67538d4d072c22734";

[[nodiscard]] inline bool validAddress(const QString& value)
{
    static const QRegularExpression match{QStringLiteral("^0x[0-9a-fA-F]{40}$")};
    return match.match(value).hasMatch() && value.mid(2) != QString(40, QLatin1Char('0'));
}

[[nodiscard]] inline bool validHexCalldata(const QString& value)
{
    if (!value.startsWith(QStringLiteral("0x")) || value.size() < 10
        || value.size() > 128'000 || (value.size() % 2) != 0) return false;
    static const QRegularExpression match{QStringLiteral("^0x[0-9a-fA-F]+$")};
    return match.match(value).hasMatch();
}

[[nodiscard]] inline bool validUint256OrZero(const QString& value)
{
    return value == QStringLiteral("0") || validUint256(value);
}

// Exact decimal-to-hex conversion without doubles or platform-dependent
// 64-bit truncation; handles all valid unsigned 256-bit amounts.
[[nodiscard]] inline QString decimalToHex256(QString decimal)
{
    if (!validUint256OrZero(decimal)) return {};
    if (decimal == QStringLiteral("0")) return QStringLiteral("0");
    QString output{};
    constexpr auto alphabet = "0123456789abcdef";
    while (!decimal.isEmpty() && decimal != QStringLiteral("0")) {
        QString next{};
        int carry = 0;
        for (const QChar digit : std::as_const(decimal)) {
            const int value = carry * 10 + digit.digitValue();
            const int quotient = value / 16;
            carry = value % 16;
            if (quotient != 0 || !next.isEmpty())
                next.append(QChar(QLatin1Char(static_cast<char>('0' + quotient))));
        }
        output.prepend(QLatin1Char(alphabet[carry]));
        decimal = next.isEmpty() ? QStringLiteral("0") : next;
    }
    return output;
}

[[nodiscard]] inline QString exactApprovalCalldata(const QString& spender,
                                                    const QString& amount)
{
    if (!validAddress(spender) || !validUint256(amount)
        || spender.compare(QString::fromLatin1(baseAllowanceHolder),
                           Qt::CaseInsensitive) != 0) return {};
    const QString hexAmount = decimalToHex256(amount);
    if (hexAmount.isEmpty() || hexAmount.size() > 64) return {};
    return QStringLiteral("0x095ea7b3")
        + spender.mid(2).toLower().rightJustified(64, QLatin1Char('0'))
        + hexAmount.rightJustified(64, QLatin1Char('0'));
}

struct ValidatedSwapQuote final {
    QString taker{};
    QString sellToken{};
    QString buyToken{};
    QString sellAmount{};
    QString buyAmount{};
    QString minBuyAmount{};
    QString totalNetworkFee{};
    QString to{};
    QString data{};
    QString value{};
    QString gas{};
    bool requiresApproval{};
    qint64 expiresAtMs{};

    [[nodiscard]] QVariantMap preview() const
    {
        return {{QStringLiteral("sellAmount"),sellAmount},
                {QStringLiteral("swapTarget"),to},
                {QStringLiteral("buyAmount"),buyAmount},
                {QStringLiteral("minBuyAmount"),minBuyAmount},
                {QStringLiteral("totalNetworkFee"),totalNetworkFee},
                {QStringLiteral("requiresApproval"),requiresApproval},
                {QStringLiteral("expiresAt"),expiresAtMs},
                {QStringLiteral("spender"),QString::fromLatin1(baseAllowanceHolder)}};
    }

    [[nodiscard]] QJsonObject swapTransaction() const
    {
        return {{QStringLiteral("from"),taker},
                {QStringLiteral("chainId"),QStringLiteral("0x2105")},
                {QStringLiteral("to"),to},
                {QStringLiteral("data"),data},
                {QStringLiteral("value"),QStringLiteral("0x")+decimalToHex256(value)},
                {QStringLiteral("gas"),QStringLiteral("0x")+decimalToHex256(gas)}};
    }

    [[nodiscard]] QJsonObject approvalTransaction() const
    {
        return {{QStringLiteral("from"),taker},
                {QStringLiteral("chainId"),QStringLiteral("0x2105")},
                {QStringLiteral("to"),sellToken.mid(QStringLiteral("eip155:8453/erc20:").size())},
                {QStringLiteral("data"),exactApprovalCalldata(
                    QString::fromLatin1(baseAllowanceHolder), sellAmount)},
                {QStringLiteral("value"),QStringLiteral("0x0")}};
    }
};

[[nodiscard]] inline std::optional<ValidatedSwapQuote> validateSwapQuote(
    const QJsonObject& quote, const SwapIntent& intent, const QString& taker,
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch())
{
    const auto field = [&quote](const char* key) {
        return quote.value(QString::fromLatin1(key)).toString();
    };
    if (quote.value(QStringLiteral("chainId")).toInt() != 8453
        || !validAddress(taker)
        || field("taker").compare(taker, Qt::CaseInsensitive) != 0
        || field("sellToken").compare(intent.sellToken, Qt::CaseInsensitive) != 0
        || field("buyToken").compare(intent.buyToken, Qt::CaseInsensitive) != 0
        || !validUint256(field("sellAmount"))
        || field("sellAmount") != intent.sellAmount
        || !validUint256(field("buyAmount"))
        || !validUint256(field("minBuyAmount"))
        || !validUint256OrZero(field("totalNetworkFee"))
        || field("spender").compare(QString::fromLatin1(baseAllowanceHolder),
                                    Qt::CaseInsensitive) != 0
        || !quote.value(QStringLiteral("requiresApproval")).isBool()
        || !quote.value(QStringLiteral("expiresAt")).isDouble()) return std::nullopt;
    const qint64 expiry = static_cast<qint64>(
        quote.value(QStringLiteral("expiresAt")).toDouble());
    if (expiry <= nowMs || expiry > nowMs + 25'000) return std::nullopt;
    const QJsonObject tx = quote.value(QStringLiteral("transaction")).toObject();
    const QString to = tx.value(QStringLiteral("to")).toString();
    const QString data = tx.value(QStringLiteral("data")).toString();
    const QString value = tx.value(QStringLiteral("value")).toString();
    const QString gas = tx.value(QStringLiteral("gas")).toString();
    // 0x transaction.to may be a dynamic Settler. Only approval spender
    // is fixed to AllowanceHolder; both are independently checked.
    if (!validAddress(to)
        || to.compare(taker, Qt::CaseInsensitive) == 0
        || !validHexCalldata(data) || !validUint256OrZero(value)
        || !validUint256(gas)) return std::nullopt;
    if (intent.sellToken.endsWith(QStringLiteral("/native"))) {
        if (value != intent.sellAmount
            || quote.value(QStringLiteral("requiresApproval")).toBool())
            return std::nullopt;
    } else if (value != QStringLiteral("0")) return std::nullopt;
    // Numeric comparisons use padded decimal strings, NEVER double.
    const auto greater = [](const QString& a, const QString& b) {
        return a.size() > b.size() || (a.size() == b.size() && a > b);
    };
    if (greater(field("minBuyAmount"), field("buyAmount")))
        return std::nullopt;
    return ValidatedSwapQuote{
        .taker = taker, .sellToken = intent.sellToken, .buyToken = intent.buyToken,
        .sellAmount = intent.sellAmount, .buyAmount = field("buyAmount"),
        .minBuyAmount = field("minBuyAmount"), .totalNetworkFee = field("totalNetworkFee"),
        .to = to, .data = data, .value = value, .gas = gas,
        .requiresApproval = quote.value(QStringLiteral("requiresApproval")).toBool(),
        .expiresAtMs = expiry,
    };
}
} // namespace farcaster::platform::miniapp
