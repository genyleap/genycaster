cmake_minimum_required(VERSION 3.30)

project(Genycaster
    VERSION 1.0.0
    DESCRIPTION "Genycaster is a native, high-performance Farcaster client built from the ground up with Modern C++ and Qt."
    LANGUAGES CXX
)

if(ANDROID)
    # The bundled SQLite amalgamation is C and must use the Android NDK toolchain.
    # Native desktop builds continue to use their existing SQLite integration.
    enable_language(C)
endif()

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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AimcppProjectChecks)
include(ProjectWarnings)
include(ArchitectureGuards)
include(Sanitizers)

# Android APK builds should compile the product, not host/device test runners.
# Keep the flag opt-in on Android; explicit cache overrides are respected for
# developers intentionally cross-compiling device tests.
set(_farcasterBuildTestsDefault ON)
if(ANDROID)
    set(_farcasterBuildTestsDefault OFF)
endif()
option(FARCASTER_BUILD_TESTS "Build unit, contract, integration and architecture tests"
    ${_farcasterBuildTestsDefault})
unset(_farcasterBuildTestsDefault)
option(FARCASTER_BUILD_UI "Build the Qt Quick presentation shell" ON)

set(_genycasterPublicDataDefault OFF)
if(FARCASTER_BUILD_UI)
    set(_genycasterPublicDataDefault ON)
endif()
option(
    GENYCASTER_ENABLE_PUBLIC_DATA_PROVIDER
    "Compile optional CoinGecko/GeckoTerminal/Neynar public insight adapters"
    ${_genycasterPublicDataDefault}
)
unset(_genycasterPublicDataDefault)

set(_farcasterMediaUploadDefault OFF)
if(FARCASTER_BUILD_UI)
    set(_farcasterMediaUploadDefault ON)
endif()
option(
    FARCASTER_ENABLE_MEDIA_UPLOAD
    "Compile provider-neutral media upload through the Genyleap media gateway"
    ${_farcasterMediaUploadDefault}
)
unset(_farcasterMediaUploadDefault)

# Shipping deployment identity/origins. These are first-party product defaults,
# not provider identities. Runtime CLI/environment overrides remain available for
# development, testing and self-hosted deployments.
set(_GENYCASTER_CANONICAL_SIWF_DOMAIN "genycaster.genyleap.com")
set(_GENYCASTER_CANONICAL_SIWF_URI "https://genycaster.genyleap.com/login")
set(FARCASTER_DEFAULT_SIWF_DOMAIN
    "${_GENYCASTER_CANONICAL_SIWF_DOMAIN}"
    CACHE STRING "Default SIWF domain for shipping Genycaster builds")
set(FARCASTER_DEFAULT_SIWF_URI
    "${_GENYCASTER_CANONICAL_SIWF_URI}"
    CACHE STRING "Default SIWF URI for shipping Genycaster builds")

# CMake cache entries survive source upgrades. Reject stale malformed SIWF values
# before they can be compiled into a shipping binary. Local development values
# such as http://localhost:3000/login remain valid.
if(NOT FARCASTER_DEFAULT_SIWF_DOMAIN MATCHES "^[A-Za-z0-9.-]+(:[0-9]+)?$")
    message(WARNING
        "Invalid cached FARCASTER_DEFAULT_SIWF_DOMAIN='${FARCASTER_DEFAULT_SIWF_DOMAIN}'. "
        "Resetting it to '${_GENYCASTER_CANONICAL_SIWF_DOMAIN}'.")
    set(FARCASTER_DEFAULT_SIWF_DOMAIN
        "${_GENYCASTER_CANONICAL_SIWF_DOMAIN}"
        CACHE STRING "Default SIWF domain for shipping Genycaster builds" FORCE)
