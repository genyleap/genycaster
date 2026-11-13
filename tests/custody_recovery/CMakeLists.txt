cmake_minimum_required(VERSION 3.30)
project(GenyCasterNativeCustodyVectors LANGUAGES CXX)
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)

# Reuse the real domain/capability/protocol modules now required by the
# ephemeral EIP-712 custody authorizer, rather than stubbing signature types.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
include(ProjectWarnings)
add_subdirectory("../../src/foundation" "${CMAKE_BINARY_DIR}/native-foundation")
add_subdirectory("../../src/domain" "${CMAKE_BINARY_DIR}/native-domain")
add_subdirectory("../../src/capabilities" "${CMAKE_BINARY_DIR}/native-capabilities")
add_subdirectory("../../src/protocol" "${CMAKE_BINARY_DIR}/native-protocol")

add_library(native_custody_vectors_support STATIC)
target_sources(native_custody_vectors_support
    PUBLIC FILE_SET CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../src/platform"
        FILES ../../src/platform/custody_recovery.cppm
    PRIVATE
        ../../src/platform/custody_recovery.cpp
        ../../src/platform/custody_bip39_english.hpp)
target_compile_features(native_custody_vectors_support PUBLIC cxx_std_26)
target_link_libraries(native_custody_vectors_support
    PUBLIC farcaster::capabilities farcaster::protocol
    PRIVATE OpenSSL::Crypto)

add_executable(genycaster_custody_vectors vectors.cpp)
target_compile_features(genycaster_custody_vectors PRIVATE cxx_std_26)
target_link_libraries(genycaster_custody_vectors PRIVATE native_custody_vectors_support)
enable_testing()
add_test(NAME custody.bip39_bip32_vectors COMMAND genycaster_custody_vectors)

# A second, independent implementation must recover the same public address
# from the signed native EIP-191 proof. Never test only the code that signed it.
find_program(GENYCASTER_NODE_EXECUTABLE node REQUIRED)
add_executable(genycaster_custody_proof proof_interop.cpp)
target_compile_features(genycaster_custody_proof PRIVATE cxx_std_26)
target_link_libraries(genycaster_custody_proof PRIVATE native_custody_vectors_support)
add_test(NAME custody.eip191_noble_interop
    COMMAND "${GENYCASTER_NODE_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/interop.test.mjs"
    "$<TARGET_FILE:genycaster_custody_proof>")

add_executable(genycaster_custody_key_add key_add.cpp)
target_compile_features(genycaster_custody_key_add PRIVATE cxx_std_26)
target_link_libraries(genycaster_custody_key_add PRIVATE native_custody_vectors_support)
add_test(NAME custody.one_shot_key_add COMMAND genycaster_custody_key_add)

add_executable(genycaster_custody_key_add_interop key_add_interop.cpp)
target_compile_features(genycaster_custody_key_add_interop PRIVATE cxx_std_26)
target_link_libraries(genycaster_custody_key_add_interop PRIVATE native_custody_vectors_support)
add_test(NAME custody.eip712_noble_interop
    COMMAND "${GENYCASTER_NODE_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/key_add_interop.test.mjs"
    "$<TARGET_FILE:genycaster_custody_key_add_interop>")

add_executable(genycaster_native_onboard_vectors native_onboard.cpp)
target_compile_features(genycaster_native_onboard_vectors PRIVATE cxx_std_26)
target_link_libraries(genycaster_native_onboard_vectors PRIVATE
    native_custody_vectors_support farcaster::protocol OpenSSL::Crypto)
add_test(NAME custody.hypersnap_native_onboard COMMAND genycaster_native_onboard_vectors)

