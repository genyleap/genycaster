cmake_minimum_required(VERSION 3.30)
project(GenyCasterIconRasterTest LANGUAGES CXX)
find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Quick Svg)
qt_standard_project_setup(REQUIRES 6.12)
enable_testing()

add_executable(genycaster_android_icon_raster_test main.cpp)
target_compile_features(genycaster_android_icon_raster_test PRIVATE cxx_std_20)
target_include_directories(genycaster_android_icon_raster_test PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../ui/mobile")
target_link_libraries(genycaster_android_icon_raster_test PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Svg)
qt_add_resources(genycaster_android_icon_raster_test genycaster_icon_test
    PREFIX "/qt/qml/Farcaster/Client/assets/icons/genyleap/icons"
    BASE "${CMAKE_CURRENT_LIST_DIR}/../../ui/assets/icons/genyleap/icons"
    FILES
        "${CMAKE_CURRENT_LIST_DIR}/../../ui/assets/icons/genyleap/icons/system/check.svg"
        "${CMAKE_CURRENT_LIST_DIR}/../../ui/assets/icons/genyleap/icons/brands/geny.svg")
add_test(NAME icon.alpha_and_tint COMMAND genycaster_android_icon_raster_test)
set_tests_properties(icon.alpha_and_tint PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