endif()
if(NOT FARCASTER_DEFAULT_SIWF_URI MATCHES "^https?://[^ \t\r\n\"'\\]+$")
    message(WARNING
        "Invalid cached FARCASTER_DEFAULT_SIWF_URI='${FARCASTER_DEFAULT_SIWF_URI}'. "
        "Resetting it to '${_GENYCASTER_CANONICAL_SIWF_URI}'.")
    set(FARCASTER_DEFAULT_SIWF_URI
        "${_GENYCASTER_CANONICAL_SIWF_URI}"
        CACHE STRING "Default SIWF URI for shipping Genycaster builds" FORCE)
endif()
string(REGEX REPLACE "^https?://([^/]+).*$" "\\1" _farcasterSiwfUriAuthority
    "${FARCASTER_DEFAULT_SIWF_URI}")
if(NOT _farcasterSiwfUriAuthority STREQUAL FARCASTER_DEFAULT_SIWF_DOMAIN)
    message(WARNING
        "Cached SIWF domain/URI do not describe the same relying party "
        "('${FARCASTER_DEFAULT_SIWF_DOMAIN}' vs '${FARCASTER_DEFAULT_SIWF_URI}'). "
        "Resetting both to the canonical Genycaster deployment identity.")
    set(FARCASTER_DEFAULT_SIWF_DOMAIN
        "${_GENYCASTER_CANONICAL_SIWF_DOMAIN}"
        CACHE STRING "Default SIWF domain for shipping Genycaster builds" FORCE)
    set(FARCASTER_DEFAULT_SIWF_URI
        "${_GENYCASTER_CANONICAL_SIWF_URI}"
        CACHE STRING "Default SIWF URI for shipping Genycaster builds" FORCE)
endif()
unset(_farcasterSiwfUriAuthority)
unset(_GENYCASTER_CANONICAL_SIWF_DOMAIN)
unset(_GENYCASTER_CANONICAL_SIWF_URI)
set(FARCASTER_DEFAULT_SNAPCHAIN_BASE_URL
    "https://snapchain.genyleap.com"
    CACHE STRING "Default Genycaster-facing Snapchain HTTP origin")
set(FARCASTER_DEFAULT_MEDIA_GATEWAY_BASE_URL
    "https://storage-farcaster.genyleap.com"
    CACHE STRING "Default Genycaster media upload gateway origin")
set(FARCASTER_DEFAULT_APP_FID
    "866408"
    CACHE STRING "Canonical Farcaster App FID for Genycaster attribution")
set(FARCASTER_DEFAULT_APP_NAME
    "Genycaster"
    CACHE STRING "Canonical Farcaster App display name for Genycaster")
# CMake caches survive source upgrades. Older development builds either guessed
# FID 256 or retained 0 as an unconfigured client identity. Migrate those known
# legacy values to the canonical Genycaster/Genyleap App FID used for attribution.
if(FARCASTER_DEFAULT_APP_FID STREQUAL "256" OR FARCASTER_DEFAULT_APP_FID STREQUAL "0")
    set(FARCASTER_DEFAULT_APP_FID
        "866408"
        CACHE STRING "Canonical Farcaster App FID for Genycaster attribution" FORCE)
endif()
if(NOT FARCASTER_DEFAULT_APP_FID MATCHES "^[0-9]+$")
    message(FATAL_ERROR
        "FARCASTER_DEFAULT_APP_FID must be an unsigned decimal FID.")
endif()

# Optional native permalink destination, independent of Farcaster provider/auth.
# WASM always uses the HTTPS origin hosting the current application.
set(GENYCASTER_DEFAULT_PUBLIC_BASE_URL "" CACHE STRING
    "Default public Genycaster HTTPS base URL for packaged native builds")

