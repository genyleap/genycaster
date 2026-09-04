add_library(farcaster_transport_qt_network)
add_library(farcaster::transport_qt_network ALIAS farcaster_transport_qt_network)

target_sources(farcaster_transport_qt_network
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            http.cppm
            async_http.cppm
    PRIVATE
        http.cpp
        async_http.cpp
)

target_link_libraries(farcaster_transport_qt_network
    PUBLIC farcaster::transport
    PRIVATE Qt6::Network
)

target_compile_features(farcaster_transport_qt_network PUBLIC cxx_std_26)
set_property(TARGET farcaster_transport_qt_network PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_transport_qt_network)


if(FARCASTER_ENABLE_MEDIA_UPLOAD)
    add_library(farcaster_transport_qt_media)
    add_library(farcaster::transport_qt_media ALIAS farcaster_transport_qt_media)
    target_sources(farcaster_transport_qt_media
        PUBLIC
            FILE_SET CXX_MODULES
            FILES media_upload.cppm
        PRIVATE
            media_upload.cpp
    )
    target_link_libraries(farcaster_transport_qt_media
        PUBLIC
            farcaster::application_media
            farcaster::transport_qt_network
        PRIVATE
            Qt6::Core
            Qt6::Network
    )
    target_compile_features(farcaster_transport_qt_media PUBLIC cxx_std_26)
    set_property(TARGET farcaster_transport_qt_media PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_transport_qt_media)
endif()
