/*!
 * @file        ui/app/identity/account_session_principal.hpp
 * @brief       Protocol-scoped identity verification for account cookies.
 *
 * @details
 * An FID alone is not a globally unique account principal. Accept the
 * exact issuer + subject tuple issued by the trusted account service;
 * never turn a protocol-only HyperSnap signer into an OpenProof linkage.
 *
 * @author      Kambiz Asadzadeh
 * @since       18 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
#pragma once

#include <QJsonObject>
#include <QString>
#include <QUrl>
#include <QtTypes>
#include <cstdint>

namespace genycaster::identity {

// HyperSnap's native FID namespace starts at the first value outside uint32.
// A native issuer must never borrow an unrelated lower Farcaster FID.
inline constexpr std::uint64_t firstNativeHyperSnapFid =
    std::uint64_t{1} << 32;

[[nodiscard]] inline bool matchesAccountSessionPrincipal(
    const QJsonObject& data, const qulonglong fid, const QUrl& openProofIssuer)
{
    const QString canonicalFid = QString::number(fid);
    if (fid == 0 || data.value(QStringLiteral("fid")).toString() != canonicalFid)
        return false;
    const QString issuer = data.value(QStringLiteral("issuer")).toString();
    const QString subject = data.value(QStringLiteral("subject")).toString();
    const bool openProofFarcaster =
        issuer == openProofIssuer.toString(QUrl::FullyEncoded)
        && subject == QStringLiteral("farcaster:") + canonicalFid;
    const bool nativeHyperSnap =
        fid >= firstNativeHyperSnapFid
        && issuer == QStringLiteral("hypersnap:native-registry")
        && subject == QStringLiteral("hypersnap:") + canonicalFid;
    return openProofFarcaster || nativeHyperSnap;
}

} // namespace genycaster::identity
