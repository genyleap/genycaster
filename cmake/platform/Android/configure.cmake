# Android: one configuration file, grouped by toolchain, test, transport and dependencies.
# All entrypoints are only invoked by Android's composition path.

# --- Toolchain language ---
if(ANDROID)
    # The bundled SQLite amalgamation is C and must use the Android NDK toolchain.
    # Native desktop builds continue to use their existing SQLite integration.
    enable_language(C)
endif()

# --- OpenSSL root normalization (script-mode testable) ---
# Normalize Qt Creator's cached Android OpenSSL prefix.
#
# A PATH cache entry entered as "Users/name/..." (without leading slash) is
# silently absolutized by CMake relative to the checkout. For a macOS
# checkout under /Users/name/project this becomes
# /Users/name/project/Users/name/... . Repair only this recognizable case,
# and only if a complete target-ABI OpenSSL prefix exists at the candidate.
function(genycaster_resolve_android_openssl_root output requested source_root)
    if(IS_ABSOLUTE "${requested}")
        set(_root "${requested}")
    else()
        get_filename_component(_root "${requested}" ABSOLUTE
            BASE_DIR "${source_root}")
    endif()
    cmake_path(NORMAL_PATH _root)

    set(_source_prefix "${source_root}/")
    string(FIND "${_root}" "${_source_prefix}" _source_position)
    if(_source_position EQUAL 0
            AND NOT EXISTS "${_root}/include/openssl/opensslv.h")
        string(LENGTH "${_source_prefix}" _prefix_length)
        string(SUBSTRING "${_root}" ${_prefix_length} -1 _relative_tail)

        # These are known Unix absolute path roots, not an attempt
        # to reinterpret arbitrary relative paths as a host SDK location.
        if(_relative_tail MATCHES "^(Users|home|opt|tmp|var|mnt)/")
            set(_candidate "/${_relative_tail}")
            if(EXISTS "${_candidate}/include/openssl/opensslv.h"
                    AND EXISTS "${_candidate}/lib/libcrypto_3.so"
                    AND EXISTS "${_candidate}/lib/libssl_3.so")
                message(WARNING
                    "Corrected GENYCASTER_ANDROID_OPENSSL_ROOT from "
                    "'${_root}' to '${_candidate}'. "
                    "Update the Qt Creator Release kit CMake variable to "
                    "the absolute path with its leading '/'.")
                set(_root "${_candidate}")
                get_property(_cache_help CACHE GENYCASTER_ANDROID_OPENSSL_ROOT
                    PROPERTY HELPSTRING)
                set(GENYCASTER_ANDROID_OPENSSL_ROOT "${_root}" CACHE PATH
                    "${_cache_help}" FORCE)
            else()
                message(FATAL_ERROR
                    "GENYCASTER_ANDROID_OPENSSL_ROOT appears to have lost its "
                    "leading '/': '${_root}'. "
                    "Set the Qt Creator Release CMake variable to '${_candidate}'. "
                    "Its include/ and lib/ files must contain the Android "
                    "arm64-v8a OpenSSL libraries (provisioned by CI).")
            endif()
        endif()
    endif()

    set(${output} "${_root}" PARENT_SCOPE)
endfunction()

