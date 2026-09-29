find_package(Boost 1.75 REQUIRED)

add_library(farcaster_provider_openproof_auth)
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
