cmake_minimum_required(VERSION 3.30)

project(Genycaster
    VERSION 0.4.0
    DESCRIPTION "Genycaster is a native, high-performance Farcaster client built from the ground up with Modern C++ and Qt."
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AimcppProjectChecks)
include(ProjectWarnings)
include(ArchitectureGuards)

option(FARCASTER_BUILD_TESTS "Build unit, contract, integration and architecture tests" ON)
option(FARCASTER_BUILD_UI "Build the Qt Quick presentation shell" ON)

# These options describe product composition. Feature implementation targets are
# created only when real behavior exists; empty ceremonial feature libraries are forbidden.
option(FARCASTER_ENABLE_SOCIAL "Enable social product composition" ON)
option(FARCASTER_ENABLE_FEED "Enable feed product composition" ON)
option(FARCASTER_ENABLE_CHANNELS "Enable channels product composition" OFF)
option(FARCASTER_ENABLE_MINIAPPS "Enable Mini Apps product composition" OFF)
option(FARCASTER_ENABLE_CHAT "Enable chat product composition" OFF)
option(FARCASTER_ENABLE_SEARCH "Enable search product composition" OFF)
option(FARCASTER_ENABLE_NOTIFICATIONS "Enable notifications product composition" OFF)
option(FARCASTER_ENABLE_WALLET "Enable wallet product composition" OFF)

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

set(_farcasterSnapchainSignerProviderDefault OFF)
if(FARCASTER_BUILD_UI AND FARCASTER_ENABLE_PROTOCOL_SIGNING)
    # Runnable signed-write builds include canonical signer-registry inspection.
    # KEY_ADD/KEY_REMOVE submission and custody authorization remain fail-closed.
    set(_farcasterSnapchainSignerProviderDefault ON)
endif()
option(
    FARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER
    "Compile the canonical Snapchain signer-registry adapter"
    ${_farcasterSnapchainSignerProviderDefault}
)
unset(_farcasterSnapchainSignerProviderDefault)

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
    find_package(Qt6 6.6 REQUIRED COMPONENTS Quick Qml QuickControls2 Network)
    set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml")

    if(QT_KNOWN_POLICY_QTP0004)
        qt_policy(SET QTP0004 NEW)
    endif()

    qt_standard_project_setup(REQUIRES 6.6)
endif()

add_subdirectory(src/foundation)
add_subdirectory(src/domain)
add_subdirectory(src/protocol)
add_subdirectory(src/capabilities)
add_subdirectory(src/provider)
add_subdirectory(src/local_state)
add_subdirectory(src/application)
add_subdirectory(src/execution)
add_subdirectory(src/routing)
add_subdirectory(src/sync)
add_subdirectory(src/storage)
add_subdirectory(src/transport)

if(FARCASTER_ENABLE_PROTOCOL_SIGNING)
    add_subdirectory(src/platform)
endif()

if(FARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER)
    add_subdirectory(src/providers/snapchain)
endif()

if(FARCASTER_ENABLE_HYPERSNAP_PROVIDER)
    add_subdirectory(src/providers/hypersnap)
endif()

if(FARCASTER_ENABLE_COMPATIBILITY_PROVIDERS)
    add_subdirectory(src/providers/compatibility/farcaster_auth)
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
