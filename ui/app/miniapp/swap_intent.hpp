/*!
 * @file        ui/app/miniapp/swap_intent.hpp
 * @brief       Strict, side-effect-free Farcaster swapToken validation.
 *
 * @details
 * Parses untrusted CAIP-19 token prefill and exact uint256 token
 * base-unit amounts. A Mini App cannot supply execution authority,
 *              an approval spender, transaction data, or a quote through here.
 * @author      Kambiz Asadzadeh
 * @since       18 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QString>
#include <QVariantMap>

#include <optional>

namespace farcaster::platform::miniapp {

struct SwapIntent final {
    QString sellToken{};
    QString buyToken{};
    QString sellAmount{};
    QString chain{QStringLiteral("eip155:8453")};

    [[nodiscard]] QVariantMap toVariantMap() const {
        return {
            {QStringLiteral("sellToken"), sellToken},
            {QStringLiteral("buyToken"), buyToken},
            {QStringLiteral("sellAmount"), sellAmount},
            {QStringLiteral("chain"), chain},
        };
    }
};

// The first execution adapter is Base-only. Accepting CAIP-19 identifiers on
// arbitrary chains without a matching quote+execution engine is unsafe.
[[nodiscard]] inline bool validBaseAsset(const QString& value) {
    if (value == QStringLiteral("eip155:8453/native")) return true;
    static const QRegularExpression erc20{
        QStringLiteral("^eip155:8453/erc20:0x[0-9a-fA-F]{40}$")};
    return erc20.match(value).hasMatch()
        && value.right(40) != QString(40, QLatin1Char('0'));
}

[[nodiscard]] inline bool validUint256(const QString& value) {
    static const QRegularExpression digits{QStringLiteral("^[1-9][0-9]{0,77}$")};
    if (!digits.match(value).hasMatch()) return false;
    // Decimal maximum of unsigned 256-bit integer; never use double or JS
    // Number for token base units (both lose precision above 2^53).
    static const QString maximum{
        QStringLiteral("115792089237316195423570985008687907853269984665640564039457584007913129639935")};
    return value.size() < maximum.size()
        || (value.size() == maximum.size() && value <= maximum);
}

[[nodiscard]] inline std::optional<SwapIntent> parseSwapIntent(
    const QJsonValue& argument, QString& rejection)
{
    rejection.clear();
    if (!argument.isUndefined() && !argument.isNull() && !argument.isObject()) {
        rejection = QStringLiteral("Invalid swap options");
        return std::nullopt;
    }
    const QJsonObject input = argument.toObject();
    for (auto field = input.begin(); field != input.end(); ++field) {
        if (field.key() != QStringLiteral("sellToken")
            && field.key() != QStringLiteral("buyToken")
            && field.key() != QStringLiteral("sellAmount")) {
            rejection = QStringLiteral("Unsupported swap option");
            return std::nullopt;
        }
        if (!field.value().isString() || field.value().toString().size() > 128) {
            rejection = QStringLiteral("Invalid swap option type");
            return std::nullopt;
        }
    }
    const SwapIntent intent{
        .sellToken = input.value(QStringLiteral("sellToken")).toString(),
        .buyToken = input.value(QStringLiteral("buyToken")).toString(),
        .sellAmount = input.value(QStringLiteral("sellAmount")).toString(),
    };
    if ((!intent.sellToken.isEmpty() && !validBaseAsset(intent.sellToken))
        || (!intent.buyToken.isEmpty() && !validBaseAsset(intent.buyToken))) {
        rejection = QStringLiteral("Only Base CAIP-19 tokens are supported");
        return std::nullopt;
    }
    if (!intent.sellToken.isEmpty() && !intent.buyToken.isEmpty()
        && intent.sellToken.compare(intent.buyToken, Qt::CaseInsensitive) == 0) {
        rejection = QStringLiteral("Cannot swap a token for itself");
        return std::nullopt;
    }
    if (!intent.sellAmount.isEmpty()
        && (intent.sellToken.isEmpty() || !validUint256(intent.sellAmount))) {
        rejection = QStringLiteral("Invalid sell amount in token base units");
        return std::nullopt;
    }
    return intent;
}

} // namespace farcaster::platform::miniapp
