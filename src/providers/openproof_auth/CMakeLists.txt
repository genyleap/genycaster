if(NOT TARGET Boost::headers)
    find_package(Boost 1.75 REQUIRED)
endif()

# Built-in provider adapters are statically composed into the client.
# On Windows, C++26 module exports in DLL targets are not portable ABI exports.
# Use an explicit archive regardless of BUILD_SHARED_LIBS/vcpkg triplet.
add_library(farcaster_provider_openproof_auth STATIC)
add_library(farcaster::provider_openproof_auth ALIAS farcaster_provider_openproof_auth)

target_sources(farcaster_provider_openproof_auth
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            auth.cppm
    PRIVATE
        auth.cpp
)

target_link_libraries(farcaster_provider_openproof_auth
    PUBLIC
        farcaster::capabilities
        farcaster::domain
        farcaster::transport
    PRIVATE
        Boost::headers
)

target_compile_features(farcaster_provider_openproof_auth PUBLIC cxx_std_26)
set_property(TARGET farcaster_provider_openproof_auth PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_provider_openproof_auth)