# --- Network TLS requirements (script-mode testable) ---
# Fail closed for an Android Qt Network SDK compiled without SSL APIs.
# Dynamic OpenSSL libraries bundled in the APK cannot enable QT_FEATURE_ssl
# in a QtNetwork binary where this feature was disabled at build time.
function(genycaster_require_android_qt_tls qt_network_includes)
    set(_config "")
    foreach(_include IN LISTS qt_network_includes)
        if(EXISTS "${_include}/qtnetwork-config.h")
            set(_config "${_include}/qtnetwork-config.h")
            break()
        endif()
        if(EXISTS "${_include}/QtNetwork/qtnetwork-config.h")
            set(_config "${_include}/QtNetwork/qtnetwork-config.h")
            break()
        endif()
    endforeach()

    if(_config STREQUAL "")
        message(FATAL_ERROR
            "Cannot verify the Android Qt Network SSL feature: "
            "qtnetwork-config.h was not found in Qt6Network_INCLUDE_DIRS. "
            "Use a complete Qt 6.12 Android kit with TLS support.")
    endif()

    file(STRINGS "${_config}" _ssl_features
        REGEX "^#define[ \t]+QT_FEATURE_ssl[ \t]+[-0-9]+")
    if(NOT _ssl_features MATCHES
            "^#define[ \t]+QT_FEATURE_ssl[ \t]+1([ \t]|$)")
        message(FATAL_ERROR
            "This Qt Android SDK was built without SSL/TLS support: "
            "${_config} contains '${_ssl_features}'. "
            "GenyCaster requires HTTPS for its Farcaster providers. "
            "Install or rebuild a Qt 6.12 Android kit with QT_FEATURE_ssl=1. "
            "Bundling OpenSSL libssl_3.so and libcrypto_3.so does not "
            "enable disabled QtNetwork SSL APIs.")
    endif()

    message(STATUS "Verified Android Qt Network supports SSL: ${_config}")
endfunction()


# --- Android test policy ---
macro(genycaster_android_apply_test_policy)
    if(ANDROID)
        # Old Qt Creator build directories can retain FARCASTER_BUILD_TESTS=ON
        # from desktop profiles. Building the APK must not inherit those tests.
        # Dedicated Android test configurations can explicitly opt back in.
        option(GENYCASTER_ANDROID_ENABLE_TESTS
            "Explicitly allow unit/contract test targets in Android cross-builds" OFF)
        if(NOT GENYCASTER_ANDROID_ENABLE_TESTS AND FARCASTER_BUILD_TESTS)
            message(STATUS
                "Genycaster Android: disabling cached FARCASTER_BUILD_TESTS=ON "
                "for the app build; opt in with GENYCASTER_ANDROID_ENABLE_TESTS=ON.")
            set(FARCASTER_BUILD_TESTS OFF CACHE BOOL
                "Build unit, contract, integration and architecture tests" FORCE)
        endif()
    endif()
endmacro()



# --- NDK threading and ELF page alignment ---
macro(genycaster_android_apply_toolchain_options)
    if(ANDROID)
        # Qt for Android compiles its consumers with -pthread. Clang named-module
        # BMIs/PCMs must use the same POSIX threading mode as their importers;
        # otherwise imports of Qt-free modules (for example storage_policy) fail
        # with a PCH configuration mismatch. Apply the mode before any targets
        # are created so all C++ module producers and consumers agree.
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-pthread>)

        # NDK r27 retains a 4 KiB ELF page-alignment default for some shared
        # module targets. Android 15+ devices can run with 16 KiB kernel pages:
        # every first-party .so must be loadable there, not only the Qt libraries.
        # The flags are additive on existing 4 KiB devices and apply before the
        # provider and QtKeychain targets are created.
        add_link_options(
            "-Wl,-z,max-page-size=16384"
            "-Wl,-z,common-page-size=16384")
    endif()
endmacro()



