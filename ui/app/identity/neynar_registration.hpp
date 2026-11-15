/*!
 * @file        ui/app/identity/neynar_registration.hpp
 * @brief       Farcaster registration transport contracts for Neynar.
 *
 * @details
 * Defines the paid reservation and chain-confirmation transport boundary.
 * Keeps voucher authorization distinct from on-chain custody ownership.
 *
 * @author      Genyleap Engineering
 * @since       15 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringView>

#include <cstdint>
#include <optional>

namespace genycaster::identity::neynar {

/**
 * One signed, previously reserved Farcaster FID. This is a PUBLIC request
 * description sent only to the authenticated first-party registration backend.
 * It is never a Neynar API session, wallet ID or custody key.
 *
 * The backend MUST independently check reservation owner, 10-minute TTL,
 * Optimism IdRegistry nonce, signature recovery, username availability,
 * and registration confirmation before forwarding to Neynar.
 */
struct ReservedFidTransfer final {
    std::uint64_t fid{};
    std::uint64_t reservedAtUnixSeconds{};
    std::uint64_t deadlineUnixSeconds{};
    QString custodyAddress{};
    QString eip712Signature{};
    QString requestedFname{};
};

/**
 * Neynar legacy reserved-FID transfer payload:
 * POST /v2/farcaster/user/ (from a server holding x-api-key/x-wallet-id).
 * No network I/O or secrets are available in this adapter.
 * The modern /v2/farcaster/user/register/ has a different registration
 * signature and MUST NOT be supplied with this legacy Transfer proof.
 */
[[nodiscard]] inline std::optional<QJsonObject> buildReservedFidTransferBody(
    const ReservedFidTransfer& request, const std::uint64_t nowUnixSeconds)
{
    if (request.fid == 0 || request.fid > 9'007'199'254'740'991ULL
        || request.reservedAtUnixSeconds > nowUnixSeconds
        || request.reservedAtUnixSeconds + 600 < nowUnixSeconds
        || request.deadlineUnixSeconds <= nowUnixSeconds
        || request.deadlineUnixSeconds > nowUnixSeconds + 3600
        || request.custodyAddress.size() != 42
        || request.eip712Signature.size() != 132)
        return std::nullopt;

    auto isHex = [](QStringView value) {
        if (value.size() < 2 || value.left(2) != QStringView{u"0x"})
            return false;
        for (const QChar c : value.sliced(2)) {
            const ushort u = c.unicode();
            if (!((u >= '0' && u <= '9') || (u >= 'a' && u <= 'f')
                  || (u >= 'A' && u <= 'F'))) return false;
        }
        return true;
    };
    if (!isHex(request.custodyAddress) || !isHex(request.eip712Signature))
        return std::nullopt;
    const auto rec = request.eip712Signature.right(2).toLower();
    if (rec != QLatin1String("1b") && rec != QLatin1String("1c"))
        return std::nullopt;

    if (request.requestedFname.isEmpty()
        || request.requestedFname.size() > 16)
        return std::nullopt;
    for (const QChar ch : request.requestedFname) {
        const auto c = ch.unicode();
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
              || c == '-')) return std::nullopt;
    }

    return QJsonObject{
        {QStringLiteral("fid"), static_cast<double>(request.fid)},
        {QStringLiteral("deadline"),
            static_cast<double>(request.deadlineUnixSeconds)},
        {QStringLiteral("requested_user_custody_address"),
            request.custodyAddress.toLower()},
        {QStringLiteral("signature"), request.eip712Signature},
        {QStringLiteral("fname"), request.requestedFname}
    };
}

} // namespace genycaster::identity::neynar