# These options describe product composition. Feature implementation targets are
# created only when real behavior exists; empty ceremonial feature libraries are forbidden.
option(FARCASTER_ENABLE_SOCIAL "Enable social product composition" ON)
option(FARCASTER_ENABLE_FEED "Enable feed product composition" ON)
option(FARCASTER_ENABLE_CHANNELS "Enable channels product composition" OFF)
set(_farcasterMiniAppsDefault OFF)
if(FARCASTER_BUILD_UI AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_farcasterMiniAppsDefault ON)
endif()
option(FARCASTER_ENABLE_MINIAPPS "Enable Mini Apps product composition" ${_farcasterMiniAppsDefault})
unset(_farcasterMiniAppsDefault)
option(FARCASTER_ENABLE_CHAT "Enable chat product composition" OFF)
option(FARCASTER_ENABLE_SEARCH "Enable search product composition" OFF)
option(FARCASTER_ENABLE_NOTIFICATIONS "Enable notifications product composition" ON)
option(FARCASTER_ENABLE_WALLET "Enable wallet product composition" OFF)
option(FARCASTER_ENABLE_DERIVED_CAST_VIEWS
    "Enable optional public view-count analytics in the Qt client" ON)

set(_farcasterHypersnapProviderDefault OFF)
if(FARCASTER_BUILD_UI)
    # The current runnable UI has one implemented live-read provider. Core-only
    # presets override this back to OFF explicitly, preserving provider-free builds.
    set(_farcasterHypersnapProviderDefault ON)
endif()

option(
    FARCASTER_ENABLE_HYPERSNAP_PROVIDER
    "Compile the Hypersnap v2 provider adapter"
    ${_farcasterHypersnapProviderDefault}
)
unset(_farcasterHypersnapProviderDefault)

set(_farcasterCompatibilityProviderDefault OFF)
if(FARCASTER_BUILD_UI)
    # Normal graphical builds include the optional SIWF compatibility login.
    # Provider-free core and strict-self-hosted presets override this to OFF.
    set(_farcasterCompatibilityProviderDefault ON)
endif()
option(
    FARCASTER_ENABLE_COMPATIBILITY_PROVIDERS
    "Compile managed/legacy compatibility adapters"
    ${_farcasterCompatibilityProviderDefault}
)
unset(_farcasterCompatibilityProviderDefault)

set(_genycasterOpenProofAuthDefault OFF)
if(FARCASTER_BUILD_UI)
    set(_genycasterOpenProofAuthDefault ON)
endif()
option(
    GENYCASTER_ENABLE_OPENPROOF_AUTH
    "Use OpenProof/Genyleap as the canonical Genycaster account authority"
    ${_genycasterOpenProofAuthDefault}
)
unset(_genycasterOpenProofAuthDefault)

set(_farcasterProtocolSigningDefault OFF)
if(FARCASTER_BUILD_UI)
    # Graphical builds include the native secure signer vault boundary. Writes
    # remain disabled until SignerService reports network-verified readiness.
    set(_farcasterProtocolSigningDefault ON)
endif()
option(
    FARCASTER_ENABLE_PROTOCOL_SIGNING
    "Compile the native protocol-signer secure-storage boundary"
    ${_farcasterProtocolSigningDefault}
)
unset(_farcasterProtocolSigningDefault)

set(_farcasterQtKeychainSignerVaultDefault OFF)
if(FARCASTER_ENABLE_PROTOCOL_SIGNING AND NOT APPLE)
    # Non-Apple shipping builds use QtKeychain as the portable secure-secret
    # boundary: Windows Credential Store, Linux Secret Service/KWallet, and
    # Android Keystore. Apple keeps the direct Security.framework adapter.
    set(_farcasterQtKeychainSignerVaultDefault ON)
endif()
option(
    FARCASTER_ENABLE_QTKEYCHAIN_SIGNER_VAULT
    "Enable the QtKeychain-backed secure signer vault on non-Apple platforms"
    ${_farcasterQtKeychainSignerVaultDefault}
)
unset(_farcasterQtKeychainSignerVaultDefault)

option(
    FARCASTER_REQUIRE_SECURE_SIGNER_VAULT
    "Fail configure when protocol signing has no secure platform vault backend"
    OFF
)

set(_farcasterSnapchainReadProviderDefault OFF)
if(FARCASTER_BUILD_UI)
    set(_farcasterSnapchainReadProviderDefault ON)
