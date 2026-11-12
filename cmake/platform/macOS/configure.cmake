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

# --- Apple SDK / WebKit native bridge (C++26) ---

# --- C++26 Objective-C++ / WebKit SDK integration ---
macro(genycaster_macos_setup_browser)
    # The WebKit/AppKit Objective-C++ boundary is deliberately not a C++26
    # named-module consumer. It exposes only the stable Qt/C++ types declared by
    # miniapp_browser.hpp and never leaks native framework types into the core.
    #
    # The Objective-C++ adapter uses the same Modern C++26 contract as
    # GenyCaster's shared runtime. Any errors from Apple SDK headers must be
    # diagnosed at the compiler/SDK boundary, not hidden by suppressing
    # diagnostics or lowering the language standard.
    # The main C++26 modules may use upstream LLVM. This SDK adapter is not a
    # module consumer and must use the Apple-Clang/Apple-SDK toolchain pair by
    # default. Unlike a language-standard downgrade, this does not change the
    # C++26 contract or accept invalid constructs in GenyCaster's own code.
    # Select the Xcode SDK compiler for this Apple-framework boundary. The
    # shared C++26 module compiler remains independent. A cached Homebrew LLVM
    # compiler is not a compatible fallback for this SDK/Objective-C++ translation.
    execute_process(
        COMMAND xcrun --sdk macosx --find clang++
        OUTPUT_VARIABLE _genycasterAppleClang
        ERROR_VARIABLE _genycasterAppleClangError
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _genycasterAppleClangResult)
    if(NOT _genycasterAppleClangResult EQUAL 0
       OR NOT EXISTS "${_genycasterAppleClang}")
        message(FATAL_ERROR
            "Xcode's Objective-C++26 compiler is unavailable: "
            "${_genycasterAppleClangError}")
    endif()
    # Select AppleClang before OBJCXX is enabled. Qt Creator often supplies
    # Homebrew LLVM through CMAKE_OBJCXX_COMPILER even in a new macOS build cache;
    # testing only whether that variable is empty therefore does not work.
    # Once CMake has enabled this language, changing its compiler in-place is
    # unsafe: report the mismatch and require a fresh CMake configuration.
    get_filename_component(_genycasterExpectedObjcxx
        "${_genycasterAppleClang}" REALPATH)
    if(CMAKE_OBJCXX_COMPILER_LOADED)
        get_filename_component(_genycasterConfiguredObjcxx
            "${CMAKE_OBJCXX_COMPILER}" REALPATH)
        if(NOT _genycasterConfiguredObjcxx STREQUAL _genycasterExpectedObjcxx)
            message(FATAL_ERROR
                "This CMake cache already enabled Objective-C++ with "
                "'${CMAKE_OBJCXX_COMPILER}', instead of Xcode's "
                "'${_genycasterAppleClang}'. Clear CMake Configuration for "
                "this Qt Creator Kit. C++26 and diagnostics remain enabled.")
        endif()
    else()
        if(CMAKE_OBJCXX_COMPILER)
            get_filename_component(_genycasterConfiguredObjcxx
                "${CMAKE_OBJCXX_COMPILER}" REALPATH)
            if(NOT _genycasterConfiguredObjcxx STREQUAL _genycasterExpectedObjcxx)
                message(STATUS
                    "macOS Objective-C++26: switching SDK adapter compiler "
                    "from '${CMAKE_OBJCXX_COMPILER}' to '${_genycasterAppleClang}'")
            endif()
        endif()
        set(CMAKE_OBJCXX_COMPILER "${_genycasterAppleClang}" CACHE FILEPATH
            "Xcode Objective-C++26 compiler for the macOS SDK boundary" FORCE)
    endif()
    enable_language(OBJCXX)

    # Validate the same SDK selected by CMake against the active Xcode developer
    # directory. A cached CLT/macOS27 SDK mixed with Xcode's headers previously
    # produced unrelated OpenGL availability diagnostics.
    execute_process(
        COMMAND xcrun --sdk macosx --show-sdk-path
        OUTPUT_VARIABLE _genycasterXcodeSdk
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _genycasterSdkResult)
    if(_genycasterSdkResult EQUAL 0
       AND IS_ABSOLUTE "${CMAKE_OSX_SYSROOT}"
       AND EXISTS "${CMAKE_OSX_SYSROOT}"
       AND EXISTS "${_genycasterXcodeSdk}")
        file(REAL_PATH "${CMAKE_OSX_SYSROOT}" _genycasterConfiguredSdk)
        file(REAL_PATH "${_genycasterXcodeSdk}" _genycasterSelectedSdk)
        if(NOT _genycasterConfiguredSdk STREQUAL _genycasterSelectedSdk)
            message(FATAL_ERROR
                "Mixed Apple SDKs: CMake uses '${_genycasterConfiguredSdk}', "
                "but xcrun selects '${_genycasterSelectedSdk}'. "
                "Align the Qt Creator Kit, SDKROOT and xcode-select, then "
                "configure with a fresh CMake cache.")
        endif()
    endif()

    # Apple's SDK contains legacy mixed-enum arithmetic that C++26 rejects.
    # Build a local VFS overlay with explicit enum-to-integer conversions.
    # Never modify installed SDK files, lower the standard or hide diagnostics.
    execute_process(
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/ci/prepare_macos_sdk_cpp26.py"
            --sdk "${_genycasterXcodeSdk}"
            --output "${CMAKE_CURRENT_BINARY_DIR}/macos-sdk-cpp26"
        RESULT_VARIABLE _genycasterOverlayResult
        OUTPUT_VARIABLE _genycasterOverlayOutput
        ERROR_VARIABLE _genycasterOverlayError)
    if(NOT _genycasterOverlayResult EQUAL 0)
        message(FATAL_ERROR
            "macOS C++26 SDK compatibility overlay failed: ${_genycasterOverlayError}")
    endif()
    message(STATUS "${_genycasterOverlayOutput}")
    set(_genycasterSdkOverlay
        "${CMAKE_CURRENT_BINARY_DIR}/macos-sdk-cpp26/macos_cpp26_sdk_overlay.json")

    # Do not make Qt Creator's normal CMake Configure contingent on a
    # separate SDK header/link probe. The old check_objcxx_source_compiles()
    # linked a standalone executable without our WebKit target dependencies;
    # a failed check was incorrectly promoted to FATAL_ERROR and prevented
    # the actual Objective-C++26 target from compiling at all.
    #
    # Developers may opt into a *compile-only* diagnostic probe. Its failure
    # is reported with the actual SDK diagnostic, but is not a configure gate:
    # the real browser translation unit remains the source of truth.
    option(GENYCASTER_MACOS_VALIDATE_CPP26_SDK
        "Run optional compile-only Apple SDK/C++26 compatibility diagnostics" OFF)
    if(GENYCASTER_MACOS_VALIDATE_CPP26_SDK)
        include(CheckOBJCXXSourceCompiles)
        unset(GENYCASTER_MACOS_OBJCXX26_SDK_COMPATIBLE CACHE)
        set(_genycasterSavedRequiredFlags "${CMAKE_REQUIRED_FLAGS}")
        set(_genycasterSavedTryCompileType "${CMAKE_TRY_COMPILE_TARGET_TYPE}")
        set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
        set(CMAKE_REQUIRED_FLAGS
            "${CMAKE_REQUIRED_FLAGS} -std=c++26 -fobjc-arc -fno-modules -isysroot \"${_genycasterXcodeSdk}\" -ivfsoverlay \"${_genycasterSdkOverlay}\"")
        set(_genycasterProbeSource [=[
        #import <Foundation/Foundation.h>
        #import <AppKit/NSApplication.h>
        #import <AppKit/NSEvent.h>
        #import <AppKit/NSImage.h>
        #import <WebKit/WKWebView.h>
        #import <WebKit/WKPreferences.h>
        #import <WebKit/WKNavigationAction.h>
        #import <WebKit/WKNavigationResponse.h>
        #include <CoreGraphics/CGImage.h>
        #include <ApplicationServices/ApplicationServices.h>
        ]=])
        string(APPEND _genycasterProbeSource
            "#include \"${_genycasterXcodeSdk}/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/ColorSyncDeprecated.h\"\n")
        string(APPEND _genycasterProbeSource [=[
        #ifndef GENYCASTER_CPP26_OVERLAY_CGIMAGE
        #error "CGImage.h was not redirected by the C++26 SDK overlay"
        #endif
        #ifndef GENYCASTER_CPP26_OVERLAY_COLORSYNC
        #error "ColorSyncDeprecated.h was not redirected by the C++26 SDK overlay"
        #endif
        int genycaster_sdk_probe() { return 0; }
        ]=])
        check_objcxx_source_compiles(
            "${_genycasterProbeSource}"
            GENYCASTER_MACOS_OBJCXX26_SDK_COMPATIBLE)
        set(CMAKE_REQUIRED_FLAGS "${_genycasterSavedRequiredFlags}")
        set(CMAKE_TRY_COMPILE_TARGET_TYPE "${_genycasterSavedTryCompileType}")
        unset(_genycasterSavedRequiredFlags)
        unset(_genycasterSavedTryCompileType)
        unset(_genycasterProbeSource)
        if(NOT GENYCASTER_MACOS_OBJCXX26_SDK_COMPATIBLE)
            message(WARNING
                "Optional Apple SDK/C++26 probe did not compile. This is "
                "not a Qt Creator Configure error; inspect CMakeConfigureLog.yaml "
                "for the exact AppleClang diagnostic. The native browser "
                "target is still compiled as Objective-C++26.")
        endif()
    endif()

    find_library(genycasterWebKitFramework WebKit REQUIRED)
    find_library(genycasterAppKitFramework AppKit REQUIRED)
    find_library(genycasterQuartzCoreFramework QuartzCore REQUIRED)

    add_library(farcaster_miniapp_browser STATIC
        "${CMAKE_SOURCE_DIR}/ui/app/miniapp/browser_macos.mm")
    set_target_properties(farcaster_miniapp_browser PROPERTIES
        OBJCXX_STANDARD 26
        OBJCXX_STANDARD_REQUIRED ON
        OBJCXX_EXTENSIONS OFF
        CXX_SCAN_FOR_MODULES OFF
        AUTOMOC OFF)
    # Place the matching Xcode SDK after cached CLT Kit flags.
    target_compile_options(farcaster_miniapp_browser PRIVATE
        -fobjc-arc
        -fno-modules
        -isysroot "${_genycasterXcodeSdk}"
        -ivfsoverlay "${_genycasterSdkOverlay}")
    target_link_libraries(farcaster_miniapp_browser PRIVATE
        Qt6::Gui Qt6::Network
        "${genycasterWebKitFramework}"
        "${genycasterAppKitFramework}"
        "${genycasterQuartzCoreFramework}")
endmacro()
