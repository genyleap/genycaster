add_library(farcaster_sync)
add_library(farcaster::sync ALIAS farcaster_sync)

target_sources(farcaster_sync
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            model.cppm
            checkpoint_store.cppm
            coordinator.cppm
    PRIVATE
        coordinator.cpp
)

target_link_libraries(farcaster_sync PUBLIC farcaster::foundation)
target_compile_features(farcaster_sync PUBLIC cxx_std_26)
set_property(TARGET farcaster_sync PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_sync)
