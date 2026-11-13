#pragma once

#include <cstdint>

namespace genycaster::identity {

/**
 * Registration identity namespaces, not interchangeable read API profiles.
 * A native HyperSnap FID does not imply an on-chain Farcaster IdRegistry FID.
 */
enum class RegistrationNetwork : std::uint8_t {
    HyperSnapNative,
    FarcasterOptimism
};

enum class RegistrationAction : std::uint8_t {
    GenerateWallet,
    VerifyOfflineBackup,
    CheckHyperSnapNativeActivation,
    PrepareHyperSnapAnchorAndPow,
    SignHyperSnapNativeOnboard,
    SubmitHyperSnapNativeOnboard,
    VerifyHyperSnapNativeFid,
    RequireSecureNeynarBackend,
    ReserveFarcasterFid,
    VerifyIdRegistryNonce,
    SignFarcasterIdTransfer,
    SubmitFarcasterRegistration,
    VerifyFarcasterIdRegistry,
    ProvisionDeviceSigner,
    Complete
};

/**
 * Explicit, network-scoped registration gates. No inferred completion
 * from a successful HTTP response or from choosing a read provider.
 *
 * This is public state only. Custody mnemonic and private signer material
 * belong to secure native platform storage and never this object.
 */
struct RegistrationReadiness final {
    bool walletGenerated{false};
    bool backupVerified{false};

    // HyperSnap Native-specific progress.
    bool nativeRegistrationActive{false};
    bool nativeAnchorFresh{false};
    bool proofOfWorkSolved{false};
    bool nativeCustodySigned{false};
    bool nativeSubmitted{false};
    bool nativeFidConfirmed{false};

    // Farcaster (Optimism)/Neynar-specific progress.
    bool secureNeynarServiceConfigured{false};
    bool farcasterFidReserved{false};
    bool idRegistryNonceVerified{false};
    bool transferSigned{false};
    bool neynarSubmitted{false};
    bool idRegistryCustodyConfirmed{false};

    // Both routes require a network-registered device signer before "ready".
    bool deviceSignerConfirmed{false};
};

[[nodiscard]] constexpr RegistrationAction nextRegistrationAction(
    const RegistrationNetwork network,
    const RegistrationReadiness& state) noexcept
{
    if (!state.walletGenerated) return RegistrationAction::GenerateWallet;
    if (!state.backupVerified) return RegistrationAction::VerifyOfflineBackup;
    if (network == RegistrationNetwork::HyperSnapNative) {
        if (!state.nativeRegistrationActive)
            return RegistrationAction::CheckHyperSnapNativeActivation;
        if (!state.nativeAnchorFresh)
            return RegistrationAction::PrepareHyperSnapAnchorAndPow;
        if (!state.proofOfWorkSolved)
            return RegistrationAction::PrepareHyperSnapAnchorAndPow;
        if (!state.nativeCustodySigned)
            return RegistrationAction::SignHyperSnapNativeOnboard;
        if (!state.nativeSubmitted)
            return RegistrationAction::SubmitHyperSnapNativeOnboard;
        if (!state.nativeFidConfirmed)
            return RegistrationAction::VerifyHyperSnapNativeFid;
    } else {
        if (!state.secureNeynarServiceConfigured)
            return RegistrationAction::RequireSecureNeynarBackend;
        if (!state.farcasterFidReserved)
            return RegistrationAction::ReserveFarcasterFid;
        if (!state.idRegistryNonceVerified)
            return RegistrationAction::VerifyIdRegistryNonce;
        if (!state.transferSigned)
            return RegistrationAction::SignFarcasterIdTransfer;
        if (!state.neynarSubmitted)
            return RegistrationAction::SubmitFarcasterRegistration;
        if (!state.idRegistryCustodyConfirmed)
            return RegistrationAction::VerifyFarcasterIdRegistry;
    }
    if (!state.deviceSignerConfirmed)
        return RegistrationAction::ProvisionDeviceSigner;
    return RegistrationAction::Complete;
}

} // namespace genycaster::identity
