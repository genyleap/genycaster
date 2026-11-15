#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

import farcaster.application.registration_plan;

int main()
{
    using namespace farcaster::application::registration_plan;
    using A = RegistrationAction;
    using N = RegistrationNetwork;
    RegistrationReadiness state{};
    for (const auto network : {N::HyperSnapNative, N::FarcasterOptimism})
        assert(nextRegistrationAction(network, state) == A::GenerateWallet);
    state.walletGenerated = true;
    for (const auto network : {N::HyperSnapNative, N::FarcasterOptimism})
        assert(nextRegistrationAction(network, state) == A::VerifyOfflineBackup);
    state.backupVerified = true;

    // Farcaster preconditions cannot accidentally activate native HyperSnap.
    assert(nextRegistrationAction(N::HyperSnapNative, state)
           == A::CheckHyperSnapNativeActivation);
    assert(nextRegistrationAction(N::FarcasterOptimism, state)
           == A::RequireSecureNeynarBackend);

    state.nativeRegistrationActive = true;
    assert(nextRegistrationAction(N::HyperSnapNative, state)
           == A::PrepareHyperSnapAnchorAndPow);
    state.nativeAnchorFresh = true;
    state.proofOfWorkSolved = true;
    assert(nextRegistrationAction(N::HyperSnapNative, state)
           == A::SignHyperSnapNativeOnboard);
    state.nativeCustodySigned = true;
    assert(nextRegistrationAction(N::HyperSnapNative, state)
           == A::SubmitHyperSnapNativeOnboard);
    state.nativeSubmitted = true;
    // A broadcast result is NOT proof of registration.
    assert(nextRegistrationAction(N::HyperSnapNative, state)
           == A::VerifyHyperSnapNativeFid);
    state.nativeFidConfirmed = true;
    assert(nextRegistrationAction(N::HyperSnapNative, state)
           == A::ProvisionDeviceSigner);

    // Even when native registration is ready, the Farcaster route MUST
    // independently reserve/confirm an IdRegistry identity.
    assert(nextRegistrationAction(N::FarcasterOptimism, state)
           == A::RequireSecureNeynarBackend);
    state.secureNeynarServiceConfigured = true;
    assert(nextRegistrationAction(N::FarcasterOptimism, state)
           == A::ReserveFarcasterFid);
    state.farcasterFidReserved = true;
    assert(nextRegistrationAction(N::FarcasterOptimism, state)
           == A::VerifyIdRegistryNonce);
    state.idRegistryNonceVerified = true;
    assert(nextRegistrationAction(N::FarcasterOptimism, state)
           == A::SignFarcasterIdTransfer);
    state.transferSigned = true;
    assert(nextRegistrationAction(N::FarcasterOptimism, state)
           == A::SubmitFarcasterRegistration);
    state.neynarSubmitted = true;
    assert(nextRegistrationAction(N::FarcasterOptimism, state)
           == A::VerifyFarcasterIdRegistry);
    state.idRegistryCustodyConfirmed = true;
    assert(nextRegistrationAction(N::FarcasterOptimism, state)
           == A::ProvisionDeviceSigner);

    state.deviceSignerConfirmed = true;
    assert(nextRegistrationAction(N::HyperSnapNative, state) == A::Complete);
    assert(nextRegistrationAction(N::FarcasterOptimism, state) == A::Complete);
    state.idRegistryCustodyConfirmed = false;
    assert(nextRegistrationAction(N::FarcasterOptimism, state)
           == A::VerifyFarcasterIdRegistry);
    assert(nextRegistrationAction(N::HyperSnapNative, state) == A::Complete);
    std::cout << "Registration protocol isolation and signer readiness passed\n";
}
