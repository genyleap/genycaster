/*!
 * @file        ui/app/identity/native_wallet_signer.hpp
 * @brief       Native custody signer access boundary.
 *
 * @details
 * Hands a verified custody authorizer to the protocol signer lifecycle.
 * Prevents controller-level access to raw custody secrets.
 *
 * @author      Genyleap Engineering
 * @since       15 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once
#include "native_wallet.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <memory>
#include <QString>

// Only include from C++ source with an imported signer capability module.
// This is a trusted native implementation detail, never a QML method.
class NativeCustodySignerAccess final {
public:
    using CloudEnrollmentPermit = std::function<std::optional<QString>(const QString&, const QString&)>;
    [[nodiscard]] static CloudEnrollmentPermit forCloudEnrollment(
        const NativeCustodyWalletController& wallet, std::uint64_t fid, const QString& custodyAddress);
    [[nodiscard]] static std::shared_ptr<
        farcaster::capability::signer::ICustodyAuthorizer>
    forConfirmedNativeFid(const NativeCustodyWalletController& wallet,
                          std::uint64_t fid, const QString& custodyAddress);
    [[nodiscard]] static std::shared_ptr<
        farcaster::capability::signer::ICustodyAuthorizer>
    forConfirmedFarcasterFid(const NativeCustodyWalletController& wallet,
                             std::uint64_t fid, const QString& custodyAddress);
};
