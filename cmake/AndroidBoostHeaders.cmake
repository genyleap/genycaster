include_guard(GLOBAL)

if(NOT ANDROID)
    message(FATAL_ERROR "AndroidBoostHeaders.cmake is only valid for Android builds")
endif()

# The adapters embed Boost.JSON's implementation through boost/json/src.hpp.
# They require portable headers, not a host-platform Boost library or config.
# Supplying Boost::headers early also avoids Qt Creator's package-manager
# interceptor resolving find_package(Boost) to an incompatible host package.
if(TARGET Boost::headers)
    return()
endif()

set(GENYCASTER_ANDROID_BOOST_INCLUDE_DIR "" CACHE PATH
    "Optional Boost headers root for Android (containing boost/json/src.hpp)")

if(NOT GENYCASTER_ANDROID_BOOST_INCLUDE_DIR)
    # Host Boost *headers* can be reused when cross-compiling; its binaries
    # must never be linked. A pinned source archive is the fallback.
    find_path(_genycasterAndroidBoostHostHeaders
        NAMES boost/json/src.hpp
        HINTS /opt/homebrew/opt/boost/include /opt/homebrew/include
              /usr/local/include
        NO_CMAKE_FIND_ROOT_PATH
    )
    if(_genycasterAndroidBoostHostHeaders
            AND EXISTS "${_genycasterAndroidBoostHostHeaders}/boost/config.hpp"
            AND EXISTS "${_genycasterAndroidBoostHostHeaders}/boost/version.hpp")
        file(STRINGS "${_genycasterAndroidBoostHostHeaders}/boost/version.hpp"
            _genycasterBoostVersionLine
            REGEX "^#[ \t]*define BOOST_VERSION[ \t]+[0-9]+")
        if(_genycasterBoostVersionLine MATCHES "BOOST_VERSION[ \t]+([0-9]+)"
                AND CMAKE_MATCH_1 GREATER_EQUAL 107500)
            set(GENYCASTER_ANDROID_BOOST_INCLUDE_DIR
                "${_genycasterAndroidBoostHostHeaders}")
        endif()
    endif()
endif()

if(NOT GENYCASTER_ANDROID_BOOST_INCLUDE_DIR)
    # Pin the official complete header archive; it is downloaded only when
    # suitable host headers are unavailable. Never build native Boost binaries.
    include(FetchContent)
    FetchContent_Declare(genycaster_android_boost
        URL "https://archives.boost.io/release/1.89.0/source/boost_1_89_0.tar.bz2"
        URL_HASH "SHA256=85a33fa22621b4f314f8e85e1a5e2a9363d22e4f4992925d4bb3bc631b5a0c7a"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR "__genycaster_headers_only__"
    )
    FetchContent_MakeAvailable(genycaster_android_boost)
    set(GENYCASTER_ANDROID_BOOST_INCLUDE_DIR "${genycaster_android_boost_SOURCE_DIR}")
    if(NOT EXISTS "${GENYCASTER_ANDROID_BOOST_INCLUDE_DIR}/boost/json/src.hpp")
        set(GENYCASTER_ANDROID_BOOST_INCLUDE_DIR
            "${genycaster_android_boost_SOURCE_DIR}/boost_1_89_0")
    endif()
endif()

foreach(_genycasterRequiredBoostHeader IN ITEMS
        boost/json.hpp boost/json/src.hpp boost/config.hpp boost/version.hpp)
    if(NOT EXISTS
            "${GENYCASTER_ANDROID_BOOST_INCLUDE_DIR}/${_genycasterRequiredBoostHeader}")
        message(FATAL_ERROR
            "Android Boost headers incomplete: ${_genycasterRequiredBoostHeader} "
            "not found under '${GENYCASTER_ANDROID_BOOST_INCLUDE_DIR}'.")
    endif()
endforeach()

add_library(genycaster_android_boost_headers INTERFACE)
add_library(Boost::headers ALIAS genycaster_android_boost_headers)
target_include_directories(genycaster_android_boost_headers SYSTEM INTERFACE
    "${GENYCASTER_ANDROID_BOOST_INCLUDE_DIR}")
message(STATUS
    "Genycaster Android portable Boost headers: ${GENYCASTER_ANDROID_BOOST_INCLUDE_DIR}")
