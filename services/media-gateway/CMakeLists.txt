cmake_minimum_required(VERSION 3.30)
project(genyleap_media_gateway_native VERSION 1.2.0 LANGUAGES CXX)

include(FetchContent)

# Quilibrium C++ SDK C++23 BMI compatibility boundary.
# The SDK intentionally requires module consumers to use its C++23 dialect; this isolated
# server-side Node-API addon is not part of the C++26 Farcaster application/core target graph.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(GENYLEAP_QUILIBRIUM_SDK_REVISION
    "3599f81f6284797692dedc096f6ba0f35fe317a2"
    CACHE STRING "Pinned Quilibrium C++ SDK revision")
set(GENYLEAP_QUILIBRIUM_SOURCE_DIR
    ""
    CACHE PATH "Optional local Quilibrium-SDK checkout used instead of FetchContent")
set(GENYLEAP_NODE_INCLUDE_DIR
    ""
    CACHE PATH "Directory containing node_api.h")

set(QUILIBRIUM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(QUILIBRIUM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QUILIBRIUM_BUILD_C_API OFF CACHE BOOL "" FORCE)
set(QUILIBRIUM_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(QUILIBRIUM_ENABLE_LIVE_TESTS OFF CACHE BOOL "" FORCE)

if(GENYLEAP_QUILIBRIUM_SOURCE_DIR)
    add_subdirectory(
        "${GENYLEAP_QUILIBRIUM_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/_deps/quilibrium-sdk-build"
        EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(
        quilibrium_sdk
        GIT_REPOSITORY https://github.com/thecompez/Quilibrium-SDK.git
        GIT_TAG "${GENYLEAP_QUILIBRIUM_SDK_REVISION}"
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(quilibrium_sdk)
endif()

if(NOT GENYLEAP_NODE_INCLUDE_DIR)
    execute_process(
        COMMAND node -p "process.execPath"
        OUTPUT_VARIABLE GENYLEAP_NODE_EXECUTABLE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GENYLEAP_NODE_QUERY_RESULT)
    if(GENYLEAP_NODE_QUERY_RESULT EQUAL 0 AND GENYLEAP_NODE_EXECUTABLE)
        get_filename_component(GENYLEAP_NODE_BIN_DIR "${GENYLEAP_NODE_EXECUTABLE}" DIRECTORY)
        get_filename_component(GENYLEAP_NODE_PREFIX "${GENYLEAP_NODE_BIN_DIR}" DIRECTORY)
        set(GENYLEAP_NODE_INCLUDE_CANDIDATE "${GENYLEAP_NODE_PREFIX}/include/node")
        if(EXISTS "${GENYLEAP_NODE_INCLUDE_CANDIDATE}/node_api.h")
            set(GENYLEAP_NODE_INCLUDE_DIR "${GENYLEAP_NODE_INCLUDE_CANDIDATE}")
        endif()
    endif()
endif()

if(NOT EXISTS "${GENYLEAP_NODE_INCLUDE_DIR}/node_api.h")
    message(FATAL_ERROR
        "node_api.h was not found. Set GENYLEAP_NODE_INCLUDE_DIR to the active Node.js include/node directory.")
endif()

add_library(genyleap_quilibrium_storage MODULE)
target_sources(genyleap_quilibrium_storage PRIVATE native/quilibrium_storage_addon.cpp)
target_compile_features(genyleap_quilibrium_storage PRIVATE cxx_std_23)
target_include_directories(genyleap_quilibrium_storage PRIVATE "${GENYLEAP_NODE_INCLUDE_DIR}")
target_link_libraries(genyleap_quilibrium_storage PRIVATE Quilibrium::SDK)
set_property(TARGET genyleap_quilibrium_storage PROPERTY CXX_SCAN_FOR_MODULES ON)
set_target_properties(genyleap_quilibrium_storage PROPERTIES
    PREFIX ""
    SUFFIX ".node")

if(MSVC)
    target_compile_options(genyleap_quilibrium_storage PRIVATE /W4 /permissive-)
else()
    target_compile_options(genyleap_quilibrium_storage PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow)
endif()

if(APPLE)
    target_link_options(genyleap_quilibrium_storage PRIVATE "LINKER:-undefined,dynamic_lookup")
endif()
