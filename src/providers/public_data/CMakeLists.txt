find_package(Boost 1.75 REQUIRED)

add_library(farcaster_provider_public_data)
add_library(farcaster::provider_public_data ALIAS farcaster_provider_public_data)

target_sources(farcaster_provider_public_data
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            config.cppm
            market.cppm
            score.cppm
            holdings.cppm
    PRIVATE
        market.cpp
        score.cpp
        holdings.cpp
        boost_json.cpp
)

target_link_libraries(farcaster_provider_public_data
    PUBLIC
        farcaster::capabilities
        farcaster::domain
        farcaster::transport
    PRIVATE
        Boost::headers
)

target_compile_definitions(farcaster_provider_public_data PRIVATE BOOST_JSON_NO_LIB)
target_compile_features(farcaster_provider_public_data PUBLIC cxx_std_26)
set_property(TARGET farcaster_provider_public_data PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_provider_public_data)
