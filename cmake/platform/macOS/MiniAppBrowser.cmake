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
# Respect an explicitly configured OBJCXX compiler (e.g. custom CI kits).
if(NOT CMAKE_OBJCXX_COMPILER)
    execute_process(
        COMMAND xcrun --sdk macosx --find clang++
        OUTPUT_VARIABLE _genycasterAppleClang
        ERROR_VARIABLE _genycasterAppleClangError
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _genycasterAppleClangResult)
    if(NOT _genycasterAppleClangResult EQUAL 0
       OR NOT EXISTS "${_genycasterAppleClang}")
        message(FATAL_ERROR
            "GenyCaster's macOS Mini App adapter requires the Apple SDK compiler "
            "for Objective-C++26. xcrun could not locate clang++: "
            "${_genycasterAppleClangError}")
    endif()
    set(CMAKE_OBJCXX_COMPILER "${_genycasterAppleClang}")
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

# Verify the actual compiler/SDK pair now, rather than allowing a full native
# build to fail deep inside Apple headers with an obscure enum diagnostic.
# C++26 type safety and compiler warnings are not relaxed in this probe.
include(CheckOBJCXXSourceCompiles)
unset(GENYCASTER_MACOS_OBJCXX26_SDK_COMPATIBLE CACHE)
set(_genycasterSavedRequiredFlags "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++26 -fobjc-arc")
check_objcxx_source_compiles([=[
#import <Foundation/Foundation.h>
#import <AppKit/NSApplication.h>
#import <WebKit/WKWebView.h>
int main() { return 0; }
]=] GENYCASTER_MACOS_OBJCXX26_SDK_COMPATIBLE)
set(CMAKE_REQUIRED_FLAGS "${_genycasterSavedRequiredFlags}")
unset(_genycasterSavedRequiredFlags)
if(NOT GENYCASTER_MACOS_OBJCXX26_SDK_COMPATIBLE)
    message(FATAL_ERROR
        "The configured Objective-C++ compiler and Apple macOS SDK cannot "
        "compile SDK headers in C++26 mode. Keep OBJCXX_STANDARD=26 and "
        "select a matching Xcode/AppleClang SDK in Qt Creator. Check "
        "xcode-select -p, xcrun --sdk macosx --show-sdk-path and "
        "CMakeFiles/CMakeError.log (or CMakeConfigureLog.yaml).")
endif()

find_library(genycasterWebKitFramework WebKit REQUIRED)
find_library(genycasterAppKitFramework AppKit REQUIRED)
find_library(genycasterQuartzCoreFramework QuartzCore REQUIRED)

add_library(farcaster_miniapp_browser STATIC
    "${CMAKE_SOURCE_DIR}/ui/app/miniapp_browser_macos.mm")
set_target_properties(farcaster_miniapp_browser PROPERTIES
    OBJCXX_STANDARD 26
    OBJCXX_STANDARD_REQUIRED ON
    OBJCXX_EXTENSIONS OFF
    CXX_SCAN_FOR_MODULES OFF
    AUTOMOC OFF)
target_compile_options(farcaster_miniapp_browser PRIVATE -fobjc-arc)
target_link_libraries(farcaster_miniapp_browser PRIVATE
    Qt6::Gui Qt6::Network
    "${genycasterWebKitFramework}"
    "${genycasterAppKitFramework}"
    "${genycasterQuartzCoreFramework}")