# --- ABI-matched OpenSSL libraries ---
macro(genycaster_android_setup_openssl)
    # Android builds must link a target-ABI OpenSSL, never the Homebrew/macOS library.
    # Create imported targets before the provider and signer subdirectories are added,
    # so Qt Creator's package-provider shim cannot redirect FindOpenSSL to a host SDK.
    if(NOT ANDROID)
        message(FATAL_ERROR "Android OpenSSL setup is only valid for Android builds")
    endif()

    set(GENYCASTER_ANDROID_OPENSSL_ROOT "" CACHE PATH
        "Android ABI-specific OpenSSL prefix containing include/ and lib/lib{crypto,ssl}_3.so")

    if(NOT GENYCASTER_ANDROID_OPENSSL_ROOT)
        message(FATAL_ERROR
            "Genycaster Android requires an OpenSSL 3.5.9+ arm64-v8a prefix. "
            "GitHub Actions provisions and verifies the target libraries directly "
            "in its Android job. For manual cross-builds set "
            "-DGENYCASTER_ANDROID_OPENSSL_ROOT=<prepared Android prefix>; "
            "never use host macOS/Homebrew OpenSSL.")
    endif()

    if(NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        message(FATAL_ERROR
            "Genycaster's Android OpenSSL packaging is currently verified only for "
            "arm64-v8a. Requested ABI: '${CMAKE_ANDROID_ARCH_ABI}'.")
    endif()

    # Qt Creator may pass a macOS absolute path without its leading '/'. CMake
    # turns that relative CACHE PATH into e.g. <source>/Users/... and persists it
    # in the Release kit. Recover only when the absolute candidate actually has
    # the required target-ABI artifacts; do not guess or fall back to host OpenSSL.
    genycaster_resolve_android_openssl_root(_genycasterAndroidSslRoot
        "${GENYCASTER_ANDROID_OPENSSL_ROOT}" "${CMAKE_SOURCE_DIR}")
    set(_genycasterAndroidSslHeader
        "${_genycasterAndroidSslRoot}/include/openssl/opensslv.h")
    set(_genycasterAndroidCrypto
        "${_genycasterAndroidSslRoot}/lib/libcrypto_3.so")
    set(_genycasterAndroidSsl
        "${_genycasterAndroidSslRoot}/lib/libssl_3.so")
    foreach(_genycasterAndroidFile IN ITEMS
            "${_genycasterAndroidSslHeader}"
            "${_genycasterAndroidCrypto}"
            "${_genycasterAndroidSsl}")
        if(NOT EXISTS "${_genycasterAndroidFile}")
            message(FATAL_ERROR
                "Missing Android OpenSSL artifact: ${_genycasterAndroidFile}\n"
                "The Android arm64-v8a OpenSSL prefix has not been prepared, "
                "or the previous cross-build was incomplete.\n"
                "GitHub Actions provisions the target ABI libraries in the "
                "Android CI job; a manual cross-build requires an already prepared "
                "Android OpenSSL prefix. Confirm these files exist under "
                "'${_genycasterAndroidSslRoot}':\n"
                "  include/openssl/opensslv.h\n"
                "  lib/libcrypto_3.so\n"
                "  lib/libssl_3.so\n"
                "Never use the host macOS/Homebrew OpenSSL libraries for Android.")
        endif()
    endforeach()

    file(STRINGS "${_genycasterAndroidSslHeader}" _genycasterAndroidSslVersionLines
        REGEX "^#[ \t]*define OPENSSL_VERSION_(MAJOR|MINOR|PATCH)[ \t]+[0-9]+")
    set(_genycasterAndroidSslVersionParts "")
    foreach(_genycasterAndroidSslPart IN ITEMS MAJOR MINOR PATCH)
        set(_genycasterAndroidSslPartValue "")
        foreach(_genycasterAndroidSslVersionLine IN LISTS _genycasterAndroidSslVersionLines)
            if(_genycasterAndroidSslVersionLine MATCHES
                    "^#[ \t]*define OPENSSL_VERSION_${_genycasterAndroidSslPart}[ \t]+([0-9]+)")
                set(_genycasterAndroidSslPartValue "${CMAKE_MATCH_1}")
            endif()
        endforeach()
        if(_genycasterAndroidSslPartValue STREQUAL "")
            message(FATAL_ERROR "Cannot read OpenSSL version from ${_genycasterAndroidSslHeader}")
        endif()
        list(APPEND _genycasterAndroidSslVersionParts "${_genycasterAndroidSslPartValue}")
    endforeach()
    list(JOIN _genycasterAndroidSslVersionParts "." _genycasterAndroidSslVersion)
    if(_genycasterAndroidSslVersion VERSION_LESS "3.5.9")
        message(FATAL_ERROR
            "Android OpenSSL ${_genycasterAndroidSslVersion} is too old; "
            "Genycaster requires 3.5.9 or newer.")
    endif()

    add_library(OpenSSL::Crypto SHARED IMPORTED GLOBAL)
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION "${_genycasterAndroidCrypto}"
        INTERFACE_INCLUDE_DIRECTORIES "${_genycasterAndroidSslRoot}/include")
    add_library(OpenSSL::SSL SHARED IMPORTED GLOBAL)
    set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LOCATION "${_genycasterAndroidSsl}"
        INTERFACE_INCLUDE_DIRECTORIES "${_genycasterAndroidSslRoot}/include"
        INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)

    set(GENYCASTER_ANDROID_OPENSSL_CRYPTO_LIBRARY "${_genycasterAndroidCrypto}")
    set(GENYCASTER_ANDROID_OPENSSL_SSL_LIBRARY "${_genycasterAndroidSsl}")
    message(STATUS
        "Genycaster Android OpenSSL: ${_genycasterAndroidSslVersion} "
        "(${CMAKE_ANDROID_ARCH_ABI}), ${_genycasterAndroidSslRoot}")
