# Recovery custody derivation is a portable, private OpenSSL boundary:
# never pass the mnemonic or secp256k1 key into QML, a browser, or a provider.
# This leaf provides public address derivation only; it does not authenticate.
if(NOT TARGET OpenSSL::Crypto)
    find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
endif()
add_library(farcaster_platform_custody_recovery STATIC)
target_sources(farcaster_platform_custody_recovery
    PUBLIC FILE_SET CXX_MODULES FILES custody_recovery.cppm
    PRIVATE custody_recovery.cpp custody_bip39_english.hpp)
add_library(farcaster::platform_custody_recovery ALIAS farcaster_platform_custody_recovery)
target_compile_features(farcaster_platform_custody_recovery PUBLIC cxx_std_26)
target_link_libraries(farcaster_platform_custody_recovery
    PUBLIC farcaster::capabilities farcaster::protocol
    PRIVATE OpenSSL::Crypto)
farcaster_enable_warnings(farcaster_platform_custody_recovery)

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
    find_library(FARCASTER_COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
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
        PRIVATE OpenSSL::Crypto
            "${FARCASTER_SECURITY_FRAMEWORK}"
            "${FARCASTER_COREFOUNDATION_FRAMEWORK}"
    )
    set(_farcasterSecureSignerVaultBackend "apple-keychain")
elseif(FARCASTER_ENABLE_QTKEYCHAIN_SIGNER_VAULT)
    # QtKeychain 0.17+ provides one maintained secure-secret API across the
    # remaining shipping targets: Windows Credential Store, Linux Secret
    # Service/KWallet, and Android Keystore. It does not use plaintext fallback
    # unless a caller opts into that explicitly; Genycaster never does.
    find_package(Qt6Keychain 0.17 CONFIG QUIET)
    set(_farcasterFetchQtKeychainDefault OFF)
    if(ANDROID)
        set(_farcasterFetchQtKeychainDefault ON)
    endif()
    option(GENYCASTER_FETCH_QTKEYCHAIN
        "Build pinned QtKeychain 0.17 from source if no suitable package is installed"
        ${_farcasterFetchQtKeychainDefault})
    unset(_farcasterFetchQtKeychainDefault)
    if(FARCASTER_BUILD_UI AND GENYCASTER_FETCH_QTKEYCHAIN AND NOT Qt6Keychain_FOUND)
        # A Linux source build of QtKeychain uses the system's Secret Service
        # headers. Fail before fetching source or compiling Qt when the
        # distribution's development package is absent.
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            find_package(PkgConfig QUIET)
            if(NOT PkgConfig_FOUND)
                message(FATAL_ERROR
                    "QtKeychain requires pkg-config and libsecret-1 development files "
                    "for secure Linux signer storage.")
            endif()
            pkg_check_modules(GENYCASTER_QTKEYCHAIN_LIBSECRET QUIET libsecret-1)
            if(NOT GENYCASTER_QTKEYCHAIN_LIBSECRET_FOUND)
                message(FATAL_ERROR
                    "Secure Linux signer storage requires libsecret-1 development files. "
                    "Install libsecret-1-dev (Debian/Ubuntu) or the equivalent "
                    "distribution package before configuring GenyCaster. "
                    "Do not disable FARCASTER_REQUIRE_SECURE_SIGNER_VAULT in shipping builds.")
            endif()
        endif()
        # Package builds explicitly enable this source fallback on Linux/Windows.
        # Android uses it by default. Developer builds without libsecret can
        # still configure their pre-existing fail-closed signer adapter.
        include(FetchContent)
        set(BUILD_TRANSLATIONS OFF)
        FetchContent_Declare(genycaster_android_qtkeychain
            GIT_REPOSITORY "https://github.com/frankosterfeld/qtkeychain.git"
            GIT_TAG "875f77d9f61bd97fd84cca47ce3bc71186dfbd09"
            GIT_SHALLOW FALSE
        )
        FetchContent_MakeAvailable(genycaster_android_qtkeychain)
        unset(BUILD_TRANSLATIONS)
        if(TARGET qt6keychain)
            if(NOT TARGET Qt6Keychain::Qt6Keychain)
                add_library(Qt6Keychain::Qt6Keychain ALIAS qt6keychain)
            endif()
            set(Qt6Keychain_FOUND TRUE)
        endif()
    endif()
    if(Qt6Keychain_FOUND)
        if(NOT TARGET OpenSSL::Crypto)
            find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
        endif()
        target_sources(farcaster_platform_signer PRIVATE signer_vault_qtkeychain.cpp)
        target_link_libraries(farcaster_platform_signer
            PUBLIC farcaster::capabilities farcaster::domain farcaster::foundation
            PRIVATE OpenSSL::Crypto Qt6Keychain::Qt6Keychain
        )
        if(WIN32)
            # Direct WinCred signer storage avoids nested Qt event loops on
            # the signer worker; keep QtKeychain for Linux/Android.
            target_link_libraries(farcaster_platform_signer PRIVATE advapi32)
        endif()
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
