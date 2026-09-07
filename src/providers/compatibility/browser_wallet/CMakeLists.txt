find_package(Boost 1.75 REQUIRED)
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)

add_library(farcaster_provider_compatibility_browser_wallet)
add_library(farcaster::provider_compatibility_browser_wallet ALIAS farcaster_provider_compatibility_browser_wallet)

target_sources(farcaster_provider_compatibility_browser_wallet
    PRIVATE
        boost_json.cpp
)

if(FARCASTER_ENABLE_BROWSER_WALLET_AUTHORIZER)
    target_sources(farcaster_provider_compatibility_browser_wallet
        PUBLIC
            FILE_SET CXX_MODULES
            FILES
                authorizer.cppm
        PRIVATE
            authorizer.cpp
    )
    target_link_libraries(farcaster_provider_compatibility_browser_wallet PUBLIC farcaster::protocol)
endif()

if(FARCASTER_ENABLE_BROWSER_WALLET_BRIDGE)
    target_sources(farcaster_provider_compatibility_browser_wallet
        PUBLIC
            FILE_SET CXX_MODULES
            FILES
                bridge.cppm
        PRIVATE
            bridge.cpp
    )
endif()

target_link_libraries(farcaster_provider_compatibility_browser_wallet
    PUBLIC
        farcaster::capabilities
    PRIVATE
        Boost::headers
        OpenSSL::Crypto
)

target_compile_definitions(farcaster_provider_compatibility_browser_wallet PRIVATE BOOST_JSON_NO_LIB)
target_compile_features(farcaster_provider_compatibility_browser_wallet PUBLIC cxx_std_26)
set_property(TARGET farcaster_provider_compatibility_browser_wallet PROPERTY CXX_SCAN_FOR_MODULES ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS farcaster_gcc14_browser_wallet_modules=1)
    set_property(TARGET farcaster_provider_compatibility_browser_wallet PROPERTY JOB_POOL_COMPILE farcaster_gcc14_browser_wallet_modules)
endif()

farcaster_enable_warnings(farcaster_provider_compatibility_browser_wallet)
