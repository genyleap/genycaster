cmake_minimum_required(VERSION 3.30)
project(GenyCasterMiniAppHostContracts LANGUAGES CXX)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.12 REQUIRED COMPONENTS Core Network Gui)
enable_testing()

add_executable(miniapp_host_contracts
    test_main.cpp
    ../../ui/app/miniapp_host_controller.cpp
    ../../ui/app/miniapp_host_controller.hpp
)
target_compile_features(miniapp_host_contracts PRIVATE cxx_std_26)
target_link_libraries(miniapp_host_contracts PRIVATE Qt6::Core Qt6::Network Qt6::Gui)
add_test(NAME miniapp.host.security COMMAND miniapp_host_contracts)
