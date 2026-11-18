# Dedicated QtCore-only test: validates untrusted swapToken inputs without
# importing the application, a wallet, a network service or the UI runtime.
cmake_minimum_required(VERSION 3.24)
project(genycaster_swap_intent_contract LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.12 REQUIRED COMPONENTS Core)
add_executable(swap_intent_contract swap_intent_contract.cpp)
target_link_libraries(swap_intent_contract PRIVATE Qt6::Core)
enable_testing()
add_test(NAME swap_intent_contract COMMAND swap_intent_contract)
