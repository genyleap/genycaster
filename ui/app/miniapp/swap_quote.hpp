/*!
 * @file        ui/app/miniapp/swap_quote.hpp
 * @brief       Strict Base Uniswap v2 on-chain quote and ABI verification.
 *
 * @details
 * Reconstructs every swap selector, argument, route and amount locally.
 * No commercial aggregator, dynamic transaction target, external approval
 * spender, wallet key or implicit signing permission is accepted.
 *
 * @author      Kambiz Asadzadeh
 * @since       18 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
#pragma once

#include "swap_intent.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>
#include <utility>

namespace farcaster::platform::miniapp {

inline constexpr auto baseSwapRouter = "0x4752ba5dbc23f44d87826276bf6fd6b1c372ad24";
inline constexpr auto baseWrappedEther = "0x4200000000000000000000000000000000000006";

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

// Exact decimal-to-hex conversion; never round via a floating point number.
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

[[nodiscard]] inline QString abiUint(const QString& decimal)
{
    const QString encoded=decimalToHex256(decimal);
    return encoded.isEmpty() || encoded.size() > 64 ? QString{}
        : encoded.rightJustified(64,QLatin1Char('0'));
}
[[nodiscard]] inline QString abiAddress(const QString& address)
{
    return validAddress(address)
        ? address.mid(2).toLower().rightJustified(64,QLatin1Char('0')) : QString{};
}
[[nodiscard]] inline bool isNativeSwapToken(const QString& token)
{
    return token == QStringLiteral("eip155:8453/native");
}
[[nodiscard]] inline QString contractForSwapToken(const QString& token)
{
    if (isNativeSwapToken(token)) return QString::fromLatin1(baseWrappedEther);
    constexpr char prefix[] = "eip155:8453/erc20:";
    if (!token.startsWith(QString::fromLatin1(prefix))) return {};
    const QString address=token.mid(sizeof(prefix)-1);
    return validAddress(address) ? address : QString{};
}

[[nodiscard]] inline QString exactApprovalCalldata(const QString& spender,
                                                    const QString& amount)
{
    if (!validAddress(spender) || !validUint256(amount)
        || spender.compare(QString::fromLatin1(baseSwapRouter),
                           Qt::CaseInsensitive) != 0) return {};
    return QStringLiteral("0x095ea7b3")+abiAddress(spender)+abiUint(amount);
}

[[nodiscard]] inline QString encodeSwapCalldata(
    const SwapIntent& intent,const QString& taker,const QString& minBuy,
    const QStringList& path,const qint64 deadline)
{
    if (!validAddress(taker) || !validUint256(minBuy)
        || !validUint256(intent.sellAmount) || deadline<=0
        || path.size()<2 || path.size()>3) return {};
    const QString first=contractForSwapToken(intent.sellToken);
    const QString last=contractForSwapToken(intent.buyToken);
    if (first.isEmpty() || last.isEmpty()
        || first.compare(last,Qt::CaseInsensitive)==0
        || path.front().compare(first,Qt::CaseInsensitive)!=0
        || path.back().compare(last,Qt::CaseInsensitive)!=0
        || (path.size()==3
            && (path.at(1).compare(QString::fromLatin1(baseWrappedEther),
                                   Qt::CaseInsensitive)!=0
                || first.compare(QString::fromLatin1(baseWrappedEther),
                                 Qt::CaseInsensitive)==0
                || last.compare(QString::fromLatin1(baseWrappedEther),
                                Qt::CaseInsensitive)==0))) return {};
    QString pathWords=abiUint(QString::number(path.size()));
    for (const QString& part:path) {
        const QString address=abiAddress(part);
        if (address.isEmpty()) return {};
        pathWords+=address;
    }
    const QString recipient=abiAddress(taker);
    const QString until=abiUint(QString::number(deadline));
    if (isNativeSwapToken(intent.sellToken)) {
        return QStringLiteral("0x7ff36ab5")
            +abiUint(minBuy)+abiUint(QStringLiteral("128"))
            +recipient+until+pathWords;
    }
    return (isNativeSwapToken(intent.buyToken)
                ?QStringLiteral("0x18cbafe5"):QStringLiteral("0x38ed1739"))
        +abiUint(intent.sellAmount)+abiUint(minBuy)
        +abiUint(QStringLiteral("160"))+recipient+until+pathWords;
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
                {QStringLiteral("spender"),QString::fromLatin1(baseSwapRouter)}};
    }

    [[nodiscard]] QJsonObject swapTransaction() const
    {
        // Let the selected EIP-1193 wallet estimate the execution gas; the
        // RPC quote's gas figure is displayed for guidance, not a gas cap.
        return {{QStringLiteral("from"),taker},
                {QStringLiteral("chainId"),QStringLiteral("0x2105")},
                {QStringLiteral("to"),to},
                {QStringLiteral("data"),data},
                {QStringLiteral("value"),QStringLiteral("0x")+decimalToHex256(value)}};
    }

    [[nodiscard]] QJsonObject approvalTransaction() const
    {
        return {{QStringLiteral("from"),taker},
                {QStringLiteral("chainId"),QStringLiteral("0x2105")},
                {QStringLiteral("to"),contractForSwapToken(sellToken)},
                {QStringLiteral("data"),exactApprovalCalldata(
                    QString::fromLatin1(baseSwapRouter),sellAmount)},
                {QStringLiteral("value"),QStringLiteral("0x0")}};
    }
};

[[nodiscard]] inline std::optional<ValidatedSwapQuote> validateSwapQuote(
    const QJsonObject& quote,const SwapIntent& intent,const QString& taker,
    const qint64 nowMs=QDateTime::currentMSecsSinceEpoch())
{
    const auto field=[&quote](const char* key) {
        return quote.value(QString::fromLatin1(key)).toString();
    };
    if (field("provider")!=QStringLiteral("uniswap-v2-onchain")
        || quote.value(QStringLiteral("chainId")).toInt()!=8453
        || !validAddress(taker)
        || field("taker").compare(taker,Qt::CaseInsensitive)!=0
        || field("sellToken").compare(intent.sellToken,Qt::CaseInsensitive)!=0
        || field("buyToken").compare(intent.buyToken,Qt::CaseInsensitive)!=0
        || !validUint256(field("sellAmount"))
        || field("sellAmount")!=intent.sellAmount
        || !validUint256(field("buyAmount"))
        || !validUint256(field("minBuyAmount"))
        || !validUint256OrZero(field("totalNetworkFee"))
        || field("spender").compare(QString::fromLatin1(baseSwapRouter),
                                    Qt::CaseInsensitive)!=0
        || !quote.value(QStringLiteral("requiresApproval")).isBool()
        || !quote.value(QStringLiteral("expiresAt")).isDouble()
        || !quote.value(QStringLiteral("deadline")).isDouble()) return std::nullopt;

    const qint64 expiry=static_cast<qint64>(
        quote.value(QStringLiteral("expiresAt")).toDouble());
    const qint64 deadline=static_cast<qint64>(
        quote.value(QStringLiteral("deadline")).toDouble());
    if (expiry<=nowMs || expiry>nowMs+25'000
        || deadline<=nowMs/1000 || deadline>nowMs/1000+190) return std::nullopt;
    const auto greater=[](const QString& a,const QString& b) {
        return a.size()>b.size() || (a.size()==b.size() && a>b);
    };
    if (greater(field("minBuyAmount"),field("buyAmount")))
        return std::nullopt;

    const QJsonArray route=quote.value(QStringLiteral("route")).toArray();
    QStringList path;
    for(const QJsonValue& part:route) {
        if (!part.isString()) return std::nullopt;
        path.append(part.toString());
    }
    const QString expected=encodeSwapCalldata(
        intent,taker,field("minBuyAmount"),path,deadline);
    if (expected.isEmpty()) return std::nullopt;
    const QJsonObject tx=quote.value(QStringLiteral("transaction")).toObject();
    const QString to=tx.value(QStringLiteral("to")).toString();
    const QString data=tx.value(QStringLiteral("data")).toString();
    const QString value=tx.value(QStringLiteral("value")).toString();
    const QString gas=tx.value(QStringLiteral("gas")).toString();
    if (to.compare(QString::fromLatin1(baseSwapRouter),Qt::CaseInsensitive)!=0
        || data.compare(expected,Qt::CaseInsensitive)!=0
        || !validHexCalldata(data) || !validUint256OrZero(value)
        || !validUint256(gas)) return std::nullopt;
    if (isNativeSwapToken(intent.sellToken)) {
        if (value!=intent.sellAmount
            || quote.value(QStringLiteral("requiresApproval")).toBool())
            return std::nullopt;
    } else if (value!=QStringLiteral("0")) return std::nullopt;
    return ValidatedSwapQuote{
        .taker=taker,.sellToken=intent.sellToken,.buyToken=intent.buyToken,
        .sellAmount=intent.sellAmount,.buyAmount=field("buyAmount"),
        .minBuyAmount=field("minBuyAmount"),.totalNetworkFee=field("totalNetworkFee"),
        .to=to,.data=data,.value=value,.gas=gas,
        .requiresApproval=quote.value(QStringLiteral("requiresApproval")).toBool(),
        .expiresAtMs=expiry,
    };
}
} // namespace farcaster::platform::miniapp
