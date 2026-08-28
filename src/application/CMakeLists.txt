foreach(featureName IN ITEMS SOCIAL FEED CHANNELS MINIAPPS CHAT SEARCH NOTIFICATIONS WALLET)
    if(FARCASTER_ENABLE_${featureName})
        set(FARCASTER_ENABLE_${featureName}_CPP true)
    else()
        set(FARCASTER_ENABLE_${featureName}_CPP false)
    endif()
endforeach()


if(FARCASTER_SELF_HOSTED_ONLY)
    set(FARCASTER_DEPLOYMENT_PROFILE_CPP StrictSelfHosted)
else()
    set(FARCASTER_DEPLOYMENT_PROFILE_CPP Standard)
endif()

set(configuredFeaturesSource "${CMAKE_CURRENT_BINARY_DIR}/configured_features.cpp")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/configured_features.cpp.in"
    "${configuredFeaturesSource}"
    @ONLY
)

add_library(farcaster_application)
add_library(farcaster::application ALIAS farcaster_application)

target_sources(farcaster_application
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            configuration.cppm
            executor.cppm
            read_model.cppm
            read_gateway.cppm
            read_service.cppm
            session.cppm
            signer.cppm
    PRIVATE
        "${configuredFeaturesSource}"
        read_service.cpp
        session.cpp
        signer.cpp
)

target_link_libraries(farcaster_application PUBLIC farcaster::foundation farcaster::domain farcaster::capabilities farcaster::local_state)
target_compile_features(farcaster_application PUBLIC cxx_std_26)
set_property(TARGET farcaster_application PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_application)
