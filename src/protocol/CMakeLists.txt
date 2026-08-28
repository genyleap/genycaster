add_library(farcaster_protocol)
add_library(farcaster::protocol ALIAS farcaster_protocol)

target_sources(farcaster_protocol
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            hash.cppm
            message.cppm
            key_authorization.cppm
    PRIVATE
        hash.cpp
        message.cpp
        key_authorization.cpp
)

target_link_libraries(farcaster_protocol
    PUBLIC
        farcaster::domain
        farcaster::foundation
)

target_compile_features(farcaster_protocol PUBLIC cxx_std_26)
set_property(TARGET farcaster_protocol PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_protocol)

add_library(farcaster_protocol_write)
add_library(farcaster::protocol_write ALIAS farcaster_protocol_write)

target_sources(farcaster_protocol_write
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            write.cppm
    PRIVATE
        write.cpp
)

target_link_libraries(farcaster_protocol_write
    PUBLIC
        farcaster::protocol
        farcaster::capabilities
)

target_compile_features(farcaster_protocol_write PUBLIC cxx_std_26)
set_property(TARGET farcaster_protocol_write PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_protocol_write)
