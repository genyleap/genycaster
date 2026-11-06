# Test targets belong to this directory so IDEs group them under tests/ui.
find_package(Qt6 REQUIRED COMPONENTS Test)
get_target_property(genycasterUiSourceDir genycaster SOURCE_DIR)
get_target_property(genycasterUiBinaryDir genycaster BINARY_DIR)

# Reuse the production resource manifests and generated translation outputs.
# The UI directory is processed before tests; do not duplicate its asset globs.
get_directory_property(farcasterTranslationQmFiles
    DIRECTORY "${genycasterUiSourceDir}" DEFINITION farcasterTranslationQmFiles)
get_directory_property(genyleapIconSystemFiles
    DIRECTORY "${genycasterUiSourceDir}" DEFINITION _genyleapIconSystemFiles)
get_directory_property(notoEmojiLottieFiles
    DIRECTORY "${genycasterUiSourceDir}" DEFINITION _notoEmojiLottieFiles)

if(NOT EMSCRIPTEN)
    add_executable(genycaster_background_shutdown_tests
        background_shutdown_tests.cpp)
    target_link_libraries(genycaster_background_shutdown_tests PRIVATE
        genycaster_shutdown farcaster::transport_qt_network
        Qt6::Core Qt6::Gui Qt6::Network Qt6::Concurrent)
    target_compile_features(genycaster_background_shutdown_tests PRIVATE cxx_std_26)
    set_property(TARGET genycaster_background_shutdown_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    foreach(shutdownMode IN ITEMS quit close-window early-return success timeout
            gui-stop gui-success scope-reuse)
        add_test(NAME ui.background_shutdown.${shutdownMode}
            COMMAND genycaster_background_shutdown_tests ${shutdownMode})
        set_tests_properties(ui.background_shutdown.${shutdownMode} PROPERTIES
            TIMEOUT 8 LABELS "ui;network;shutdown"
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FATAL_WARNINGS=1")
    endforeach()
endif()

# Exercise QQmlApplicationEngine cleanup with an asynchronous Loader and
# fatal Qt warnings. The native root must be destroyed before its dispatcher.
qt_add_executable(farcaster_qml_engine_shutdown_tests
    qml_engine_shutdown_tests.cpp)
target_compile_features(farcaster_qml_engine_shutdown_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_qml_engine_shutdown_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
farcaster_enable_warnings(farcaster_qml_engine_shutdown_tests)
add_test(NAME ui.qml_engine_shutdown COMMAND farcaster_qml_engine_shutdown_tests)
set_tests_properties(ui.qml_engine_shutdown PROPERTIES
    TIMEOUT 15 LABELS "ui;qml;shutdown"
    ENVIRONMENT "QT_QPA_PLATFORM=minimal;QT_FATAL_WARNINGS=1")

# Deterministic C++ geometry tests. QML only retains Shape and color bindings;
# time-series sampling and coordinate generation are native and bounded.
qt_add_executable(farcaster_token_sparkline_tests
    token_sparkline_geometry_tests.cpp
    "${genycasterUiSourceDir}/render/token_sparkline_geometry.cpp"
    "${genycasterUiSourceDir}/render/token_sparkline_geometry.hpp")
target_compile_features(farcaster_token_sparkline_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_token_sparkline_tests PRIVATE
    Qt6::Core Qt6::Qml Qt6::Test)
farcaster_enable_warnings(farcaster_token_sparkline_tests)
add_test(NAME ui.token_sparkline_geometry COMMAND farcaster_token_sparkline_tests)
set_tests_properties(ui.token_sparkline_geometry PROPERTIES
    TIMEOUT 15 LABELS "ui;market;qt;rendering")

# A standalone real-Qt network/QObject test, independent of the giant
# executable and its C++ module graph. The local test server makes request
# batching, exact cast counts and unknown values deterministic.
qt_add_executable(farcaster_cast_views_tests
    cast_view_controller_tests.cpp
    "${genycasterUiSourceDir}/app/cast_view_controller.cpp"
    "${genycasterUiSourceDir}/app/cast_view_controller.hpp")
target_compile_features(farcaster_cast_views_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_cast_views_tests PRIVATE Qt6::Core Qt6::Network)
farcaster_enable_warnings(farcaster_cast_views_tests)
add_test(NAME ui.cast_views_contract COMMAND farcaster_cast_views_tests)
set_tests_properties(ui.cast_views_contract PROPERTIES
    LABELS "ui;analytics;qt"
    TIMEOUT 15)

# Actual Qt HTTP/lifecycle fixture for the browser reaction adapter.
# QML no longer owns or parses network replies.
qt_add_executable(farcaster_browser_reactions_tests
    browser_reaction_controller_tests.cpp
    "${genycasterUiSourceDir}/app/browser_reaction_controller.cpp"
    "${genycasterUiSourceDir}/app/browser_reaction_controller.hpp")
target_compile_features(farcaster_browser_reactions_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_browser_reactions_tests PRIVATE Qt6::Core Qt6::Network)
farcaster_enable_warnings(farcaster_browser_reactions_tests)
add_test(NAME ui.browser_reactions_contract COMMAND farcaster_browser_reactions_tests)
set_tests_properties(ui.browser_reactions_contract PROPERTIES
    LABELS "ui;reactions;qt;wasm"
    TIMEOUT 15)

# Browser-safe link/miniapp metadata relay with actual bounded HTTP tests.
qt_add_executable(farcaster_browser_link_metadata_tests
    browser_link_metadata_controller_tests.cpp
    "${genycasterUiSourceDir}/app/browser_link_metadata_controller.cpp"
    "${genycasterUiSourceDir}/app/browser_link_metadata_controller.hpp")
target_compile_features(farcaster_browser_link_metadata_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_browser_link_metadata_tests PRIVATE Qt6::Core Qt6::Network)
farcaster_enable_warnings(farcaster_browser_link_metadata_tests)
add_test(NAME ui.browser_link_metadata_contract
    COMMAND farcaster_browser_link_metadata_tests)
set_tests_properties(ui.browser_link_metadata_contract PROPERTIES
    LABELS "ui;metadata;qt;wasm"
    TIMEOUT 15)

qt_add_executable(farcaster_provider_translation_tests
    provider_translation_catalog_tests.cpp)
target_compile_features(farcaster_provider_translation_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_provider_translation_tests PRIVATE Qt6::Core)
target_compile_definitions(farcaster_provider_translation_tests PRIVATE
    GENYCASTER_QM_EN="${genycasterUiBinaryDir}/qml_en_US.qm"
    GENYCASTER_QM_FA="${genycasterUiBinaryDir}/qml_fa_IR.qm")
add_dependencies(farcaster_provider_translation_tests farcaster_translation_catalogs)
farcaster_enable_warnings(farcaster_provider_translation_tests)
add_test(NAME ui.provider_translation_contract COMMAND farcaster_provider_translation_tests)
set_tests_properties(ui.provider_translation_contract PROPERTIES
    LABELS "ui;i18n;provider;qt"
    TIMEOUT 15)

qt_add_executable(farcaster_provider_profiles_tests
    provider_profiles_controller_tests.cpp
    "${genycasterUiSourceDir}/app/provider_profiles_controller.cpp"
    "${genycasterUiSourceDir}/app/provider_profiles_controller.hpp")
target_compile_features(farcaster_provider_profiles_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_provider_profiles_tests PRIVATE Qt6::Core Qt6::Network Qt6::Gui)
target_compile_definitions(farcaster_provider_profiles_tests PRIVATE
    FARCASTER_HAS_SNAPCHAIN_READ=1
    FARCASTER_HAS_HYPERSNAP_PROVIDER=1
    FARCASTER_HAS_SNAPCHAIN_WRITE=1)
farcaster_enable_warnings(farcaster_provider_profiles_tests)
add_test(NAME ui.provider_profiles_contract COMMAND farcaster_provider_profiles_tests)
set_tests_properties(ui.provider_profiles_contract PROPERTIES
    LABELS "ui;provider;qt;offline" TIMEOUT 15)

qt_add_executable(farcaster_node_selection_tests
    node_selection_controller_tests.cpp
    "${PROJECT_SOURCE_DIR}/src/providers/hypersnap/node_selection_controller.cpp"
    "${genycasterUiSourceDir}/app/node_selection_controller.hpp")
target_compile_features(farcaster_node_selection_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_node_selection_tests PRIVATE Qt6::Core Qt6::Network)
farcaster_enable_warnings(farcaster_node_selection_tests)
add_test(NAME ui.node_selection_contract COMMAND farcaster_node_selection_tests)
set_tests_properties(ui.node_selection_contract PROPERTIES
    LABELS "ui;provider;qt;offline"
    TIMEOUT 15)

qt_add_executable(farcaster_browser_token_tests
    browser_token_preview_controller_tests.cpp
    "${genycasterUiSourceDir}/app/browser_token_preview_controller.cpp"
    "${genycasterUiSourceDir}/app/browser_token_preview_controller.hpp")
target_compile_features(farcaster_browser_token_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_browser_token_tests PRIVATE Qt6::Core Qt6::Network)
farcaster_enable_warnings(farcaster_browser_token_tests)
add_test(NAME ui.browser_token_contract COMMAND farcaster_browser_token_tests)
set_tests_properties(ui.browser_token_contract PROPERTIES
    LABELS "ui;market;qt;wasm"
    TIMEOUT 15)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(farcasterHeadlessPlatform "offscreen")
if(APPLE)
    # Qt 6.11's macOS offscreen plugin emits a fatal generic-font alias
    # warning before project QML loads; the minimal plugin is warning-clean.
    set(farcasterHeadlessPlatform "minimal")
endif()

add_test(NAME ui.smoke COMMAND genycaster --smoke-test)
set_tests_properties(ui.smoke PROPERTIES
    LABELS "ui;smoke"
    ENVIRONMENT
        "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
    TIMEOUT 30
)

add_test(
    NAME ui.localization_en_smoke
    COMMAND genycaster --smoke-test --language=en
)
set_tests_properties(ui.localization_en_smoke PROPERTIES
    LABELS "ui;smoke;localization;ltr"
    ENVIRONMENT
        "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
    TIMEOUT 30
)

add_test(
    NAME ui.localization_fa_smoke
    COMMAND genycaster
        --smoke-test
        --language=fa
        --regional-locale=fa_IR
        --calendar=jalali
        --timestamp-display=calendar
        --time-zone=Asia/Tehran
        --currency=USD
        --additional-currencies=IRT,EUR
)
set_tests_properties(ui.localization_fa_smoke PROPERTIES
    LABELS "ui;smoke;localization;rtl"
    ENVIRONMENT
        "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
    TIMEOUT 30
)

add_test(
    NAME ui.appearance_dark_smoke
    COMMAND genycaster --smoke-test --theme-mode=dark --color-theme=geny
)
set_tests_properties(ui.appearance_dark_smoke PROPERTIES
    LABELS "ui;smoke;appearance;dark"
    ENVIRONMENT
        "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
    TIMEOUT 30
)

add_test(
    NAME ui.appearance_light_smoke
    COMMAND genycaster --smoke-test --theme-mode=light --color-theme=ocean --reduce-motion=true
)
set_tests_properties(ui.appearance_light_smoke PROPERTIES
    LABELS "ui;smoke;appearance;light"
    ENVIRONMENT
        "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
    TIMEOUT 30
)

if(FARCASTER_ENABLE_HYPERSNAP_PROVIDER)
    add_test(
        NAME ui.runtime_smoke
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/ui/runtime_smoke.py"
            "$<TARGET_FILE:genycaster>"
    )
    set_tests_properties(ui.runtime_smoke PROPERTIES
        LABELS "ui;smoke;integration"
        ENVIRONMENT
            "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
        TIMEOUT 30
    )
endif()

add_executable(farcaster_openproof_connections_tests
    openproof_connections_tests.cpp
    "${genycasterUiSourceDir}/app/openproof_connections_controller.cpp"
    "${genycasterUiSourceDir}/app/openproof_connections_controller.hpp"
    "${genycasterUiSourceDir}/app/browser_openproof_auth.cpp"
    "${genycasterUiSourceDir}/app/browser_openproof_auth.hpp")
target_include_directories(farcaster_openproof_connections_tests PRIVATE
    "${genycasterUiSourceDir}/app")
target_link_libraries(farcaster_openproof_connections_tests PRIVATE
    Qt6::Core Qt6::Network Qt6::Gui)
farcaster_enable_warnings(farcaster_openproof_connections_tests)
qt_add_resources(farcaster_openproof_connections_tests openproof_connections_test_config
    PREFIX "/genycaster-config"
    BASE "${genycasterUiSourceDir}/config"
    FILES "${genycasterUiSourceDir}/config/public_endpoints.json")
add_test(NAME ui.openproof_connections COMMAND farcaster_openproof_connections_tests)
set_tests_properties(ui.openproof_connections PROPERTIES TIMEOUT 10
    LABELS "ui;openproof;connections;security")

add_executable(farcaster_channel_picker_controller_tests
    channel_picker_controller_tests.cpp
    "${genycasterUiSourceDir}/app/channel_picker_controller.cpp"
    "${genycasterUiSourceDir}/app/channel_picker_controller.hpp")
target_include_directories(farcaster_channel_picker_controller_tests PRIVATE
    "${genycasterUiSourceDir}/app")
target_link_libraries(farcaster_channel_picker_controller_tests PRIVATE
    Qt6::Core Qt6::Network)
farcaster_enable_warnings(farcaster_channel_picker_controller_tests)
add_test(NAME ui.channel_picker_controller COMMAND farcaster_channel_picker_controller_tests)
set_tests_properties(ui.channel_picker_controller PROPERTIES TIMEOUT 12
    LABELS "ui;channels;selection;network")

add_executable(farcaster_update_controller_tests
    update_controller_tests.cpp
    "${genycasterUiSourceDir}/app/update_controller.cpp"
    "${genycasterUiSourceDir}/app/update_controller.hpp"
    "${genycasterUiSourceDir}/app/update/install.cpp"
    "${genycasterUiSourceDir}/app/update/install.hpp")
target_include_directories(farcaster_update_controller_tests PRIVATE
    "${genycasterUiSourceDir}/app")
target_link_libraries(farcaster_update_controller_tests PRIVATE
    genycaster_release_identity
    Qt6::Core Qt6::Network Qt6::Gui)
add_test(NAME ui.update_controller COMMAND farcaster_update_controller_tests)
set_tests_properties(ui.update_controller PROPERTIES TIMEOUT 8 LABELS "ui;update;security")

add_executable(farcaster_browser_wheel_fallback_tests
    browser_wheel_fallback_tests.cpp)
target_link_libraries(farcaster_browser_wheel_fallback_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
add_test(NAME ui.browser_wheel_fallback COMMAND farcaster_browser_wheel_fallback_tests)
set_tests_properties(ui.browser_wheel_fallback PROPERTIES TIMEOUT 15
    LABELS "ui;scroll;wasm"
    ENVIRONMENT "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_BACKEND=software")

add_executable(farcaster_feed_rows_model_tests
    feed_rows_model_tests.cpp
    "${genycasterUiSourceDir}/app/feed_rows_model.cpp"
    "${genycasterUiSourceDir}/app/feed_rows_model.hpp")
target_compile_features(farcaster_feed_rows_model_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_feed_rows_model_tests PRIVATE
    Qt6::Core Qt6::Qml Qt6::Test)
farcaster_enable_warnings(farcaster_feed_rows_model_tests)
add_test(NAME ui.feed_rows_model COMMAND farcaster_feed_rows_model_tests)
set_tests_properties(ui.feed_rows_model PROPERTIES
    TIMEOUT 30 LABELS "ui;qt;model;read-pipeline")

add_executable(farcaster_media_scroll_tests
    media_scroll_tests.cpp
    "${genycasterUiSourceDir}/app/feed_rows_model.cpp"
    "${genycasterUiSourceDir}/app/feed_rows_model.hpp"
    "${genycasterUiSourceDir}/render/scene_graph_superellipse_item.cpp")
target_include_directories(farcaster_media_scroll_tests PRIVATE
    "${genycasterUiSourceDir}/render")
target_link_libraries(farcaster_media_scroll_tests PRIVATE
    farcaster::presentation Qt6::Test Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia)
target_compile_definitions(farcaster_media_scroll_tests PRIVATE
    MEDIA_COMPONENT_DIRECTORY="${genycasterUiSourceDir}/components"
    MEDIA_THEME_FILE="${genycasterUiSourceDir}/theme/Theme.qml")
add_dependencies(farcaster_media_scroll_tests farcaster_translation_catalogs)
qt_add_resources(farcaster_media_scroll_tests media_scroll_translations
    PREFIX "/i18n"
    BASE "${genycasterUiBinaryDir}"
    FILES ${farcasterTranslationQmFiles})
qt_add_resources(farcaster_media_scroll_tests media_scroll_icons
    PREFIX "/qt/qml/Farcaster/Client/assets/icons/genyleap"
    BASE "${genycasterUiSourceDir}/assets/icons/genyleap"
    FILES ${genyleapIconSystemFiles})
# Parse the complete native startup QML dependency chain with a real
# QQmlEngine. qmlcache compilation alone misses unknown attached objects
# such as Layout.minimumWidth without import QtQuick.Layouts.
add_test(NAME ui.qml_import_smoke COMMAND farcaster_media_scroll_tests)
set_tests_properties(ui.qml_import_smoke PROPERTIES TIMEOUT 20
    LABELS "ui;smoke;qml;native"
    ENVIRONMENT "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1;GENYCASTER_QML_IMPORT_SMOKE_ONLY=1")
add_test(NAME ui.media_scroll COMMAND farcaster_media_scroll_tests)
set_tests_properties(ui.media_scroll PROPERTIES TIMEOUT 45
    LABELS "ui;media;scroll"
    ENVIRONMENT "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1")
if(APPLE)
    add_test(NAME ui.media_scroll_metal COMMAND farcaster_media_scroll_tests)
    set_tests_properties(ui.media_scroll_metal PROPERTIES TIMEOUT 45 RUN_SERIAL TRUE
        LABELS "ui;media;scroll;metal"
        ENVIRONMENT "QT_QPA_PLATFORM=cocoa;QSG_RHI_BACKEND=metal;QT_QUICK_BACKEND=;QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1")
endif()
add_executable(farcaster_link_preview_shutdown_tests
    link_preview_shutdown_tests.cpp
    "${genycasterUiSourceDir}/app/link_preview_resolver.cpp")
target_include_directories(farcaster_link_preview_shutdown_tests PRIVATE
    "${genycasterUiSourceDir}/app")
target_link_libraries(farcaster_link_preview_shutdown_tests PRIVATE Qt6::Core Qt6::Network)
add_test(NAME ui.link_preview_shutdown COMMAND farcaster_link_preview_shutdown_tests)
set_tests_properties(ui.link_preview_shutdown PROPERTIES TIMEOUT 8
    LABELS "ui;network;shutdown")

add_executable(farcaster_emoji_ui_tests emoji_runtime_tests.cpp)
target_link_libraries(farcaster_emoji_ui_tests PRIVATE farcaster_emoji_runtime farcaster::provider_emoji Qt6::Test Qt6::Quick)
target_compile_definitions(farcaster_emoji_ui_tests PRIVATE
    EMOJI_COMPONENT_DIRECTORY="${genycasterUiSourceDir}/components"
    EMOJI_SOURCE_DIRECTORY="${genycasterUiSourceDir}/assets/emoji/lottie")
qt_add_resources(farcaster_emoji_ui_tests emoji_test_assets
    PREFIX "/qt/qml/Farcaster/Client/assets/emoji/lottie"
    BASE "${genycasterUiSourceDir}/assets/emoji/lottie"
    FILES ${notoEmojiLottieFiles})
add_test(NAME ui.emoji_runtime COMMAND farcaster_emoji_ui_tests)
set_tests_properties(ui.emoji_runtime PROPERTIES TIMEOUT 90
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1")
if(APPLE)
    add_test(NAME ui.emoji_runtime_metal COMMAND farcaster_emoji_ui_tests)
    set_tests_properties(ui.emoji_runtime_metal PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE
        ENVIRONMENT "QT_QPA_PLATFORM=cocoa;QSG_RHI_BACKEND=metal;QT_QUICK_BACKEND=;QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1")
endif()
