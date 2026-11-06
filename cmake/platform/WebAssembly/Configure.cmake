# Platform-scoped GenyCaster build behavior; not used on other targets.
if(EMSCRIPTEN)
    # SQLite's WASM amalgamation is C, whereas the native product is C++26
    # modules. Never discover/link host macOS SQLite in a cross build.
    enable_language(C)
    message(STATUS "Genycaster WebAssembly: browser sandbox composition")
    # No OS keychain or localhost EIP-1193 server exists in a browser.
    # Fail closed for signing rather than silently keeping keys in JS storage.
    set(FARCASTER_ENABLE_PROTOCOL_SIGNING OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_QTKEYCHAIN_SIGNER_VAULT OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_SNAPCHAIN_WRITE_PROVIDER OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_SIGNER_ATTESTATION_PROVIDER OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_BROWSER_WALLET_AUTHORIZER OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_BROWSER_WALLET_BRIDGE OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_MINIAPPS OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_MEDIA_UPLOAD OFF CACHE BOOL "" FORCE)
    set(GENYCASTER_ENABLE_PUBLIC_DATA_PROVIDER OFF CACHE BOOL "" FORCE)
    # Native SIWF and OpenProof adapters use blocking / OS browser transport;
    # they require a dedicated asynchronous web identity adapter.
    set(FARCASTER_ENABLE_COMPATIBILITY_PROVIDERS OFF CACHE BOOL "" FORCE)
    set(GENYCASTER_ENABLE_OPENPROOF_AUTH OFF CACHE BOOL "" FORCE)
    set(FARCASTER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
endif()
