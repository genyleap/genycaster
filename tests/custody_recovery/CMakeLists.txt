cmake_minimum_required(VERSION 3.30)
project(GenyCasterNativeCustodyVectors LANGUAGES CXX)
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)

add_library(native_custody_vectors_support STATIC)
target_sources(native_custody_vectors_support
    PUBLIC FILE_SET CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../src/platform"
        FILES ../../src/platform/custody_recovery.cppm
    PRIVATE
        ../../src/platform/custody_recovery.cpp
        ../../src/platform/custody_bip39_english.hpp)
target_compile_features(native_custody_vectors_support PUBLIC cxx_std_26)
target_link_libraries(native_custody_vectors_support PRIVATE OpenSSL::Crypto)

add_executable(genycaster_custody_vectors vectors.cpp)
target_compile_features(genycaster_custody_vectors PRIVATE cxx_std_26)
target_link_libraries(genycaster_custody_vectors PRIVATE native_custody_vectors_support)
enable_testing()
add_test(NAME custody.bip39_bip32_vectors COMMAND genycaster_custody_vectors)
