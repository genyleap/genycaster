cmake_minimum_required(VERSION 3.30)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/platform/Android/configure.cmake")

set(_fixture "${CMAKE_CURRENT_LIST_DIR}/../../build/android-openssl-path-test")
get_filename_component(_fixture "${_fixture}" ABSOLUTE)
set(_project "${_fixture}/project")
set(_prefix "${_fixture}/android-deps")
file(MAKE_DIRECTORY "${_project}" "${_prefix}/include/openssl" "${_prefix}/lib")
file(WRITE "${_prefix}/include/openssl/opensslv.h" "/* fixture */\n")
file(WRITE "${_prefix}/lib/libcrypto_3.so" "mock")
file(WRITE "${_prefix}/lib/libssl_3.so" "mock")

# CMake CACHE PATH turns a missing-leading-slash 'opt/...' into
# <project>/opt/..., reproducing the reported Qt Creator Release failure.
string(SUBSTRING "${_prefix}" 1 -1 _without_slash)
set(_broken "${_project}/${_without_slash}")
set(GENYCASTER_ANDROID_OPENSSL_ROOT "${_broken}" CACHE PATH "Test prefix" FORCE)
genycaster_resolve_android_openssl_root(_actual
    "${GENYCASTER_ANDROID_OPENSSL_ROOT}" "${_project}")
if(NOT _actual STREQUAL _prefix)
    message(FATAL_ERROR "Did not repair leading-slash path: '${_actual}'")
endif()
if(NOT GENYCASTER_ANDROID_OPENSSL_ROOT STREQUAL _prefix)
    message(FATAL_ERROR "Did not update cached OpenSSL root")
endif()

# Correct absolute path remains untouched and is accepted.
genycaster_resolve_android_openssl_root(_absolute "${_prefix}" "${_project}")
if(NOT _absolute STREQUAL _prefix)
    message(FATAL_ERROR "Changed a valid absolute path")
endif()

# Genuine project-relative prefixes must resolve within the project.
genycaster_resolve_android_openssl_root(_relative "vendor/openssl" "${_project}")
if(NOT _relative STREQUAL "${_project}/vendor/openssl")
    message(FATAL_ERROR "Changed a project-relative path: '${_relative}'")
endif()

message(STATUS "Android OpenSSL root path resolution tests passed")
