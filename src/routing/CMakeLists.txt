add_library(farcaster_routing)
add_library(farcaster::routing ALIAS farcaster_routing)

target_sources(farcaster_routing
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            registry.cppm
            health.cppm
            policy.cppm
            router.cppm
            read_gateway.cppm
            read_async_gateway.cppm
    PRIVATE
        registry.cpp
        health.cpp
        policy.cpp
        router.cpp
        read_gateway.cpp
        read_async_gateway.cpp
)

target_link_libraries(farcaster_routing
    PUBLIC
        farcaster::foundation
        farcaster::capabilities
        farcaster::provider
        farcaster::application
)
target_compile_features(farcaster_routing PUBLIC cxx_std_26)
set_property(TARGET farcaster_routing PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_routing)
