add_executable(farcaster_core_tests
    core_tests.cpp
)
target_link_libraries(farcaster_core_tests
    PRIVATE
        farcaster::application
        farcaster::routing
        farcaster::sync
)
target_compile_features(farcaster_core_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_core_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_core_tests)
add_test(NAME core.behavior COMMAND farcaster_core_tests)
set_tests_properties(core.behavior PROPERTIES
    LABELS "unit"
    ENVIRONMENT
        "FARCASTER_EXPECT_SOCIAL=${FARCASTER_ENABLE_SOCIAL};FARCASTER_EXPECT_FEED=${FARCASTER_ENABLE_FEED};FARCASTER_EXPECT_CHANNELS=${FARCASTER_ENABLE_CHANNELS};FARCASTER_EXPECT_MINIAPPS=${FARCASTER_ENABLE_MINIAPPS};FARCASTER_EXPECT_CHAT=${FARCASTER_ENABLE_CHAT};FARCASTER_EXPECT_SEARCH=${FARCASTER_ENABLE_SEARCH};FARCASTER_EXPECT_NOTIFICATIONS=${FARCASTER_ENABLE_NOTIFICATIONS};FARCASTER_EXPECT_WALLET=${FARCASTER_ENABLE_WALLET};FARCASTER_EXPECT_SELF_HOSTED_ONLY=${FARCASTER_SELF_HOSTED_ONLY};FARCASTER_EXPECT_SIWF_DOMAIN=${FARCASTER_DEFAULT_SIWF_DOMAIN};FARCASTER_EXPECT_SIWF_URI=${FARCASTER_DEFAULT_SIWF_URI};FARCASTER_EXPECT_SNAPCHAIN_BASE_URL=${FARCASTER_DEFAULT_SNAPCHAIN_BASE_URL};FARCASTER_EXPECT_MEDIA_GATEWAY_BASE_URL=${FARCASTER_DEFAULT_MEDIA_GATEWAY_BASE_URL};FARCASTER_EXPECT_APP_FID=${FARCASTER_DEFAULT_APP_FID};FARCASTER_EXPECT_APP_NAME=${FARCASTER_DEFAULT_APP_NAME}"
)

add_executable(farcaster_execution_tests execution_tests.cpp)
target_link_libraries(farcaster_execution_tests PRIVATE farcaster::execution)
target_compile_features(farcaster_execution_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_execution_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_execution_tests)
add_test(NAME execution.bounded_read_pool COMMAND farcaster_execution_tests)
set_tests_properties(execution.bounded_read_pool PROPERTIES LABELS "unit;execution;concurrency")