endmacro()



# --- Portable Boost headers ---
function(genycaster_android_setup_boost_headers)
    if(NOT ANDROID)
        message(FATAL_ERROR "Android Boost setup is only valid for Android builds")
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
endfunction()

# Shared Android SDKs do not always export an imported CMake target for a
# dynamically loaded Qt plugin. Probe the selected target SDK, never the
# desktop Qt installation or another ABI.
function(genycaster_android_find_sdk_plugin output sdk_root type name abi)
    set(_plugin "")
    foreach(_candidate IN ITEMS
        "${sdk_root}/plugins/${type}/libplugins_${type}_${name}_${abi}.so"
        "${sdk_root}/plugins/${type}/lib${name}_${abi}.so")
        if(EXISTS "${_candidate}" AND NOT IS_DIRECTORY "${_candidate}")
            set(_plugin "${_candidate}")
            break()
        endif()
    endforeach()
    set(${output} "${_plugin}" PARENT_SCOPE)
endfunction()

function(genycaster_android_configure_media_backend target)
    option(GENYCASTER_REQUIRE_ANDROID_FFMPEG
        "Require FFmpeg media plugin in Android SDK (mandatory in CI)" OFF)
    # Qt6Multimedia_DIR belongs to the target kit, even during cross compile.
    get_filename_component(_sdk "${Qt6Multimedia_DIR}/../../.." ABSOLUTE)
    genycaster_android_find_sdk_plugin(_binary "${_sdk}" multimedia ffmpegmediaplugin "${ANDROID_ABI}")
    if(NOT TARGET Qt6::QFFmpegMediaPlugin
       AND EXISTS "${Qt6Multimedia_DIR}/Qt6QFFmpegMediaPluginConfig.cmake")
        find_package(Qt6QFFmpegMediaPlugin QUIET CONFIG
            PATHS "${Qt6Multimedia_DIR}" NO_DEFAULT_PATH)
    endif()
    if(TARGET Qt6::QFFmpegMediaPlugin)
        set(genycaster_android_media_plugins INCLUDE_BY_TYPE multimedia Qt6::QFFmpegMediaPlugin PARENT_SCOPE)
        message(STATUS "Android FFmpeg: Qt plugin target available")
    elseif(_binary)
        # Qt documents this deployment property for dynamically loaded plugins.
        # Stage just this ABI's plugin with the Android naming convention.
        set(_stage "${CMAKE_CURRENT_BINARY_DIR}/android-extra-plugins")
        file(MAKE_DIRECTORY "${_stage}/multimedia")
        configure_file("${_binary}"
            "${_stage}/multimedia/libplugins_multimedia_ffmpegmediaplugin_${ANDROID_ABI}.so" COPYONLY)
        set_property(TARGET ${target} APPEND PROPERTY QT_ANDROID_EXTRA_PLUGINS "${_stage}")
        message(STATUS "Android FFmpeg: using SDK plugin file ${_binary}")
    else()
        if(GENYCASTER_REQUIRE_ANDROID_FFMPEG)
            message(FATAL_ERROR "Android FFmpeg plugin missing from target Qt SDK: ${_sdk}. Install a Qt Multimedia SDK with FFmpeg support.")
        endif()
        message(WARNING "Android Qt SDK has no discoverable FFmpeg plugin. Local build may continue, but media playback will remain unavailable unless FFmpeg can load; unsafe native fallback is disabled.")
    endif()
endfunction()
