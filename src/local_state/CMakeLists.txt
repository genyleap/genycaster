add_library(farcaster_local_state)
add_library(farcaster::local_state ALIAS farcaster_local_state)

target_sources(farcaster_local_state
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            entity.cppm
            timeline.cppm
            read.cppm
            session.cppm
            signer.cppm
            write.cppm
)

target_link_libraries(farcaster_local_state PUBLIC farcaster::foundation farcaster::domain farcaster::capabilities)
target_compile_features(farcaster_local_state PUBLIC cxx_std_26)
set_property(TARGET farcaster_local_state PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_local_state)
