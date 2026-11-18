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
