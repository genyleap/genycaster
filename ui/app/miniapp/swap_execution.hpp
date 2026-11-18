/*!
 * @file        ui/app/miniapp/swap_execution.hpp
 * @brief       Ethereum receipt and transaction hash validation for Base swaps.
 *
 * @details
 * Rejects mismatched or unconfirmed receipts, failed status and malformed
 * hashes before the host reports a successful Farcaster swapToken action.
 * Read-only validation never signs, approves or transfers tokens.
 *
 * @author      Kambiz Asadzadeh
 * @since       18 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
#pragma once

#include "swap_quote.hpp"
#include <QJsonObject>
#include <QJsonValue>

namespace farcaster::platform::miniapp {

[[nodiscard]] inline bool validTransactionHash(const QString& value)
{
    static const QRegularExpression re{QStringLiteral("^0x[0-9a-fA-F]{64}$")};
    return re.match(value).hasMatch();
}

enum class ReceiptStatus { Pending, Success, Reverted, Invalid };

[[nodiscard]] inline ReceiptStatus validateReceipt(
    const QJsonValue& raw, const QString& expectedHash,
    const QString& sender, const QString& recipient)
{
    if (!validTransactionHash(expectedHash) || !validAddress(sender)
        || !validAddress(recipient)) return ReceiptStatus::Invalid;
    if (raw.isNull() || raw.isUndefined()) return ReceiptStatus::Pending;
    if (!raw.isObject()) return ReceiptStatus::Invalid;
    const QJsonObject receipt=raw.toObject();
    const auto value=[&receipt](const char* name) {
        return receipt.value(QString::fromLatin1(name)).toString();
    };
    if (value("transactionHash").compare(expectedHash,Qt::CaseInsensitive)!=0
        || value("from").compare(sender,Qt::CaseInsensitive)!=0
        || value("to").compare(recipient,Qt::CaseInsensitive)!=0
        || !validTransactionHash(value("blockHash"))) return ReceiptStatus::Invalid;
    const QString block=value("blockNumber");
    static const QRegularExpression hex{QStringLiteral("^0x[0-9a-fA-F]{1,16}$")};
    if (!hex.match(block).hasMatch() || block==QStringLiteral("0x0"))
        return ReceiptStatus::Invalid;
    if (value("status")==QStringLiteral("0x1")) return ReceiptStatus::Success;
    if (value("status")==QStringLiteral("0x0")) return ReceiptStatus::Reverted;
    return ReceiptStatus::Invalid;
}

} // namespace farcaster::platform::miniapp
