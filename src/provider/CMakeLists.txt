add_library(farcaster_provider)
add_library(farcaster::provider ALIAS farcaster_provider)

target_sources(farcaster_provider
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            provider.cppm
)

target_compile_features(farcaster_provider PUBLIC cxx_std_26)
set_property(TARGET farcaster_provider PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_provider)
