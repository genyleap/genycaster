# Android builds must link a target-ABI OpenSSL, never the Homebrew/macOS library.
# Create imported targets before the provider and signer subdirectories are added,
# so Qt Creator's package-provider shim cannot redirect FindOpenSSL to a host SDK.
if(NOT ANDROID)
    message(FATAL_ERROR "AndroidOpenSSL.cmake is only valid for Android builds")
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
include("${CMAKE_CURRENT_LIST_DIR}/OpenSSLRoot.cmake")
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
