# Dedicated QtCore-only test: validates untrusted swapToken inputs without
# importing the application, a wallet, a network service or the UI runtime.
cmake_minimum_required(VERSION 3.24)
project(genycaster_swap_intent_contract LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Network)
set(CMAKE_AUTOMOC ON)
add_executable(swap_intent_contract swap_intent_contract.cpp)
target_link_libraries(swap_intent_contract PRIVATE Qt6::Core)
enable_testing()
add_test(NAME swap_intent_contract COMMAND swap_intent_contract)
add_executable(swap_quote_contract swap_quote_contract.cpp)
target_link_libraries(swap_quote_contract PRIVATE Qt6::Core)
add_test(NAME swap_quote_contract COMMAND swap_quote_contract)
# Compile the real QObject swap controller (including moc), not only contracts.
add_library(swap_host_compile STATIC
    ../../ui/app/miniapp/host.cpp
    ../../ui/app/miniapp/host.hpp)
target_link_libraries(swap_host_compile PRIVATE Qt6::Core Qt6::Gui Qt6::Network)
target_compile_features(swap_host_compile PRIVATE cxx_std_26)

# The main Linux diagnostic build disables native WebEngine, so it cannot
# compile the Mini App runtime. This isolated module build compiles the real
# C++26 runtime implementation against the abstract browser interface and Qt.
# Browser factory symbols remain unresolved in this static library by design.
add_library(swap_runtime_compile STATIC)
target_sources(swap_runtime_compile
    PUBLIC FILE_SET CXX_MODULES BASE_DIRS ${CMAKE_SOURCE_DIR}/../.. FILES
        ../../src/foundation/error.cppm
        ../../src/foundation/result.cppm
        ../../src/capabilities/wallet.cppm
        ../../ui/app/miniapp/runtime.cppm
    PRIVATE ../../ui/app/miniapp/runtime.cpp)
set_property(TARGET swap_runtime_compile PROPERTY CXX_SCAN_FOR_MODULES ON)
target_compile_definitions(swap_runtime_compile PRIVATE GENYCASTER_MINIAPP_SIWF=0)
target_link_libraries(swap_runtime_compile PRIVATE Qt6::Core Qt6::Network)
target_compile_features(swap_runtime_compile PRIVATE cxx_std_26)
# This is a syntax/type/linkage smoke target, not an optimized app binary.
# Avoid GCC 16 module debug/optimizer memory spikes on CI runners.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(swap_runtime_compile PRIVATE -O0 -g0)
endif()

# Compile the authenticated Qt account quote adapter when the full
# repository's generated deployment header is available. The CI-only miniapp
# smoke project is standalone, so do not require a secret or generated header
# on otherwise-clean CI worktrees.
set(_swap_deployment_header
    "${CMAKE_SOURCE_DIR}/../../build/registration-full-verify/generated")
if(EXISTS "${_swap_deployment_header}/genycaster_deployment.hpp")
    find_package(Qt6 6.12 REQUIRED COMPONENTS Concurrent)
    add_library(swap_identity_compile STATIC
        ../../ui/app/identity/connections.cpp
        ../../ui/app/identity/connections.hpp)
    target_include_directories(swap_identity_compile PRIVATE
        "${_swap_deployment_header}")
    target_link_libraries(swap_identity_compile PRIVATE
        Qt6::Core Qt6::Gui Qt6::Network Qt6::Concurrent)
    target_compile_features(swap_identity_compile PRIVATE cxx_std_26)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(swap_identity_compile PRIVATE -O0 -g0)
    endif()
endif()
