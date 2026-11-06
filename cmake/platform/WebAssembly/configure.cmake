# WebAssembly: browser runtime policy and portable source dependencies.

if(EMSCRIPTEN)
    # SQLite's WASM amalgamation is C, whereas the native product is C++26
    # modules. Never discover/link host macOS SQLite in a cross build.
    enable_language(C)
    message(STATUS "Genycaster WebAssembly: browser sandbox composition")
    # No OS keychain or localhost EIP-1193 server exists in a browser.
    # Fail closed for signing rather than silently keeping keys in JS storage.
    set(FARCASTER_ENABLE_PROTOCOL_SIGNING OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_QTKEYCHAIN_SIGNER_VAULT OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_SNAPCHAIN_WRITE_PROVIDER OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_SIGNER_ATTESTATION_PROVIDER OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_BROWSER_WALLET_AUTHORIZER OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_BROWSER_WALLET_BRIDGE OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_MINIAPPS OFF CACHE BOOL "" FORCE)
    set(FARCASTER_ENABLE_MEDIA_UPLOAD OFF CACHE BOOL "" FORCE)
    set(GENYCASTER_ENABLE_PUBLIC_DATA_PROVIDER OFF CACHE BOOL "" FORCE)
    # Native SIWF and OpenProof adapters use blocking / OS browser transport;
    # they require a dedicated asynchronous web identity adapter.
    set(FARCASTER_ENABLE_COMPATIBILITY_PROVIDERS OFF CACHE BOOL "" FORCE)
    set(GENYCASTER_ENABLE_OPENPROOF_AUTH OFF CACHE BOOL "" FORCE)
    set(FARCASTER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
endif()

# --- Portable Boost headers ---
function(genycaster_wasm_setup_boost_headers)
    if(TARGET Boost::headers)
        return()
    endif()

    # Boost.JSON in the Hypersnap adapter is built from boost/json/src.hpp.
    # Cross-compiling does not need a host-architecture Boost library or a
    # BoostConfig.cmake from the WASM sysroot: only portable C++ headers.
    # Qt Creator's MaintenanceTool can intercept find_package(Boost) during
    # cross-compilation and fail before the headers are inspected.
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "WebAssembly Boost setup requires Emscripten")
    endif()

    set(GENYCASTER_WASM_BOOST_INCLUDE_DIR ""
        CACHE PATH "Optional Boost include root (containing boost/json.hpp) for WASM builds")

    if(NOT GENYCASTER_WASM_BOOST_INCLUDE_DIR)
        # Explicitly look on the build host for *headers only*. NEVER link any
        # Homebrew/macOS Boost binaries into the Emscripten target.
        find_path(_genycasterBoostHostHeaders
            NAMES boost/json.hpp
            HINTS /opt/homebrew/include /usr/local/include
            NO_CMAKE_FIND_ROOT_PATH
        )
        if(_genycasterBoostHostHeaders
            AND EXISTS "${_genycasterBoostHostHeaders}/boost/json/src.hpp"
            AND EXISTS "${_genycasterBoostHostHeaders}/boost/version.hpp")
            set(GENYCASTER_WASM_BOOST_INCLUDE_DIR "${_genycasterBoostHostHeaders}")
        endif()
    endif()

    if(NOT GENYCASTER_WASM_BOOST_INCLUDE_DIR)
        # Use an official full header distribution as a reproducible fallback.
        # It is large (~155 MB), but fetched only once into the CMake build cache.
        # A hard-pinned digest guards against archive substitution.
        include(FetchContent)
        FetchContent_Declare(genycaster_wasm_boost
            URL "https://archives.boost.io/release/1.89.0/source/boost_1_89_0.tar.bz2"
            URL_HASH "SHA256=85a33fa22621b4f314f8e85e1a5e2a9363d22e4f4992925d4bb3bc631b5a0c7a"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            # This dependency is a source of headers, NOT an independent
            # Boost CMake subproject with platform-native compiled targets.
            SOURCE_SUBDIR "__genycaster_no_boost_subproject__"
        )
        FetchContent_MakeAvailable(genycaster_wasm_boost)
        set(GENYCASTER_WASM_BOOST_INCLUDE_DIR "${genycaster_wasm_boost_SOURCE_DIR}")
        if(NOT EXISTS "${GENYCASTER_WASM_BOOST_INCLUDE_DIR}/boost/json.hpp")
            # Handle archive formats which retain their single top-level folder.
            set(GENYCASTER_WASM_BOOST_INCLUDE_DIR
                "${genycaster_wasm_boost_SOURCE_DIR}/boost_1_89_0")
        endif()
    endif()

    if(NOT EXISTS "${GENYCASTER_WASM_BOOST_INCLUDE_DIR}/boost/json.hpp"
       OR NOT EXISTS "${GENYCASTER_WASM_BOOST_INCLUDE_DIR}/boost/json/src.hpp"
       OR NOT EXISTS "${GENYCASTER_WASM_BOOST_INCLUDE_DIR}/boost/config.hpp")
        message(FATAL_ERROR
            "WASM Boost headers missing. GENYCASTER_WASM_BOOST_INCLUDE_DIR must "
            "contain boost/json.hpp, boost/json/src.hpp, and boost/config.hpp.")
    endif()

    # The adapter uses an in-tree compilation unit with boost/json/src.hpp; no
    # Boost binaries (native or WASM) are linked.
    add_library(genycaster_wasm_boost_headers INTERFACE)
    add_library(Boost::headers ALIAS genycaster_wasm_boost_headers)
    target_include_directories(genycaster_wasm_boost_headers INTERFACE
        "${GENYCASTER_WASM_BOOST_INCLUDE_DIR}")
    message(STATUS "Genycaster WASM Boost.JSON headers: ${GENYCASTER_WASM_BOOST_INCLUDE_DIR}")
endfunction()