endif()
option(FARCASTER_ENABLE_SNAPCHAIN_READ_PROVIDER
    "Compile direct raw Snapchain read-node adapter with exact capability declarations"
    ${_farcasterSnapchainReadProviderDefault})
unset(_farcasterSnapchainReadProviderDefault)

set(_farcasterSnapchainSignerProviderDefault OFF)
if(FARCASTER_BUILD_UI AND FARCASTER_ENABLE_PROTOCOL_SIGNING)
    # Runnable signed-write builds include canonical signer-registry inspection,
    # KEY_ADD/KEY_REMOVE submission, and post-submit network verification.
    set(_farcasterSnapchainSignerProviderDefault ON)
endif()
option(
    FARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER
    "Compile the canonical Snapchain signer-registry adapter"
    ${_farcasterSnapchainSignerProviderDefault}
)
unset(_farcasterSnapchainSignerProviderDefault)

set(_farcasterSnapchainWriteProviderDefault OFF)
if(FARCASTER_BUILD_UI AND FARCASTER_ENABLE_PROTOCOL_SIGNING)
    # Protocol-message submission/reconciliation is a separate capability from signer
    # provisioning even though normal shipping UI builds compose both.
    set(_farcasterSnapchainWriteProviderDefault ON)
endif()
option(
    FARCASTER_ENABLE_SNAPCHAIN_WRITE_PROVIDER
    "Compile the canonical Snapchain message submit/reconciliation adapter"
    ${_farcasterSnapchainWriteProviderDefault}
)
unset(_farcasterSnapchainWriteProviderDefault)

set(_farcasterBrowserWalletAuthorizerDefault OFF)
if(FARCASTER_BUILD_UI AND FARCASTER_ENABLE_PROTOCOL_SIGNING)
    # The compatibility authorizer is transport-only: it serves canonical EIP-712
    # documents on loopback and delegates URL launching to the Qt shell.
    set(_farcasterBrowserWalletAuthorizerDefault ON)
endif()
option(
    FARCASTER_ENABLE_BROWSER_WALLET_AUTHORIZER
    "Compile the cross-platform loopback EIP-1193 custody authorizer"
    ${_farcasterBrowserWalletAuthorizerDefault}
)
unset(_farcasterBrowserWalletAuthorizerDefault)

set(_farcasterMiniAppWalletBridgeDefault OFF)
if(FARCASTER_BUILD_UI AND FARCASTER_ENABLE_MINIAPPS)
    set(_farcasterMiniAppWalletBridgeDefault ON)
endif()
option(
    FARCASTER_ENABLE_BROWSER_WALLET_BRIDGE
    "Compile the isolated EIP-1193 Mini App browser-wallet bridge"
    ${_farcasterMiniAppWalletBridgeDefault}
)
unset(_farcasterMiniAppWalletBridgeDefault)

set(_farcasterSignerAttestationProviderDefault OFF)
if(FARCASTER_BUILD_UI AND FARCASTER_ENABLE_PROTOCOL_SIGNING)
    # Production App-FID attribution uses a tiny HTTPS attestation service so
    # the user's custody wallet signs only KEY_ADD. The app custody key never
    # ships inside the desktop binary.
    set(_farcasterSignerAttestationProviderDefault ON)
endif()
option(
    FARCASTER_ENABLE_SIGNER_ATTESTATION_PROVIDER
    "Compile the App-FID SignedKeyRequest attestation adapter"
    ${_farcasterSignerAttestationProviderDefault}
)
unset(_farcasterSignerAttestationProviderDefault)

option(FARCASTER_SELF_HOSTED_ONLY "Disallow public/managed providers by deployment policy" OFF)

if(FARCASTER_BUILD_UI AND NOT FARCASTER_ENABLE_HYPERSNAP_PROVIDER)
    message(FATAL_ERROR
        "The runnable Genycaster UI currently requires a compiled live-read provider. "
        "Hypersnap is the only implemented UI read provider in this checkpoint. "
        "Enable FARCASTER_ENABLE_HYPERSNAP_PROVIDER=ON or use a provider-free core preset."
    )
