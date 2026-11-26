/*!
 * @file        ui/app/identity/cloud_device_retirement.hpp
 * @brief       Durable cloud-device retirement records for native Qt adapters.
 *
 * @details
 * Shares public retirement markers and one-way signed revocation receipts
 * at the textual Qt settings boundary, separately from private key storage.
 *
 * @author      Kambiz Asadzadeh
 * @since       26 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
#pragma once
#include <QSettings>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QtTypes>

namespace genycaster::identity::cloud_retirement {

// This durable outbox contains only public identities and one-way signed
// revocation receipts. It is deliberately separate from credential storage.
[[nodiscard]] inline QSettings settings()
{
    return QSettings{QSettings::IniFormat,QSettings::UserScope,
        QStringLiteral("Genyleap"),QStringLiteral("Genycaster-cloud-retirement")};
}

[[nodiscard]] inline QString marker(const QString& network,qulonglong fid)
{
    return QStringLiteral("pending/")+network+QStringLiteral("/")+QString::number(fid);
}

[[nodiscard]] inline bool validPrincipal(const QString& network,qulonglong fid)
{
    return fid && ((network==QStringLiteral("farcaster")&&fid<=9'007'199'254'740'991ULL)||
        (network==QStringLiteral("hypersnap")&&fid>=4294967296ULL));
}

[[nodiscard]] inline bool mark(const QString& network,qulonglong fid)
{
    if(!validPrincipal(network,fid))return false;
    auto store=settings();
    store.setValue(marker(network,fid),true);
    store.sync();
    return store.status()==QSettings::NoError;
}

[[nodiscard]] inline QStringList pending(QSettings& store)
{
    store.sync();
    auto keys=store.allKeys();
    keys.removeIf([](const QString& key){return !key.startsWith(QStringLiteral("pending/"));});
    return keys;
}

[[nodiscard]] inline QStringList receipts(QSettings& store)
{
    store.sync();
    auto keys=store.allKeys();
    keys.removeIf([](const QString& key){return !key.startsWith(QStringLiteral("receipts/"));});
    keys.sort();
    return keys;
}

} // namespace genycaster::identity::cloud_retirement