if(FARCASTER_ENABLE_MEDIA_UPLOAD)
    add_executable(farcaster_media_upload_tests media_upload_tests.cpp)
    target_link_libraries(farcaster_media_upload_tests
        PRIVATE
            farcaster::application_media
    )
    target_compile_features(farcaster_media_upload_tests PRIVATE cxx_std_26)
    set_property(TARGET farcaster_media_upload_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_media_upload_tests)
    add_test(NAME application.media_upload COMMAND farcaster_media_upload_tests)
    set_tests_properties(application.media_upload PROPERTIES LABELS "unit;media;upload;lifecycle")

    if(FARCASTER_BUILD_UI)
        add_executable(farcaster_media_lifecycle_presentation_tests media_lifecycle_presentation_tests.cpp)
        target_link_libraries(farcaster_media_lifecycle_presentation_tests
            PRIVATE
                farcaster::presentation
                Qt6::Core
        )
        target_compile_features(farcaster_media_lifecycle_presentation_tests PRIVATE cxx_std_26)
        set_property(TARGET farcaster_media_lifecycle_presentation_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
        farcaster_enable_warnings(farcaster_media_lifecycle_presentation_tests)
        add_test(NAME presentation.media_asset_lifecycle COMMAND farcaster_media_lifecycle_presentation_tests)
        set_tests_properties(presentation.media_asset_lifecycle PROPERTIES LABELS "unit;media;lifecycle;presentation")
    endif()
endif()

if(FARCASTER_BUILD_UI)
    add_executable(farcaster_presentation_icon_tests presentation_icon_tests.cpp)
    target_link_libraries(farcaster_presentation_icon_tests
        PRIVATE
            farcaster::presentation
            Qt6::Core
    )
    target_compile_features(farcaster_presentation_icon_tests PRIVATE cxx_std_26)
    set_property(TARGET farcaster_presentation_icon_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_presentation_icon_tests)
    add_test(NAME presentation.contextual_icons COMMAND farcaster_presentation_icon_tests)
    set_tests_properties(presentation.contextual_icons PROPERTIES LABELS "unit;presentation;icons")

    add_executable(farcaster_presentation_row_notifications_tests
        presentation_row_notifications_tests.cpp)
    target_link_libraries(farcaster_presentation_row_notifications_tests
        PRIVATE farcaster::presentation Qt6::Core Qt6::Qml)
    target_compile_features(farcaster_presentation_row_notifications_tests PRIVATE cxx_std_26)
    set_property(TARGET farcaster_presentation_row_notifications_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_presentation_row_notifications_tests)
    add_test(NAME presentation.feed_row_notifications
        COMMAND farcaster_presentation_row_notifications_tests)
    set_tests_properties(presentation.feed_row_notifications PROPERTIES
        LABELS "unit;presentation;qt;read-pipeline"
        TIMEOUT 20)
endif()

if(FARCASTER_BUILD_UI)
    add_executable(farcaster_storage_policy_tests storage_policy_tests.cpp)
    target_link_libraries(farcaster_storage_policy_tests
        PRIVATE
            farcaster::presentation_media
    )
    target_compile_features(farcaster_storage_policy_tests PRIVATE cxx_std_26)
    set_property(TARGET farcaster_storage_policy_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_storage_policy_tests)
    add_test(NAME presentation.storage_policy COMMAND farcaster_storage_policy_tests)
    set_tests_properties(presentation.storage_policy PROPERTIES LABELS "unit;media;storage;policy")

    add_executable(farcaster_media_classification_tests media_classification_tests.cpp)
    target_link_libraries(farcaster_media_classification_tests
        PRIVATE
            farcaster::presentation_media
    )
    target_compile_features(farcaster_media_classification_tests PRIVATE cxx_std_26)
    set_property(TARGET farcaster_media_classification_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_media_classification_tests)
    add_test(NAME presentation.media_classification COMMAND farcaster_media_classification_tests)
    set_tests_properties(presentation.media_classification PROPERTIES LABELS "unit;media;classification;presentation")

    add_executable(farcaster_media_playback_coordinator_tests media_playback_coordinator_tests.cpp)
    target_link_libraries(farcaster_media_playback_coordinator_tests
        PRIVATE
            farcaster::presentation
            Qt6::Core
    )
    target_compile_features(farcaster_media_playback_coordinator_tests PRIVATE cxx_std_26)
    set_property(TARGET farcaster_media_playback_coordinator_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    set_property(TARGET farcaster_media_playback_coordinator_tests PROPERTY AUTOMOC ON)
    farcaster_enable_warnings(farcaster_media_playback_coordinator_tests)
    add_test(NAME presentation.media_playback_coordinator COMMAND farcaster_media_playback_coordinator_tests)
    set_tests_properties(presentation.media_playback_coordinator PROPERTIES LABELS "unit;media;lifecycle;presentation")
endif()

add_executable(farcaster_storage_tests
    storage_tests.cpp
)
target_sources(farcaster_storage_tests
    PRIVATE
        storage_behavior.cpp
        temp_database.cpp
)
target_sources(farcaster_storage_tests
    PRIVATE
        FILE_SET CXX_MODULES
        FILES
            storage_behavior.cppm
            temp_database.cppm
)
target_link_libraries(farcaster_storage_tests
    PRIVATE
        farcaster::storage
)
target_compile_features(farcaster_storage_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_storage_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_storage_tests)
add_test(NAME storage.local COMMAND farcaster_storage_tests)
set_tests_properties(storage.local PROPERTIES LABELS "integration")

add_executable(farcaster_read_pipeline_tests read_pipeline_tests.cpp)
target_link_libraries(farcaster_read_pipeline_tests
    PRIVATE
        farcaster::application
        farcaster::storage
        farcaster::execution
)
target_compile_features(farcaster_read_pipeline_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_read_pipeline_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_read_pipeline_tests)
add_test(NAME application.local_first_read COMMAND farcaster_read_pipeline_tests)
set_tests_properties(application.local_first_read PROPERTIES LABELS "unit;integration")

add_executable(farcaster_session_tests session_tests.cpp)
target_link_libraries(farcaster_session_tests
    PRIVATE
        farcaster::application
        farcaster::storage
)
target_compile_features(farcaster_session_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_session_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_session_tests)
add_test(NAME application.guest_auth_session COMMAND farcaster_session_tests)
set_tests_properties(application.guest_auth_session PROPERTIES LABELS "unit;integration")

add_executable(farcaster_signer_tests signer_tests.cpp)
target_link_libraries(farcaster_signer_tests
    PRIVATE
        farcaster::application
        farcaster::storage
)
target_compile_features(farcaster_signer_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_signer_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_signer_tests)
add_test(NAME application.protocol_signer_readiness COMMAND farcaster_signer_tests)
set_tests_properties(application.protocol_signer_readiness PROPERTIES LABELS "unit;integration;security")

add_executable(farcaster_write_routing_tests write_routing_tests.cpp)
target_link_libraries(farcaster_write_routing_tests PRIVATE farcaster::routing)
target_compile_features(farcaster_write_routing_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_write_routing_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_write_routing_tests)
add_test(NAME routing.durable_write_provider COMMAND farcaster_write_routing_tests)
set_tests_properties(routing.durable_write_provider PROPERTIES LABELS "unit;routing;write")

add_executable(farcaster_write_storage_tests write_storage_tests.cpp)
target_link_libraries(farcaster_write_storage_tests PRIVATE farcaster::storage_write)
target_compile_features(farcaster_write_storage_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_write_storage_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_write_storage_tests)
add_test(NAME storage.durable_write_outbox COMMAND farcaster_write_storage_tests)
set_tests_properties(storage.durable_write_outbox PROPERTIES LABELS "integration;write;outbox")

add_executable(farcaster_write_service_tests write_service_tests.cpp)
target_link_libraries(farcaster_write_service_tests
    PRIVATE
        farcaster::application_write
        farcaster::protocol_write
        farcaster::storage_write
)
target_compile_features(farcaster_write_service_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_write_service_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_write_service_tests)
add_test(NAME application.publish_cast_outbox COMMAND farcaster_write_service_tests)
set_tests_properties(application.publish_cast_outbox PROPERTIES LABELS "unit;integration;write;outbox;security")

add_executable(farcaster_rich_format_tests rich_format_tests.cpp)
target_link_libraries(farcaster_rich_format_tests PRIVATE farcaster::application_rich farcaster::domain)
target_compile_features(farcaster_rich_format_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_rich_format_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_rich_format_tests)
add_test(NAME application.rich_publishing_engine COMMAND farcaster_rich_format_tests)
set_tests_properties(application.rich_publishing_engine PROPERTIES LABELS "unit;rich-content;serialization;unicode")

add_executable(farcaster_emoji_tests emoji_tests.cpp)
target_link_libraries(farcaster_emoji_tests
    PRIVATE
        farcaster::application_emoji
        farcaster::application_rich
        farcaster::provider_emoji
)
target_compile_features(farcaster_emoji_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_emoji_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_emoji_tests)
add_test(NAME application.emoji_platform COMMAND farcaster_emoji_tests)
set_tests_properties(application.emoji_platform PROPERTIES
    LABELS "unit;emoji;unicode;serialization;cache;provider")
target_compile_definitions(farcaster_emoji_tests PRIVATE
    EMOJI_ASSET_DIRECTORY="${CMAKE_SOURCE_DIR}/ui/assets/emoji/lottie")
