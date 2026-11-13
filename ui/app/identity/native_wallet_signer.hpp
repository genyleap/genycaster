#pragma once
#include "native_wallet.hpp"
#include <cstdint>
#include <memory>
#include <QString>

// Only include from C++ source with an imported signer capability module.
// This is a trusted native implementation detail, never a QML method.
class NativeCustodySignerAccess final {
public:
    [[nodiscard]] static std::shared_ptr<
        farcaster::capability::signer::ICustodyAuthorizer>
    forConfirmedNativeFid(const NativeCustodyWalletController& wallet,
                          std::uint64_t fid, const QString& custodyAddress);
};
