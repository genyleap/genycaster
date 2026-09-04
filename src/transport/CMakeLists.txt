add_library(farcaster_transport)
add_library(farcaster::transport ALIAS farcaster_transport)

target_sources(farcaster_transport
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            http.cppm
            async_http.cppm
)

target_link_libraries(farcaster_transport PUBLIC farcaster::foundation)
target_compile_features(farcaster_transport PUBLIC cxx_std_26)
set_property(TARGET farcaster_transport PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_transport)

if(FARCASTER_ENABLE_HYPERSNAP_PROVIDER)
    if(APPLE)
        # The SDK already supplies headers through CMAKE_OSX_SYSROOT. Linking the
        # system curl dylib directly avoids propagating MacOSX.sdk/usr/include as
        # an explicit include path into Homebrew Clang module compilations.
        find_library(FARCASTER_CURL_LIBRARY NAMES curl REQUIRED)
    elseif(UNIX)
        # Prefer pkg-config on non-Apple Unix so a locally installed static
        # libcurl carries its complete transitive link set (OpenSSL, zlib,
        # brotli, zstd, nghttp2, PSL, ...). FindCURL may otherwise select a
        # bare libcurl.a and omit those dependencies at final executable link.
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(FARCASTER_LIBCURL QUIET IMPORTED_TARGET libcurl)
        endif()
        if(NOT TARGET PkgConfig::FARCASTER_LIBCURL)
            find_package(CURL REQUIRED)
        endif()
    else()
        find_package(CURL REQUIRED)
    endif()

    add_library(farcaster_transport_curl)
    add_library(farcaster::transport_curl ALIAS farcaster_transport_curl)
    target_sources(farcaster_transport_curl
        PUBLIC
            FILE_SET CXX_MODULES
            FILES curl.cppm
        PRIVATE
            curl.cpp
    )
    target_link_libraries(farcaster_transport_curl PUBLIC farcaster::transport)
    if(APPLE)
        target_link_libraries(farcaster_transport_curl PRIVATE "${FARCASTER_CURL_LIBRARY}")
    elseif(TARGET PkgConfig::FARCASTER_LIBCURL)
        target_link_libraries(farcaster_transport_curl PRIVATE PkgConfig::FARCASTER_LIBCURL)
    else()
        target_link_libraries(farcaster_transport_curl PRIVATE CURL::libcurl)
    endif()
    target_compile_features(farcaster_transport_curl PUBLIC cxx_std_26)
    set_property(TARGET farcaster_transport_curl PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_transport_curl)
endif()

if(FARCASTER_BUILD_UI)
    add_subdirectory(qt_network)
endif()
