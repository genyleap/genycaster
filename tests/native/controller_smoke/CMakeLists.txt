cmake_minimum_required(VERSION 3.28)
project(GenyCasterControllerSmoke LANGUAGES CXX)

# Compile-only gate for network controllers whose request cancellation
# can synchronously emit finished. Uses production sources, without the
# full C++26 module or QML application graph.
find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Network)
get_filename_component(_project_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
add_library(genycaster_controller_smoke OBJECT
    "${_project_root}/ui/app/identity/browser_auth.cpp"
    "${_project_root}/ui/app/settings/providers.cpp"
    "${_project_root}/ui/app/social/channels.cpp"
    "${_project_root}/ui/app/update/controller.cpp"
)
set_target_properties(genycaster_controller_smoke PROPERTIES
    AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_compile_features(genycaster_controller_smoke PRIVATE cxx_std_26)
target_link_libraries(genycaster_controller_smoke PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network)
target_compile_definitions(genycaster_controller_smoke PRIVATE
    GENYCASTER_PRODUCT_NAME=\"Genycaster\"
    GENYCASTER_VERSION_NUMBER=\"1.0.0\"
    GENYCASTER_RELEASE_CHANNEL=\"rc\"
    GENYCASTER_RELEASE_CHANNEL_NUMBER=\"1\")
target_compile_options(genycaster_controller_smoke PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow)
