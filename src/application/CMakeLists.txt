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

# Executor is a small application-level scheduling contract shared by read,
# session, signer and durable-write orchestration. Keep it independently
# buildable so write verification does not need to compile unrelated use cases.
add_library(farcaster_application_executor)
add_library(farcaster::application_executor ALIAS farcaster_application_executor)
target_sources(farcaster_application_executor
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            executor.cppm
)
target_link_libraries(farcaster_application_executor PUBLIC farcaster::foundation)
target_compile_features(farcaster_application_executor PUBLIC cxx_std_26)
set_property(TARGET farcaster_application_executor PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_application_executor)

add_library(farcaster_application)
add_library(farcaster::application ALIAS farcaster_application)

target_sources(farcaster_application
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            configuration.cppm
            read_model.cppm
            read_gateway.cppm
            read_async_gateway.cppm
            read_async_service.cppm
            read_service.cppm
            session.cppm
            signer.cppm
    PRIVATE
        "${configuredFeaturesSource}"
        registration_plan.hpp
        read_service.cpp
        read_async_service.cpp
        session.cpp
        signer.cpp
)

target_link_libraries(farcaster_application
    PUBLIC
        farcaster::application_executor
        farcaster::foundation
        farcaster::domain
        farcaster::capabilities
        farcaster::local_state
)
target_compile_features(farcaster_application PUBLIC cxx_std_26)
set_property(TARGET farcaster_application PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_application)


# Rich publishing is intentionally isolated from session/read orchestration. This
# keeps the AST, safe serializer and syntax registry independently testable and
# prevents UI-rich-content work from pulling the complete live-read graph into
# every compiler invocation.
add_library(farcaster_application_rich)
add_library(farcaster::application_rich ALIAS farcaster_application_rich)
target_sources(farcaster_application_rich
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            rich_format.cppm
            syntax.cppm
    PRIVATE
        rich_format.cpp
        syntax.cpp
)
target_link_libraries(farcaster_application_rich
    PUBLIC
        farcaster::application_emoji
        farcaster::foundation
        farcaster::domain
)
target_compile_features(farcaster_application_rich PUBLIC cxx_std_26)
set_property(TARGET farcaster_application_rich PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_application_rich)

# Durable protocol writes are an independently testable application use case.
# They consume only stable ports and the executor contract; no provider,
# storage implementation, Qt or signer implementation leaks inward.
add_library(farcaster_application_write)
add_library(farcaster::application_write ALIAS farcaster_application_write)
target_sources(farcaster_application_write
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            write.cppm
    PRIVATE
        write.cpp
)
target_link_libraries(farcaster_application_write
    PUBLIC
        farcaster::application_executor
        farcaster::foundation
        farcaster::domain
        farcaster::capabilities
        farcaster::local_state
)
target_compile_features(farcaster_application_write PUBLIC cxx_std_26)
set_property(TARGET farcaster_application_write PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_application_write)


if(FARCASTER_ENABLE_MEDIA_UPLOAD)
    # Media upload is an optional application capability. The core service owns
    # scheduling/session policy only; filesystem/network/storage behavior remains
    # behind capability.media and concrete outer adapters.
    add_library(farcaster_application_media)
    add_library(farcaster::application_media ALIAS farcaster_application_media)
    target_sources(farcaster_application_media
        PUBLIC
            FILE_SET CXX_MODULES
            FILES
                media_upload.cppm
                media_lifecycle.cppm
        PRIVATE
            media_upload.cpp
            media_lifecycle.cpp
    )
    target_link_libraries(farcaster_application_media
        PUBLIC
            farcaster::application_executor
            farcaster::foundation
            farcaster::domain
            farcaster::capabilities
    )
    target_compile_features(farcaster_application_media PUBLIC cxx_std_26)
    set_property(TARGET farcaster_application_media PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_application_media)
endif()
