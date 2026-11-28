cmake_minimum_required(VERSION 3.24)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(GenycasterRasterCacheFixture LANGUAGES CXX)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui)
    enable_testing()
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(provider "${CMAKE_CURRENT_LIST_DIR}/../../ui/app/media/remote_image.cpp")
set(generated "${CMAKE_CURRENT_BINARY_DIR}/raster_cache_fixture.inc")
add_custom_command(OUTPUT "${generated}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_cache.py"
        "${provider}" "${generated}"
    DEPENDS "${provider}" "${CMAKE_CURRENT_LIST_DIR}/extract_cache.py"
    VERBATIM)
add_executable(genycaster_raster_cache_fixture main.cpp "${generated}")
target_include_directories(genycaster_raster_cache_fixture PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(genycaster_raster_cache_fixture PRIVATE cxx_std_23)
target_link_libraries(genycaster_raster_cache_fixture PRIVATE Qt6::Core Qt6::Gui)
add_test(NAME raster_cache_fixture COMMAND genycaster_raster_cache_fixture)
set_tests_properties(raster_cache_fixture PROPERTIES TIMEOUT 15 LABELS "ui;media;cache")
