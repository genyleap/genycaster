cmake_minimum_required(VERSION 3.30)

project(Genycaster
    VERSION 1.0.0
    DESCRIPTION "GenyCaster is a native, high-performance decentralized social client with Modern C++ and Qt."
    LANGUAGES CXX
)

# Version identity has independent numeric, channel, and channel-number fields.
# CMake alone constructs the serialized semantic tag used by update discovery.
set(GENYCASTER_RELEASE_CHANNEL "rc")
set(GENYCASTER_RELEASE_CHANNEL_NUMBER "1")
set(GENYCASTER_RELEASE_SUFFIX "")
if(NOT GENYCASTER_RELEASE_CHANNEL STREQUAL "stable")
    if(NOT GENYCASTER_RELEASE_CHANNEL MATCHES "^(alpha|beta|rc)$"
       OR NOT GENYCASTER_RELEASE_CHANNEL_NUMBER MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "Invalid prerelease channel or channel number")
    endif()
    set(GENYCASTER_RELEASE_SUFFIX "-${GENYCASTER_RELEASE_CHANNEL}${GENYCASTER_RELEASE_CHANNEL_NUMBER}")
endif()
set(GENYCASTER_RELEASE_VERSION "${PROJECT_VERSION}${GENYCASTER_RELEASE_SUFFIX}")
set(GENYCASTER_RELEASE_TAG "v${GENYCASTER_RELEASE_VERSION}")
set(GENYCASTER_RELEASE_NOTES_FILE
    "${CMAKE_SOURCE_DIR}/release-notes/${GENYCASTER_RELEASE_TAG}.json")
set(GENYCASTER_RELEASE_MARKDOWN_FILE
    "${CMAKE_SOURCE_DIR}/release-notes/${GENYCASTER_RELEASE_TAG}.md")
if(NOT EXISTS "${GENYCASTER_RELEASE_NOTES_FILE}"
   OR NOT EXISTS "${GENYCASTER_RELEASE_MARKDOWN_FILE}")
    message(FATAL_ERROR "Missing release notes JSON or generated GitHub Markdown for ${GENYCASTER_RELEASE_TAG}")
endif()

# Platform-specific compiler, SDK and sandbox setup lives with that platform.
# Shared product configuration remains independent of the deployment target.
if(WIN32)
    include("${CMAKE_SOURCE_DIR}/cmake/platform/Windows/configure.cmake")
elseif(IOS)
    include("${CMAKE_SOURCE_DIR}/cmake/platform/iOS/configure.cmake")
elseif(ANDROID)
    include("${CMAKE_SOURCE_DIR}/cmake/platform/Android/configure.cmake")
elseif(EMSCRIPTEN)
    include("${CMAKE_SOURCE_DIR}/cmake/platform/WebAssembly/configure.cmake")
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

if(ANDROID)
    genycaster_android_apply_test_policy()
endif()

option(FARCASTER_BUILD_UI "Build the Qt Quick presentation shell" ON)

# Linux-specific relocation and packaging policies belong to the Linux
# build configuration, not to the shared project defaults.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND FARCASTER_BUILD_UI)
    include("${CMAKE_SOURCE_DIR}/cmake/platform/Linux/configure.cmake")
endif()

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

# Secure CMake-owned deployment configuration. No secrets or user-controlled JS.
set(GENYCASTER_PUBLIC_ORIGIN "https://genycaster.xyz" CACHE STRING "Canonical public application origin")
set(GENYCASTER_SIWF_ORIGIN "https://genycaster.xyz" CACHE STRING "Trusted SIWF relying party origin (separate from the hosting origin)")
# Older configured Qt build trees retain their CMake cache across source
# upgrades. Migrate only the two published legacy Genycaster defaults; keep
# explicit third-party/self-hosted overrides intact.
if(GENYCASTER_PUBLIC_ORIGIN STREQUAL "https://caster.genyleap.com")
    set(GENYCASTER_PUBLIC_ORIGIN "https://genycaster.xyz" CACHE STRING
        "Canonical public application origin" FORCE)
