find_package(Boost 1.75 REQUIRED)

add_library(farcaster_provider_hypersnap)
add_library(farcaster::provider_hypersnap ALIAS farcaster_provider_hypersnap)

target_sources(farcaster_provider_hypersnap
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            config.cppm
            request.cppm
            async_request.cppm
            async_gateway.cppm
            codec.cppm
            continuation.cppm
            registration.cppm
            feed.cppm
            profile.cppm
            cast.cppm
            social.cppm
            search.cppm
            notification.cppm
            identity.cppm
    PRIVATE
        request.cpp
        async_request.cpp
        async_gateway.cpp
        codec.cpp
        continuation.cpp
        registration.cpp
        feed.cpp
        profile.cpp
        cast.cpp
        social.cpp
        search.cpp
        notification.cpp
        identity.cpp
        boost_json.cpp
)

target_link_libraries(farcaster_provider_hypersnap
    PUBLIC
        farcaster::application
        farcaster::application_executor
        farcaster::capabilities
        farcaster::provider
        farcaster::storage
        farcaster::transport
        farcaster::execution
    PRIVATE
        Boost::headers
)

target_compile_definitions(farcaster_provider_hypersnap PRIVATE BOOST_JSON_NO_LIB)
target_compile_features(farcaster_provider_hypersnap PUBLIC cxx_std_26)
set_property(TARGET farcaster_provider_hypersnap PROPERTY CXX_SCAN_FOR_MODULES ON)

# GCC 14 C++ module dependency-file generation can race under Ninja when this
# provider target compiles implementation units in parallel. Keep the workaround
# build-system-only and narrowly scoped; Clang/MSVC remain fully parallel.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS farcaster_gcc14_modules=1)
    set_property(TARGET farcaster_provider_hypersnap PROPERTY JOB_POOL_COMPILE farcaster_gcc14_modules)
endif()

farcaster_enable_warnings(farcaster_provider_hypersnap)
