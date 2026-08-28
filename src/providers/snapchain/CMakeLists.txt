find_package(Boost 1.75 REQUIRED)

add_library(farcaster_provider_snapchain_signer)
add_library(farcaster::provider_snapchain_signer ALIAS farcaster_provider_snapchain_signer)

target_sources(farcaster_provider_snapchain_signer
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            signer.cppm
    PRIVATE
        signer.cpp
        boost_json.cpp
)

target_link_libraries(farcaster_provider_snapchain_signer
    PUBLIC
        farcaster::capabilities
        farcaster::domain
        farcaster::transport
    PRIVATE
        farcaster::protocol
        Boost::headers
)

target_compile_definitions(farcaster_provider_snapchain_signer PRIVATE BOOST_JSON_NO_LIB)
target_compile_features(farcaster_provider_snapchain_signer PUBLIC cxx_std_26)
set_property(TARGET farcaster_provider_snapchain_signer PROPERTY CXX_SCAN_FOR_MODULES ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS farcaster_gcc14_snapchain_modules=1)
    set_property(TARGET farcaster_provider_snapchain_signer PROPERTY JOB_POOL_COMPILE farcaster_gcc14_snapchain_modules)
endif()

farcaster_enable_warnings(farcaster_provider_snapchain_signer)
