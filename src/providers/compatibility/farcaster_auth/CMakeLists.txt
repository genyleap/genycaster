find_package(Boost 1.75 REQUIRED)
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)

add_library(farcaster_provider_compatibility_auth)
add_library(farcaster::provider_compatibility_auth ALIAS farcaster_provider_compatibility_auth)

target_sources(farcaster_provider_compatibility_auth
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            auth.cppm
    PRIVATE
        auth.cpp
)

target_link_libraries(farcaster_provider_compatibility_auth
    PUBLIC
        farcaster::capabilities
        farcaster::domain
        farcaster::transport
    PRIVATE
        Boost::headers
        OpenSSL::Crypto
)

target_compile_features(farcaster_provider_compatibility_auth PUBLIC cxx_std_26)
set_property(TARGET farcaster_provider_compatibility_auth PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_provider_compatibility_auth)
