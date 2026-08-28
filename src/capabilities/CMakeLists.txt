add_library(farcaster_capabilities)
add_library(farcaster::capabilities ALIAS farcaster_capabilities)

target_sources(farcaster_capabilities
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            capability.cppm
            catalog.cppm
            pagination.cppm
            feed.cppm
            profile.cppm
            cast.cppm
            identity.cppm
            social.cppm
            auth.cppm
            signer.cppm
    PRIVATE
        catalog.cpp
)

target_link_libraries(farcaster_capabilities PUBLIC farcaster::foundation farcaster::domain)
target_compile_features(farcaster_capabilities PUBLIC cxx_std_26)
set_property(TARGET farcaster_capabilities PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_capabilities)
