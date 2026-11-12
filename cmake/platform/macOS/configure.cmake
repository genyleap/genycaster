# macOS: one configuration file for bundle normalization and native SDK composition.
# In script mode this file performs only the explicitly requested bundle operation.
if(CMAKE_SCRIPT_MODE_FILE)
    # Normalize a pre-existing macOS bundle's physical directory spelling.
    #
    # OUTPUT_NAME=Genycaster already produces the canonical link path. On macOS
    # case-insensitive volumes, an older genycaster.app directory may remain on
    # disk under its original lowercase spelling even after rebuilding into the
    # new spelling. A two-step rename is required for a reliable case-only change.
    #
    # This script runs only POST_BUILD for a macOS .app target, not Android, iOS
    # or WebAssembly. It changes no executable, entitlements, identifiers or data.
    if(NOT DEFINED GENYCASTER_BUNDLE_PATH)
        message(FATAL_ERROR "GENYCASTER_BUNDLE_PATH is required")
    endif()

    get_filename_component(_bundle_name "${GENYCASTER_BUNDLE_PATH}" NAME)
    if(NOT _bundle_name STREQUAL "Genycaster.app")
        message(FATAL_ERROR "Unexpected GenyCaster bundle path: ${GENYCASTER_BUNDLE_PATH}")
    endif()

    get_filename_component(_parent "${GENYCASTER_BUNDLE_PATH}" DIRECTORY)
    file(GLOB _actual_bundles LIST_DIRECTORIES TRUE RELATIVE "${_parent}" "${_parent}/*.app")
    list(FIND _actual_bundles "genycaster.app" _legacy_index)
    if(_legacy_index EQUAL -1)
    return()
endif()

list(FIND _actual_bundles "Genycaster.app" _canonical_index)
if(NOT _canonical_index EQUAL -1)
    # On case-sensitive volumes, both names are distinct. Never replace a
    # valid canonical bundle with a stale legacy one.
    message(STATUS "Genycaster.app exists; legacy genycaster.app is left untouched")
    return()
endif()

set(_legacy "${_parent}/genycaster.app")
set(_temporary "${_parent}/.Genycaster-case-rename.app")
if(EXISTS "${_temporary}" OR IS_SYMLINK "${_temporary}")
    message(FATAL_ERROR "Cannot normalize bundle; temporary path already exists: ${_temporary}")
endif()

file(RENAME "${_legacy}" "${_temporary}" RESULT _first_result)
if(NOT _first_result STREQUAL "0")
    message(FATAL_ERROR "Cannot rename legacy macOS bundle: ${_first_result}")
endif()

file(RENAME "${_temporary}" "${GENYCASTER_BUNDLE_PATH}" RESULT _second_result)
if(NOT _second_result STREQUAL "0")
    file(RENAME "${_temporary}" "${_legacy}" RESULT _rollback_result)
    message(FATAL_ERROR "Cannot normalize Genycaster.app casing: ${_second_result} (rollback: ${_rollback_result})")
endif()
message(STATUS "Normalized legacy macOS bundle directory to Genycaster.app")
    return()
endif()

# --- Native WebKit adapter: C++26 core / C ABI / Objective-C implementation ---
macro(genycaster_macos_setup_browser)
    # Apple SDK still contains mixed-enum expressions invalid under C++26.
    # Compile WebKit/AppKit as Objective-C, not Objective-C++; no SDK overlays.
    execute_process(
        COMMAND xcrun --sdk macosx --find clang
        OUTPUT_VARIABLE _genycasterAppleClang
        ERROR_VARIABLE _genycasterAppleClangError
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _genycasterAppleClangResult)
    if(NOT _genycasterAppleClangResult EQUAL 0
       OR NOT EXISTS "${_genycasterAppleClang}")
        message(FATAL_ERROR
            "Xcode Objective-C compiler unavailable: ${_genycasterAppleClangError}")
    endif()

    get_filename_component(_genycasterExpectedObjc "${_genycasterAppleClang}" REALPATH)
    if(CMAKE_OBJC_COMPILER_LOADED)
        get_filename_component(_genycasterConfiguredObjc "${CMAKE_OBJC_COMPILER}" REALPATH)
        if(NOT _genycasterConfiguredObjc STREQUAL _genycasterExpectedObjc)
            message(FATAL_ERROR
                "Objective-C compiler changed after enable_language(OBJC). "
                "Clear the CMake cache and configure with Xcode's clang.")
        endif()
    else()
        set(CMAKE_OBJC_COMPILER "${_genycasterAppleClang}" CACHE FILEPATH
            "AppleClang for native WebKit Objective-C adapter" FORCE)
    endif()
    enable_language(OBJC)

    execute_process(
        COMMAND xcrun --sdk macosx --show-sdk-path
        OUTPUT_VARIABLE _genycasterXcodeSdk
        ERROR_VARIABLE _genycasterSdkError
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _genycasterSdkResult)
    if(NOT _genycasterSdkResult EQUAL 0
       OR NOT EXISTS "${_genycasterXcodeSdk}")
        message(FATAL_ERROR "Xcode macOS SDK unavailable: ${_genycasterSdkError}")
    endif()
    if(IS_ABSOLUTE "${CMAKE_OSX_SYSROOT}"
       AND EXISTS "${CMAKE_OSX_SYSROOT}")
        file(REAL_PATH "${CMAKE_OSX_SYSROOT}" _genycasterConfiguredSdk)
        file(REAL_PATH "${_genycasterXcodeSdk}" _genycasterSelectedSdk)
        if(NOT _genycasterConfiguredSdk STREQUAL _genycasterSelectedSdk)
            message(FATAL_ERROR
                "Mixed Apple SDKs: CMake uses '${_genycasterConfiguredSdk}', "
                "but xcrun selects '${_genycasterSelectedSdk}'. "
                "Align Qt Creator SDKROOT/CMake toolchain and clear its cache.")
        endif()
    endif()

    find_library(genycasterWebKitFramework WebKit REQUIRED)
    find_library(genycasterAppKitFramework AppKit REQUIRED)
    find_library(genycasterQuartzCoreFramework QuartzCore REQUIRED)

    add_library(farcaster_miniapp_browser STATIC
        "${CMAKE_SOURCE_DIR}/ui/app/miniapp/browser_macos.cpp"
        "${CMAKE_SOURCE_DIR}/ui/app/miniapp/browser_macos.m"
        "${CMAKE_SOURCE_DIR}/ui/app/miniapp/browser_macos_native.h")
    set_target_properties(farcaster_miniapp_browser PROPERTIES
        CXX_STANDARD 26
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        OBJC_STANDARD 11
        OBJC_STANDARD_REQUIRED ON
        OBJC_EXTENSIONS OFF
        CXX_SCAN_FOR_MODULES OFF
        AUTOMOC OFF)
    target_compile_options(farcaster_miniapp_browser PRIVATE
        "$<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>"
        "$<$<COMPILE_LANGUAGE:OBJC>:-isysroot>"
        "$<$<COMPILE_LANGUAGE:OBJC>:${_genycasterXcodeSdk}>")
    target_link_libraries(farcaster_miniapp_browser PRIVATE
        Qt6::Gui Qt6::Network
        "${genycasterWebKitFramework}"
        "${genycasterAppKitFramework}"
        "${genycasterQuartzCoreFramework}")
endmacro()