endif()

if(FARCASTER_BUILD_UI AND FARCASTER_SELF_HOSTED_ONLY)
    message(FATAL_ERROR
        "The runnable UI cannot use FARCASTER_SELF_HOSTED_ONLY yet because no self-hosted "
        "read adapter is implemented in this checkpoint. Use the strict-self-hosted core "
        "preset until a self-hosted UI provider binding exists."
    )
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

if(FARCASTER_BUILD_UI)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Quick Qml QuickControls2 Network Multimedia Concurrent LinguistTools)
    set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml")

    if(QT_KNOWN_POLICY_QTP0004)
        qt_policy(SET QTP0004 NEW)
    endif()
    if(QT_KNOWN_POLICY_QTP0002)
        # Required for generator expressions in QT_ANDROID_EXTRA_LIBS.
        qt_policy(SET QTP0002 NEW)
    endif()

    qt_standard_project_setup(REQUIRES 6.12)
endif()

# Prepare Android-only target dependencies before provider subdirectories.
# Neither Boost nor OpenSSL may link host (macOS/Homebrew) binaries into APKs.
if(ANDROID AND FARCASTER_BUILD_UI)
    include(AndroidOpenSSL)
    include(AndroidBoostHeaders)
endif()

add_subdirectory(src/foundation)
add_subdirectory(src/domain)
add_subdirectory(src/capabilities)
add_subdirectory(src/protocol)
add_subdirectory(src/provider)
add_subdirectory(src/local_state)
add_subdirectory(src/application)
add_subdirectory(src/application/emoji)
add_subdirectory(src/execution)
add_subdirectory(src/routing)
add_subdirectory(src/sync)
add_subdirectory(src/storage)
add_subdirectory(src/transport)

if(FARCASTER_ENABLE_PROTOCOL_SIGNING)
    add_subdirectory(src/platform)
endif()

if(FARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER OR FARCASTER_ENABLE_SNAPCHAIN_WRITE_PROVIDER
    OR FARCASTER_ENABLE_SNAPCHAIN_READ_PROVIDER)
    add_subdirectory(src/providers/snapchain)
endif()

if(FARCASTER_ENABLE_HYPERSNAP_PROVIDER)
    add_subdirectory(src/providers/hypersnap)
endif()

if(GENYCASTER_ENABLE_PUBLIC_DATA_PROVIDER)
    add_subdirectory(src/providers/public_data)
endif()

if(GENYCASTER_ENABLE_OPENPROOF_AUTH)
    add_subdirectory(src/providers/openproof_auth)
endif()

if(FARCASTER_ENABLE_COMPATIBILITY_PROVIDERS)
    add_subdirectory(src/providers/compatibility/farcaster_auth)
endif()

if(FARCASTER_ENABLE_BROWSER_WALLET_AUTHORIZER OR FARCASTER_ENABLE_BROWSER_WALLET_BRIDGE)
    add_subdirectory(src/providers/compatibility/browser_wallet)
endif()

# The emoji platform is part of the product core: it carries no provider or
# network dependency in the application/domain layers and is always compiled.
add_subdirectory(src/providers/emoji)

if(FARCASTER_ENABLE_SIGNER_ATTESTATION_PROVIDER)
    add_subdirectory(src/providers/compatibility/signer_attestation)
endif()

if(FARCASTER_BUILD_UI)
    add_subdirectory(src/presentation)
    add_subdirectory(ui)
endif()


if(FARCASTER_BUILD_TESTS AND BUILD_TESTING)
    add_subdirectory(tests)

    if(APPLE)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_test(
            NAME macos_sdk_include_guard
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/check_macos_sdk_includes.py"
                "${CMAKE_BINARY_DIR}/compile_commands.json"
        )
        set_tests_properties(macos_sdk_include_guard PROPERTIES LABELS "architecture;macos")
    endif()
endif()

farcaster_register_architecture_guard()