endif()
if(GENYCASTER_SIWF_ORIGIN STREQUAL "https://genycaster.genyleap.com")
    set(GENYCASTER_SIWF_ORIGIN "https://genycaster.xyz" CACHE STRING
        "Trusted SIWF relying party origin" FORCE)
endif()
set(GENYCASTER_OPENPROOF_ISSUER "https://openproof.genyleap.com" CACHE STRING "Trusted OpenProof issuer")
set(GENYCASTER_OPENPROOF_BROWSER_BASE "https://genyleap.com/openproof" CACHE STRING "OAuth browser handoff base")
set(GENYCASTER_BROWSER_GATEWAY_ORIGIN "https://storage-farcaster.genyleap.com" CACHE STRING "Browser gateway origin")
set(GENYCASTER_MEDIA_PUBLIC_ORIGIN "https://media-farcaster.genyleap.com" CACHE STRING "Media delivery origin")
set(GENYCASTER_SNAPCHAIN_ORIGIN "https://snapchain.genyleap.com" CACHE STRING "Default Snapchain read origin")
set(GENYCASTER_HYPERSNAP_ORIGIN "https://haatz.quilibrium.com" CACHE STRING "Default HyperSnap read origin")
set(GENYCASTER_COMPANY_ORIGIN "https://genyleap.com" CACHE STRING "Public legal origin")
# Protocol adapters also consume this header in headless/client-library builds.
# Generate for every build profile, not only the UI application.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_deploymentConfigArgs)
foreach(_key IN ITEMS PUBLIC_ORIGIN SIWF_ORIGIN OPENPROOF_ISSUER OPENPROOF_BROWSER_BASE
                          BROWSER_GATEWAY_ORIGIN MEDIA_PUBLIC_ORIGIN SNAPCHAIN_ORIGIN
                          HYPERSNAP_ORIGIN COMPANY_ORIGIN)
    # A semicolon would split the CMake list into new process arguments.
    # Reject it before passing any untrusted cache value to the generator.
    string(FIND "${GENYCASTER_${_key}}" ";" _deploymentListSeparator)
    if(NOT _deploymentListSeparator EQUAL -1)
        message(FATAL_ERROR "GENYCASTER_${_key} cannot contain a CMake list separator")
    endif()
    list(APPEND _deploymentConfigArgs --set "GENYCASTER_${_key}=${GENYCASTER_${_key}}")
endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/ci/generate_deployment_config.py")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/ci/generate_deployment_config.py"
            --out-dir "${CMAKE_BINARY_DIR}/generated" ${_deploymentConfigArgs}
        RESULT_VARIABLE _deploymentConfigResult
        ERROR_VARIABLE _deploymentConfigError
    )
    if(NOT _deploymentConfigResult EQUAL 0)
        message(FATAL_ERROR "Invalid GenyCaster deployment configuration: ${_deploymentConfigError}")
    endif()
    include_directories(BEFORE "${CMAKE_BINARY_DIR}/generated")

# Shipping deployment identity/origins. These are first-party product defaults,
# not provider identities. Runtime CLI/environment overrides remain available for
# development, testing and self-hosted deployments.
string(REGEX REPLACE "^https://" "" _GENYCASTER_CANONICAL_SIWF_DOMAIN
    "${GENYCASTER_SIWF_ORIGIN}")
set(_GENYCASTER_CANONICAL_SIWF_URI "${GENYCASTER_SIWF_ORIGIN}/login")
set(FARCASTER_DEFAULT_SIWF_DOMAIN
    "${_GENYCASTER_CANONICAL_SIWF_DOMAIN}"
    CACHE STRING "Default SIWF domain for shipping Genycaster builds")
set(FARCASTER_DEFAULT_SIWF_URI
    "${_GENYCASTER_CANONICAL_SIWF_URI}"
    CACHE STRING "Default SIWF URI for shipping Genycaster builds")
if(FARCASTER_DEFAULT_SIWF_DOMAIN STREQUAL "genycaster.genyleap.com")
    set(FARCASTER_DEFAULT_SIWF_DOMAIN "genycaster.xyz"
        CACHE STRING "Default SIWF domain for shipping Genycaster builds" FORCE)
