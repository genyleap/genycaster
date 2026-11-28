cmake_minimum_required(VERSION 3.30)
project(GenycasterAppleVaultFixture LANGUAGES CXX)
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_library(actual_apple_vault STATIC)
target_sources(actual_apple_vault PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${ROOT}/src" FILES
    "${ROOT}/src/foundation/error.cppm"
    "${ROOT}/src/foundation/result.cppm"
    "${ROOT}/src/foundation/time.cppm"
    "${ROOT}/src/domain/identity.cppm"
    "${ROOT}/src/domain/signer.cppm"
    "${ROOT}/src/capabilities/signer_attestation.cppm"
    "${ROOT}/src/capabilities/signer.cppm"
    "${ROOT}/src/platform/signer_vault.cppm"
    PRIVATE "${ROOT}/src/foundation/error.cpp"
            "${ROOT}/src/foundation/time.cpp"
            "${ROOT}/src/platform/signer_vault_macos.cpp")
target_include_directories(actual_apple_vault PUBLIC "${ROOT}/src/platform")
target_compile_features(actual_apple_vault PUBLIC cxx_std_26)
target_link_libraries(actual_apple_vault PUBLIC OpenSSL::Crypto)
add_executable(apple_vault_runtime apple_vault_runtime.cpp)
target_link_libraries(apple_vault_runtime PRIVATE actual_apple_vault)
enable_testing()
add_test(NAME apple_vault_runtime COMMAND apple_vault_runtime)
