add_library(farcaster_protocol)
add_library(farcaster::protocol ALIAS farcaster_protocol)

target_sources(farcaster_protocol
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            hash.cppm
            message.cppm
    PRIVATE
        hash.cpp
        message.cpp
)

target_link_libraries(farcaster_protocol
    PUBLIC
        farcaster::domain
        farcaster::foundation
)

target_compile_features(farcaster_protocol PUBLIC cxx_std_26)
set_property(TARGET farcaster_protocol PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_protocol)