# Optional native Qt backing-custody smoke. Disabled for the portable
# cryptographic test suite; enabling it uses the same Qt 6.12 already used
# by the main GenyCaster application.
option(GENYCASTER_TEST_NATIVE_QT_WALLET "Build Qt-native custody backup smoke" OFF)
if(GENYCASTER_TEST_NATIVE_QT_WALLET)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Quick Network)
    add_executable(genycaster_native_wallet_backup
        native_wallet_backup.cpp
        ../../ui/app/identity/native_wallet.cpp
        ../../ui/app/identity/native_wallet.hpp)
    set_target_properties(genycaster_native_wallet_backup PROPERTIES AUTOMOC ON)
    target_compile_features(genycaster_native_wallet_backup PRIVATE cxx_std_26)
    target_include_directories(genycaster_native_wallet_backup PRIVATE
        ../../ui/app/identity)
    target_link_libraries(genycaster_native_wallet_backup PRIVATE
        Qt6::Core Qt6::Gui Qt6::Quick native_custody_vectors_support OpenSSL::Crypto)
    add_test(NAME custody.native_wallet_backup COMMAND genycaster_native_wallet_backup)
    set_tests_properties(custody.native_wallet_backup PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# Farcaster onchain IdRegistry signatures are a distinct adapter from the
# HyperSnap NativeOnboard/Hashcash path. Run independently against Noble.
add_executable(genycaster_farcaster_transfer_vectors farcaster_transfer.cpp)
target_compile_features(genycaster_farcaster_transfer_vectors PRIVATE cxx_std_26)
target_link_libraries(genycaster_farcaster_transfer_vectors PRIVATE
    native_custody_vectors_support)
add_test(NAME custody.farcaster_id_registry_transfer
    COMMAND "${GENYCASTER_NODE_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/farcaster_transfer_interop.test.mjs"
    "$<TARGET_FILE:genycaster_farcaster_transfer_vectors>")

add_executable(genycaster_registration_plan registration_plan.cpp)
target_compile_features(genycaster_registration_plan PRIVATE cxx_std_26)
add_test(NAME custody.registration_protocol_isolation
    COMMAND genycaster_registration_plan)

if(GENYCASTER_TEST_NATIVE_QT_WALLET)
    add_executable(genycaster_neynar_payload neynar_payload.cpp)
    target_compile_features(genycaster_neynar_payload PRIVATE cxx_std_26)
    target_link_libraries(genycaster_neynar_payload PRIVATE Qt6::Core)
    add_test(NAME custody.neynar_reserved_fid_payload
        COMMAND genycaster_neynar_payload)
endif()

if(GENYCASTER_TEST_NATIVE_QT_WALLET)
    add_executable(genycaster_hypersnap_transport
        hypersnap_transport.cpp
        ../../ui/app/identity/hypersnap_registration_transport.cpp
        ../../ui/app/identity/hypersnap_registration_transport.hpp
        ../../ui/app/identity/native_registration.cpp
        ../../ui/app/identity/native_registration.hpp
        ../../ui/app/identity/native_publish.cpp
        ../../ui/app/identity/native_publish.hpp)
    set_target_properties(genycaster_hypersnap_transport PROPERTIES AUTOMOC ON)
    target_compile_features(genycaster_hypersnap_transport PRIVATE cxx_std_26)
    target_link_libraries(genycaster_hypersnap_transport PRIVATE
        Qt6::Core Qt6::Network)
    add_test(NAME custody.hypersnap_transport COMMAND genycaster_hypersnap_transport)
endif()

if(GENYCASTER_TEST_NATIVE_QT_WALLET)
    add_executable(genycaster_neynar_flow
        neynar_flow.cpp
        ../../ui/app/identity/neynar_registration_controller.cpp
        ../../ui/app/identity/neynar_registration_controller.hpp)
    set_target_properties(genycaster_neynar_flow PROPERTIES AUTOMOC ON)
    target_compile_features(genycaster_neynar_flow PRIVATE cxx_std_26)
    target_link_libraries(genycaster_neynar_flow PRIVATE Qt6::Core Qt6::Network)
    add_test(NAME custody.neynar_flow COMMAND genycaster_neynar_flow)
endif()

if(GENYCASTER_TEST_NATIVE_QT_WALLET)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Concurrent)
    add_executable(genycaster_native_prepare
        native_prepare.cpp
        ../../ui/app/identity/native_prepare.cpp
        ../../ui/app/identity/native_prepare.hpp)
    set_target_properties(genycaster_native_prepare PROPERTIES AUTOMOC ON)
    target_compile_features(genycaster_native_prepare PRIVATE cxx_std_26)
    target_link_libraries(genycaster_native_prepare PRIVATE
        Qt6::Core Qt6::Concurrent Qt6::Network native_custody_vectors_support)
    add_test(NAME custody.native_prepare COMMAND genycaster_native_prepare)
endif()
