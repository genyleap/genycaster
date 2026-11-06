cmake_minimum_required(VERSION 3.30)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/platform/Android/configure.cmake")
set(_root "${CMAKE_CURRENT_LIST_DIR}/../../build/android-qt-tls-test")
get_filename_component(_root "${_root}" ABSOLUTE)
file(MAKE_DIRECTORY "${_root}/enabled/QtNetwork" "${_root}/disabled/QtNetwork")
file(WRITE "${_root}/enabled/QtNetwork/qtnetwork-config.h" "#define QT_FEATURE_ssl 1\n")
file(WRITE "${_root}/disabled/QtNetwork/qtnetwork-config.h" "#define QT_FEATURE_ssl -1\n")

if(DEFINED CASE)
    if(CASE STREQUAL "disabled")
        genycaster_require_android_qt_tls("${_root}/disabled")
    elseif(CASE STREQUAL "missing")
        genycaster_require_android_qt_tls("${_root}/nonexistent")
    endif()
    return()
endif()

genycaster_require_android_qt_tls("${_root}/enabled")
foreach(_case IN ITEMS disabled missing)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -DCASE=${_case} -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE _status
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr)
    if(_status EQUAL 0)
        message(FATAL_ERROR "Missing or disabled SSL passed for case: ${_case}")
    endif()
    if(NOT _stderr MATCHES "SSL|TLS")
        message(FATAL_ERROR "Wrong SSL diagnostic for ${_case}: ${_stderr}")
    endif()
endforeach()
message(STATUS "Qt Network Android TLS feature gate passed positive/negative tests")
