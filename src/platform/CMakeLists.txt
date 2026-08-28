add_library(farcaster_platform_signer)
add_library(farcaster::platform_signer ALIAS farcaster_platform_signer)

target_sources(farcaster_platform_signer
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            signer_vault.cppm
)

set(_farcasterSecureSignerVaultBackend "unavailable")

if(APPLE)
    # Security.framework is shared by macOS and iOS. Keep the Apple path free of
    # Qt so the headless secure-signer profile remains useful as well.
    find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
    find_library(FARCASTER_SECURITY_FRAMEWORK Security REQUIRED)
    target_sources(farcaster_platform_signer
        PRIVATE
            signer_keychain_boundary.cpp
            signer_vault_macos.cpp
    )
    # Apple's Security/CoreFoundation headers still use CF_ENUM forward
    # declarations that Clang 22 rejects under C++26 + -Wpedantic. Keep the
    # compatibility exception confined to the textual C/ABI boundary.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set_property(SOURCE
            signer_keychain_boundary.cpp
            APPEND PROPERTY COMPILE_OPTIONS
                "-Wno-elaborated-enum-base"
                "-Wno-invalid-utf8"
                "-Wno-nullability-extension"
                "-Wno-variadic-macros"
        )
    endif()
    target_link_libraries(farcaster_platform_signer
        PUBLIC farcaster::capabilities farcaster::domain farcaster::foundation
        PRIVATE OpenSSL::Crypto "${FARCASTER_SECURITY_FRAMEWORK}"
    )
    set(_farcasterSecureSignerVaultBackend "apple-keychain")
elseif(FARCASTER_ENABLE_QTKEYCHAIN_SIGNER_VAULT)
    # QtKeychain 0.17+ provides one maintained secure-secret API across the
    # remaining shipping targets: Windows Credential Store, Linux Secret
    # Service/KWallet, and Android Keystore. It does not use plaintext fallback
    # unless a caller opts into that explicitly; Genycaster never does.
    find_package(Qt6Keychain 0.17 CONFIG QUIET)
    if(Qt6Keychain_FOUND)
        find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
        target_sources(farcaster_platform_signer PRIVATE signer_vault_qtkeychain.cpp)
        target_link_libraries(farcaster_platform_signer
            PUBLIC farcaster::capabilities farcaster::domain farcaster::foundation
            PRIVATE OpenSSL::Crypto Qt6Keychain::Qt6Keychain
        )
        set(_farcasterSecureSignerVaultBackend "qtkeychain")
    else()
        target_sources(farcaster_platform_signer PRIVATE signer_vault_unavailable.cpp)
        target_link_libraries(farcaster_platform_signer
            PUBLIC farcaster::capabilities farcaster::domain farcaster::foundation
        )
        message(WARNING
            "Qt6Keychain >= 0.17 was not found. Protocol signing will compile fail-closed "
            "without persistent signer secrets on this platform. Guest/read behavior remains available."
        )
    endif()
else()
    target_sources(farcaster_platform_signer PRIVATE signer_vault_unavailable.cpp)
    target_link_libraries(farcaster_platform_signer
        PUBLIC farcaster::capabilities farcaster::domain farcaster::foundation
    )
endif()

if(FARCASTER_REQUIRE_SECURE_SIGNER_VAULT
        AND _farcasterSecureSignerVaultBackend STREQUAL "unavailable")
    message(FATAL_ERROR
        "FARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON but no secure signer vault backend is available. "
        "Install Qt6Keychain >= 0.17 on non-Apple platforms or disable protocol signing."
    )
endif()

message(STATUS "Genycaster secure signer vault backend: ${_farcasterSecureSignerVaultBackend}")
unset(_farcasterSecureSignerVaultBackend)

target_compile_features(farcaster_platform_signer PUBLIC cxx_std_26)
set_property(TARGET farcaster_platform_signer PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_platform_signer)
