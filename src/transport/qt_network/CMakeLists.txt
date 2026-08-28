add_library(farcaster_transport_qt_network)
add_library(farcaster::transport_qt_network ALIAS farcaster_transport_qt_network)

target_sources(farcaster_transport_qt_network
    PUBLIC
        FILE_SET CXX_MODULES
        FILES http.cppm
    PRIVATE
        http.cpp
)

target_link_libraries(farcaster_transport_qt_network
    PUBLIC farcaster::transport
    PRIVATE Qt6::Network
)

target_compile_features(farcaster_transport_qt_network PUBLIC cxx_std_26)
set_property(TARGET farcaster_transport_qt_network PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_transport_qt_network)
