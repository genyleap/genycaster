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
