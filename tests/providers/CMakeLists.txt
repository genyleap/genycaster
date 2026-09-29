if(FARCASTER_ENABLE_HYPERSNAP_PROVIDER)
    add_subdirectory(hypersnap)
endif()

if(GENYCASTER_ENABLE_OPENPROOF_AUTH)
    add_subdirectory(openproof_auth)
endif()

if(FARCASTER_ENABLE_COMPATIBILITY_PROVIDERS)
    add_subdirectory(farcaster_auth)
endif()
if(FARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER OR FARCASTER_ENABLE_SNAPCHAIN_WRITE_PROVIDER)
    add_subdirectory(snapchain)
endif()

if(FARCASTER_ENABLE_BROWSER_WALLET_AUTHORIZER OR FARCASTER_ENABLE_BROWSER_WALLET_BRIDGE)
    add_subdirectory(browser_wallet)
endif()

if(FARCASTER_ENABLE_SIGNER_ATTESTATION_PROVIDER)
    add_subdirectory(signer_attestation)
endif()