endif()
if(FARCASTER_DEFAULT_SIWF_URI STREQUAL "https://genycaster.genyleap.com/login")
    set(FARCASTER_DEFAULT_SIWF_URI "https://genycaster.xyz/login"
        CACHE STRING "Default SIWF URI for shipping Genycaster builds" FORCE)
endif()

# CMake cache entries survive source upgrades. Reject stale malformed SIWF values
# before they can be compiled into a shipping binary. Local development values
# such as http://localhost:3000/login remain valid.
if(NOT FARCASTER_DEFAULT_SIWF_DOMAIN MATCHES "^[A-Za-z0-9.-]+(:[0-9]+)?$")
    message(FATAL_ERROR "Invalid FARCASTER_DEFAULT_SIWF_DOMAIN. Clear the stale CMake cache or provide a valid domain.")
endif()
if(NOT FARCASTER_DEFAULT_SIWF_URI MATCHES "^https?://[^ \t\r\n\"'\\]+$")
    message(FATAL_ERROR "Invalid FARCASTER_DEFAULT_SIWF_URI. Clear the stale CMake cache or provide a valid URI.")
endif()
string(REGEX REPLACE "^https?://([^/]+).*$" "\\1" _farcasterSiwfUriAuthority
    "${FARCASTER_DEFAULT_SIWF_URI}")
if(NOT _farcasterSiwfUriAuthority STREQUAL FARCASTER_DEFAULT_SIWF_DOMAIN)
    message(FATAL_ERROR
        "SIWF domain and URI must use the same relying-party authority. "
        "Set FARCASTER_DEFAULT_SIWF_DOMAIN and FARCASTER_DEFAULT_SIWF_URI "
        "to matching values or clear stale CMake cache entries.")
endif()
unset(_farcasterSiwfUriAuthority)
unset(_GENYCASTER_CANONICAL_SIWF_DOMAIN)
unset(_GENYCASTER_CANONICAL_SIWF_URI)
set(FARCASTER_DEFAULT_SNAPCHAIN_BASE_URL
    "${GENYCASTER_SNAPCHAIN_ORIGIN}"
    CACHE STRING "Default Genycaster-facing Snapchain HTTP origin")
set(FARCASTER_DEFAULT_MEDIA_GATEWAY_BASE_URL
    "${GENYCASTER_BROWSER_GATEWAY_ORIGIN}"
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
# Android v1 has a public, origin-locked QtWebView, not the privileged
# Farcaster Mini App SDK. Keep scope explicit and reject unsupported claims.
if(ANDROID)
    set(GENYCASTER_ANDROID_MINIAPP_SCOPE "public-only" CACHE STRING
        "Android Mini App capability: public HTTPS pages only; no SDK or wallet bridge")
    if(NOT GENYCASTER_ANDROID_MINIAPP_SCOPE STREQUAL "public-only")
        message(FATAL_ERROR
            "Android Mini App SDK support is not implemented. "
            "Use GENYCASTER_ANDROID_MINIAPP_SCOPE=public-only.")
    endif()
    if(FARCASTER_ENABLE_MINIAPPS)
        message(FATAL_ERROR
            "Android has no privileged Mini App SDK host. "
            "Do not enable FARCASTER_ENABLE_MINIAPPS on this platform.")
    endif()
endif()
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

if(ANDROID)
    genycaster_android_apply_toolchain_options()
endif()

include(CTest)

if(FARCASTER_BUILD_UI)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Quick Qml QuickControls2 Network Multimedia Concurrent LinguistTools)
    if(ANDROID)
        # The app can compile while Qt for Android was built with SSL disabled,
        # but every HTTPS provider will then fail at runtime. Reject that SDK
        # before building an APK that deceptively appears ready to connect.
        # Defined by the Android platform configure.cmake
        genycaster_require_android_qt_tls("${Qt6Network_INCLUDE_DIRS}")
    endif()
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
    genycaster_android_setup_openssl()
    genycaster_android_setup_boost_headers()
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
                "${CMAKE_SOURCE_DIR}/ci/check_macos_sdk_includes.py"
                "${CMAKE_BINARY_DIR}/compile_commands.json"
        )
        set_tests_properties(macos_sdk_include_guard PROPERTIES LABELS "architecture;macos")
    endif()
endif()

farcaster_register_architecture_guard()
