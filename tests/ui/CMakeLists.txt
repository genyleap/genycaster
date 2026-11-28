# Test targets belong to this directory so IDEs group them under tests/ui.
find_package(Qt6 REQUIRED COMPONENTS Test)

if(NOT CMAKE_CROSSCOMPILING)
    add_executable(genycaster_rendering_pacing_tests
        rendering_pacing_runtime.cpp
        "${CMAKE_SOURCE_DIR}/src/presentation/qt/rendering_controller.cpp"
        "${CMAKE_SOURCE_DIR}/src/presentation/qt/rendering_controller.hpp")
    set_target_properties(genycaster_rendering_pacing_tests PROPERTIES AUTOMOC ON)
    target_include_directories(genycaster_rendering_pacing_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/src/presentation/qt")
    target_link_libraries(genycaster_rendering_pacing_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::Test)
    target_compile_features(genycaster_rendering_pacing_tests PRIVATE cxx_std_26)
    farcaster_enable_warnings(genycaster_rendering_pacing_tests)
    add_test(NAME ui.rendering_pacing COMMAND genycaster_rendering_pacing_tests --synthetic)
    set_tests_properties(ui.rendering_pacing PROPERTIES TIMEOUT 20 LABELS "ui;rendering;performance"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QSG_RENDER_LOOP=basic")
endif()

# Exercise the same issuer/subject/FID admission policy used by the native
# account controller. These are source-level QtCore tests, not live OPP E2E.
if(NOT EMSCRIPTEN AND NOT ANDROID)
    add_executable(genycaster_account_session_principal_tests
        ../providers/account_session_principal_tests.cpp)
    target_link_libraries(genycaster_account_session_principal_tests PRIVATE Qt6::Core)
    target_compile_features(genycaster_account_session_principal_tests PRIVATE cxx_std_26)
    farcaster_enable_warnings(genycaster_account_session_principal_tests)
    add_test(NAME ui.account_session_principal
        COMMAND genycaster_account_session_principal_tests)
    set_tests_properties(ui.account_session_principal PROPERTIES
        TIMEOUT 8 LABELS "ui;identity;security;account-session")
endif()

# Real Qt event-loop lifecycle regression; use an isolated X11 display.
if(TARGET genycaster_desktop_instance AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(_genycasterXvfbRun xvfb-run)
    if(_genycasterXvfbRun)
        find_package(X11 QUIET)
        if(X11_FOUND)
            add_executable(genycaster_desktop_background_tests ../unit/desktop_background_runtime.cpp)
            target_include_directories(genycaster_desktop_background_tests PRIVATE
                "${CMAKE_SOURCE_DIR}/ui/app/platform")
            target_link_libraries(genycaster_desktop_background_tests PRIVATE
                genycaster_desktop_instance X11::X11)
            target_compile_definitions(genycaster_desktop_background_tests PRIVATE GENYCASTER_TEST_X11)
            target_compile_features(genycaster_desktop_background_tests PRIVATE cxx_std_26)
            foreach(_mode IN ITEMS no-tray tray)
                if(_mode STREQUAL "tray")
                    set(_args --tray)
                else()
                    set(_args)
                endif()
                add_test(NAME ui.desktop_background.${_mode}
                    COMMAND "${_genycasterXvfbRun}" -a $<TARGET_FILE:genycaster_desktop_background_tests> ${_args})
                set_tests_properties(ui.desktop_background.${_mode} PROPERTIES
                    TIMEOUT 12 LABELS "ui;desktop;lifecycle" ENVIRONMENT "QT_QPA_PLATFORM=xcb")
            endforeach()
        endif()
    endif()
endif()

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
    # Reproduce the macOS HyperSnap click crash: abort() may synchronously
    # emit finished(), clearing the guarded reply before refresh() returns.
    qt_add_executable(genycaster_native_registration_refresh_tests
        native_registration_refresh_tests.cpp
        "${genycasterUiSourceDir}/app/identity/native_registration.cpp"
        "${genycasterUiSourceDir}/app/identity/native_registration.hpp"
        "${genycasterUiSourceDir}/app/identity/hypersnap_registration_transport.cpp"
        "${genycasterUiSourceDir}/app/identity/hypersnap_registration_transport.hpp")
    target_link_libraries(genycaster_native_registration_refresh_tests PRIVATE
        Qt6::Core Qt6::Network Qt6::Test)
    target_compile_features(genycaster_native_registration_refresh_tests PRIVATE cxx_std_26)
    farcaster_enable_warnings(genycaster_native_registration_refresh_tests)
    add_test(NAME ui.native_registration_refresh
        COMMAND genycaster_native_registration_refresh_tests)
    set_tests_properties(ui.native_registration_refresh PROPERTIES
        TIMEOUT 15 LABELS "ui;hypersnap;registration;network;regression")

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
    # Do not dispatch queued HTTP completions during reactor destruction:
    # this reproduces the ARM64 macOS exit crash with one active and two
    # admitted queued requests, without requiring any external provider.
    add_executable(genycaster_async_reactor_shutdown_tests
        async_reactor_shutdown_tests.cpp)
    target_link_libraries(genycaster_async_reactor_shutdown_tests PRIVATE
        farcaster::transport_qt_network Qt6::Core Qt6::Network)
    target_compile_features(genycaster_async_reactor_shutdown_tests PRIVATE cxx_std_26)
    set_property(TARGET genycaster_async_reactor_shutdown_tests
        PROPERTY CXX_SCAN_FOR_MODULES ON)
    add_test(NAME ui.async_reactor_shutdown
        COMMAND genycaster_async_reactor_shutdown_tests)
    set_tests_properties(ui.async_reactor_shutdown PROPERTIES
        TIMEOUT 8 LABELS "ui;network;shutdown;cross-platform")
endif()

# Validate startup provider precedence and credential containment with real
# Qt settings; this target cannot load or log persisted API credentials.
add_executable(genycaster_provider_endpoints_tests provider_endpoints_tests.cpp)
target_link_libraries(genycaster_provider_endpoints_tests PRIVATE
    genycaster_ui_bootstrap Qt6::Core)
target_compile_features(genycaster_provider_endpoints_tests PRIVATE cxx_std_26)
set_property(TARGET genycaster_provider_endpoints_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
add_test(NAME ui.provider_endpoints COMMAND genycaster_provider_endpoints_tests)
set_tests_properties(ui.provider_endpoints PROPERTIES
    TIMEOUT 10 LABELS "ui;provider;configuration;security")

# Verify that provider profile configuration is independent of the runtime
# execution graph and never persists Read credentials or reroutes Write access.
add_executable(genycaster_provider_profiles_tests provider_profiles_tests.cpp)
target_link_libraries(genycaster_provider_profiles_tests PRIVATE
    genycaster_ui_bootstrap Qt6::Core)
target_compile_features(genycaster_provider_profiles_tests PRIVATE cxx_std_26)
set_property(TARGET genycaster_provider_profiles_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
add_test(NAME ui.provider_profiles_policy COMMAND genycaster_provider_profiles_tests)
set_tests_properties(ui.provider_profiles_policy PROPERTIES
    TIMEOUT 10 LABELS "ui;provider;profiles;security")

# Ensure protocol-safe canonical signed-message delivery: explicit writer
# selection must not inherit an unrelated Read adapter's network origin.
add_executable(genycaster_provider_routes_tests provider_routes_tests.cpp)
target_link_libraries(genycaster_provider_routes_tests PRIVATE
    genycaster_ui_bootstrap Qt6::Core)
target_compile_features(genycaster_provider_routes_tests PRIVATE cxx_std_26)
set_property(TARGET genycaster_provider_routes_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
add_test(NAME ui.provider_routes_policy COMMAND genycaster_provider_routes_tests)
set_tests_properties(ui.provider_routes_policy PROPERTIES
    TIMEOUT 10 LABELS "ui;provider;routing;security")

# Authentication remains independent of protocol Read/Write routing.
add_executable(genycaster_bootstrap_auth_tests bootstrap_auth_tests.cpp)
target_link_libraries(genycaster_bootstrap_auth_tests PRIVATE
    genycaster_ui_bootstrap Qt6::Core)
target_compile_features(genycaster_bootstrap_auth_tests PRIVATE cxx_std_26)
set_property(TARGET genycaster_bootstrap_auth_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
add_test(NAME ui.bootstrap_auth COMMAND genycaster_bootstrap_auth_tests)
set_tests_properties(ui.bootstrap_auth PROPERTIES
    TIMEOUT 10 LABELS "ui;authentication;security")

# QML context names form part of the application ABI. Reject invalid batches
# before a partially initialized root object tree can become visible.
add_executable(genycaster_bootstrap_context_tests bootstrap_context_tests.cpp)
target_link_libraries(genycaster_bootstrap_context_tests PRIVATE
    genycaster_ui_bootstrap Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
target_compile_features(genycaster_bootstrap_context_tests PRIVATE cxx_std_26)
set_property(TARGET genycaster_bootstrap_context_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
add_test(NAME ui.bootstrap_context COMMAND genycaster_bootstrap_context_tests)
set_tests_properties(ui.bootstrap_context PROPERTIES
    TIMEOUT 10 LABELS "ui;qml;bootstrap"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

# Wallet URLs are validated without starting a browser; the OS dispatch is
# separately guarded by the application shutdown boundary.
add_executable(genycaster_wallet_launch_tests wallet_launch_tests.cpp)
target_link_libraries(genycaster_wallet_launch_tests PRIVATE
    genycaster_ui_bootstrap Qt6::Core Qt6::Gui)
target_compile_features(genycaster_wallet_launch_tests PRIVATE cxx_std_26)
set_property(TARGET genycaster_wallet_launch_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
add_test(NAME ui.wallet_launch_policy COMMAND genycaster_wallet_launch_tests)
set_tests_properties(ui.wallet_launch_policy PROPERTIES
    TIMEOUT 10 LABELS "ui;wallet;security")

if(TARGET genycaster_bootstrap_worker)
    add_executable(genycaster_bootstrap_worker_tests bootstrap_worker_tests.cpp)
    target_link_libraries(genycaster_bootstrap_worker_tests PRIVATE
        genycaster_bootstrap_worker)
    target_compile_features(genycaster_bootstrap_worker_tests PRIVATE cxx_std_26)
    set_property(TARGET genycaster_bootstrap_worker_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    add_test(NAME ui.bootstrap_worker COMMAND genycaster_bootstrap_worker_tests)
    set_tests_properties(ui.bootstrap_worker PROPERTIES
        TIMEOUT 10 LABELS "ui;runtime;shutdown")
endif()

if(TARGET genycaster_bootstrap_teardown)
    add_executable(genycaster_bootstrap_teardown_tests bootstrap_teardown_tests.cpp)
    target_link_libraries(genycaster_bootstrap_teardown_tests PRIVATE
        genycaster_bootstrap_teardown Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
    target_compile_features(genycaster_bootstrap_teardown_tests PRIVATE cxx_std_26)
    set_property(TARGET genycaster_bootstrap_teardown_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    add_test(NAME ui.bootstrap_teardown COMMAND genycaster_bootstrap_teardown_tests)
    set_tests_properties(ui.bootstrap_teardown PROPERTIES
        TIMEOUT 12 LABELS "ui;qml;shutdown"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
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
    "${genycasterUiSourceDir}/render/sparkline.cpp"
    "${genycasterUiSourceDir}/render/sparkline.hpp")
target_compile_features(farcaster_token_sparkline_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_token_sparkline_tests PRIVATE
    Qt6::Core Qt6::Qml Qt6::Test)
farcaster_enable_warnings(farcaster_token_sparkline_tests)
add_test(NAME ui.token_sparkline_geometry COMMAND farcaster_token_sparkline_tests)
set_tests_properties(ui.token_sparkline_geometry PROPERTIES
    TIMEOUT 15 LABELS "ui;market;qt;rendering")

# The production scene-graph generators are compiled into this narrowly
# scoped native test. Exercise giant but representable QML dimensions without
# submitting GPU work, to catch floating-to-int tessellation UB under UBSan.
qt_add_executable(farcaster_render_geometry_bounds_tests
    render_geometry_bounds_tests.cpp
    "${genycasterUiSourceDir}/render/stroke.cpp"
    "${genycasterUiSourceDir}/render/stroke.hpp"
    "${genycasterUiSourceDir}/render/superellipse.cpp"
    "${genycasterUiSourceDir}/render/superellipse.hpp"
    "${genycasterUiSourceDir}/render/geometry_safety.hpp")
target_compile_features(farcaster_render_geometry_bounds_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_render_geometry_bounds_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml)
farcaster_enable_warnings(farcaster_render_geometry_bounds_tests)
add_test(NAME ui.render_geometry_bounds COMMAND farcaster_render_geometry_bounds_tests)
set_tests_properties(ui.render_geometry_bounds PROPERTIES
    TIMEOUT 20 LABELS "ui;qt;rendering;safety"
    ENVIRONMENT "QT_QPA_PLATFORM=minimal;QT_QUICK_BACKEND=software")

# Check untrusted multipart gateway JSON numbers before converting to an
# unsigned C++ integer. This small test intentionally needs only QtCore.
qt_add_executable(farcaster_media_json_numbers_tests media_json_numbers_tests.cpp)
target_compile_features(farcaster_media_json_numbers_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_media_json_numbers_tests PRIVATE Qt6::Core)
farcaster_enable_warnings(farcaster_media_json_numbers_tests)
add_test(NAME ui.media_json_numbers COMMAND farcaster_media_json_numbers_tests)
set_tests_properties(ui.media_json_numbers PROPERTIES
    TIMEOUT 10 LABELS "ui;qt;media;security;safety")

if(TARGET farcaster_transport_qt_media)
    # Drive malformed multipart JSON through the actual signed-media adapter,
    # not only its parser helper; no external network or private signer keys.
    qt_add_executable(farcaster_media_gateway_protocol_tests media_gateway_protocol_tests.cpp)
    target_compile_features(farcaster_media_gateway_protocol_tests PRIVATE cxx_std_26)
    target_link_libraries(farcaster_media_gateway_protocol_tests PRIVATE
        farcaster::transport_qt_media Qt6::Core)
    set_property(TARGET farcaster_media_gateway_protocol_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_media_gateway_protocol_tests)
    add_test(NAME ui.media_gateway_protocol COMMAND farcaster_media_gateway_protocol_tests)
    set_tests_properties(ui.media_gateway_protocol PROPERTIES
        TIMEOUT 15 LABELS "ui;qt;media;security;safety")
endif()

# A standalone real-Qt network/QObject test, independent of the giant
# executable and its C++ module graph. The local test server makes request
# batching, exact cast counts and unknown values deterministic.
qt_add_executable(farcaster_cast_views_tests
    cast_view_controller_tests.cpp
    "${genycasterUiSourceDir}/app/social/cast_views.cpp"
    "${genycasterUiSourceDir}/app/social/cast_views.hpp")
target_compile_features(farcaster_cast_views_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_cast_views_tests PRIVATE Qt6::Core Qt6::Network)
farcaster_enable_warnings(farcaster_cast_views_tests)
add_test(NAME ui.cast_views_contract COMMAND farcaster_cast_views_tests)
set_tests_properties(ui.cast_views_contract PROPERTIES
    LABELS "ui;analytics;qt"
    TIMEOUT 15)

# Provider-scoped, period-accurate statistics from a deterministic local HTTP node.
# Distinguishes publishing activity from received engagement, rejects account
# leakage and never invents Snapchain raw-message engagement counters.
qt_add_executable(farcaster_stats_controller_tests
    stats_controller_tests.cpp
    "${genycasterUiSourceDir}/app/social/stats.cpp"
    "${genycasterUiSourceDir}/app/social/stats.hpp")
target_compile_features(farcaster_stats_controller_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_stats_controller_tests PRIVATE Qt6::Core Qt6::Network)
farcaster_enable_warnings(farcaster_stats_controller_tests)
add_test(NAME ui.stats_controller_contract COMMAND farcaster_stats_controller_tests)
set_tests_properties(ui.stats_controller_contract PROPERTIES
    LABELS "ui;analytics;provider;qt"
    TIMEOUT 15)

if(NOT EMSCRIPTEN AND NOT ANDROID)
    # Reproduce the Visual -> Markdown -> Preview -> Visual attachment loss.
    # The same typed MediaBlock must survive Markdown edits until Remove.
    qt_add_executable(farcaster_rich_composer_modes_tests
        rich_composer_modes_tests.cpp)
    target_link_libraries(farcaster_rich_composer_modes_tests PRIVATE
        farcaster::presentation Qt6::Core)
    target_compile_features(farcaster_rich_composer_modes_tests PRIVATE cxx_std_26)
    set_property(TARGET farcaster_rich_composer_modes_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_rich_composer_modes_tests)
    add_test(NAME ui.rich_composer_modes COMMAND farcaster_rich_composer_modes_tests)
    set_tests_properties(ui.rich_composer_modes PROPERTIES
        TIMEOUT 20 LABELS "ui;composer;media;regression")
endif()

# Actual Qt HTTP/lifecycle fixture for the browser reaction adapter.
# QML no longer owns or parses network replies.
qt_add_executable(farcaster_browser_reactions_tests
    browser_reaction_controller_tests.cpp
    "${genycasterUiSourceDir}/app/browser/reactions.cpp"
    "${genycasterUiSourceDir}/app/browser/reactions.hpp")
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
    "${genycasterUiSourceDir}/app/browser/link_metadata.cpp"
    "${genycasterUiSourceDir}/app/browser/link_metadata.hpp")
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
    "${genycasterUiSourceDir}/app/settings/providers.cpp"
    "${genycasterUiSourceDir}/app/settings/providers.hpp")
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
    "${genycasterUiSourceDir}/app/network/node_selection.hpp")
target_compile_features(farcaster_node_selection_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_node_selection_tests PRIVATE Qt6::Core Qt6::Network)
farcaster_enable_warnings(farcaster_node_selection_tests)
add_test(NAME ui.node_selection_contract COMMAND farcaster_node_selection_tests)
set_tests_properties(ui.node_selection_contract PROPERTIES
    LABELS "ui;provider;qt;offline"
    TIMEOUT 15)

qt_add_executable(farcaster_browser_token_tests
    browser_token_preview_controller_tests.cpp
    "${genycasterUiSourceDir}/app/browser/token_preview.cpp"
    "${genycasterUiSourceDir}/app/browser/token_preview.hpp")
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

add_test(NAME ui.setup_layout COMMAND genycaster
    --visual-test --open-initial-setup --exercise-setup-layout --language=fa
    "--settings-path=${CMAKE_CURRENT_BINARY_DIR}/setup-layout-test.ini")
set_tests_properties(ui.setup_layout PROPERTIES
    LABELS "ui;qml;localization"
    ENVIRONMENT
        "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1"
    TIMEOUT 20)

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
    "${genycasterUiSourceDir}/app/identity/connections.cpp"
    "${genycasterUiSourceDir}/app/identity/connections.hpp"
    "${genycasterUiSourceDir}/app/identity/browser_auth.cpp"
    "${genycasterUiSourceDir}/app/identity/browser_auth.hpp")
target_include_directories(farcaster_openproof_connections_tests PRIVATE
    "${genycasterUiSourceDir}/app")
# This target recompiles the production connections.cpp independently.
# Its native secure-session proof uses QtConcurrent; the executable's link
# dependencies do not propagate to standalone test binaries.
target_link_libraries(farcaster_openproof_connections_tests PRIVATE
    Qt6::Core Qt6::Network Qt6::Gui Qt6::Concurrent)
farcaster_enable_warnings(farcaster_openproof_connections_tests)
set_source_files_properties("${CMAKE_BINARY_DIR}/generated/public_endpoints.json"
    PROPERTIES QT_RESOURCE_ALIAS "public_endpoints.json")
qt_add_resources(farcaster_openproof_connections_tests openproof_connections_test_config
    PREFIX "/genycaster-config"
    FILES "${CMAKE_BINARY_DIR}/generated/public_endpoints.json")
add_test(NAME ui.openproof_connections COMMAND farcaster_openproof_connections_tests)
set_tests_properties(ui.openproof_connections PROPERTIES TIMEOUT 10
    LABELS "ui;openproof;connections;security")

# Explicit opt-in: run against a real OS-backed keychain, never production keys.
if(NOT EMSCRIPTEN AND NOT ANDROID AND TARGET farcaster_platform_signer)
    find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
    add_executable(genycaster_secure_vault_tests secure_vault_tests.cpp)
    target_link_libraries(genycaster_secure_vault_tests PRIVATE
        Qt6::Core farcaster::platform_signer OpenSSL::Crypto)
    target_compile_features(genycaster_secure_vault_tests PRIVATE cxx_std_26)
    set_property(TARGET genycaster_secure_vault_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(genycaster_secure_vault_tests)
    add_test(NAME ui.secure_vault_live COMMAND genycaster_secure_vault_tests)
    set_tests_properties(ui.secure_vault_live PROPERTIES SKIP_RETURN_CODE 77
        TIMEOUT 90 LABELS "security;integration;secure-vault")
endif()

add_executable(farcaster_channel_picker_controller_tests
    channel_picker_controller_tests.cpp
    "${genycasterUiSourceDir}/app/social/channels.cpp"
    "${genycasterUiSourceDir}/app/social/channels.hpp")
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
    "${genycasterUiSourceDir}/app/update/controller.cpp"
    "${genycasterUiSourceDir}/app/update/controller.hpp")
target_include_directories(farcaster_update_controller_tests PRIVATE
    "${genycasterUiSourceDir}/app")
target_link_libraries(farcaster_update_controller_tests PRIVATE
    genycaster_release_identity genycaster_update_install
    Qt6::Core Qt6::Network Qt6::Gui)
add_test(NAME ui.update_controller COMMAND farcaster_update_controller_tests)
set_tests_properties(ui.update_controller PROPERTIES TIMEOUT 8 LABELS "ui;update;security")

add_executable(farcaster_browser_wheel_fallback_tests
    browser_wheel_fallback_tests.cpp)
target_compile_features(farcaster_browser_wheel_fallback_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_browser_wheel_fallback_tests PRIVATE
    genycaster_web_wheel
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
add_test(NAME ui.browser_wheel_fallback COMMAND farcaster_browser_wheel_fallback_tests)
set_tests_properties(ui.browser_wheel_fallback PROPERTIES TIMEOUT 15
    LABELS "ui;scroll;wasm"
    ENVIRONMENT "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_BACKEND=software")

add_executable(farcaster_feed_rows_model_tests
    feed_rows_model_tests.cpp
    "${genycasterUiSourceDir}/app/social/feed_rows.cpp"
    "${genycasterUiSourceDir}/app/social/feed_rows.hpp")
target_compile_features(farcaster_feed_rows_model_tests PRIVATE cxx_std_26)
target_link_libraries(farcaster_feed_rows_model_tests PRIVATE
    Qt6::Core Qt6::Qml Qt6::Test)
farcaster_enable_warnings(farcaster_feed_rows_model_tests)
add_test(NAME ui.feed_rows_model COMMAND farcaster_feed_rows_model_tests)
set_tests_properties(ui.feed_rows_model PROPERTIES
    TIMEOUT 30 LABELS "ui;qt;model;read-pipeline")

# Manual native microbenchmark only. Do not gate CI on noisy wall-clock
# thresholds or build this target as part of ordinary UI tests.
add_executable(farcaster_feed_performance_benchmark EXCLUDE_FROM_ALL
    feed_performance_benchmark.cpp
    "${genycasterUiSourceDir}/app/social/feed_rows.cpp"
    "${genycasterUiSourceDir}/app/social/feed_rows.hpp")
target_compile_features(farcaster_feed_performance_benchmark PRIVATE cxx_std_26)
target_link_libraries(farcaster_feed_performance_benchmark PRIVATE
    Qt6::Core Qt6::Qml)
farcaster_enable_warnings(farcaster_feed_performance_benchmark)

# Real Qt 6.12 viewport fixture: renders the production Magazine QML with
# its native model and verifies that hundreds of source casts never create
# hundreds of QML card objects during scroll and reverse scroll.
qt_add_executable(farcaster_masonry_view_runtime_tests
    masonry_view_runtime_tests.cpp
    "${genycasterUiSourceDir}/app/social/feed_rows.cpp"
    "${genycasterUiSourceDir}/app/social/feed_rows.hpp")
target_compile_features(farcaster_masonry_view_runtime_tests PRIVATE cxx_std_26)
target_compile_definitions(farcaster_masonry_view_runtime_tests PRIVATE
    GENYCASTER_MASONRY_QML_DIR="${genycasterUiSourceDir}/components/feed")
target_link_libraries(farcaster_masonry_view_runtime_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Test)
farcaster_enable_warnings(farcaster_masonry_view_runtime_tests)
add_test(NAME ui.masonry_window_runtime COMMAND farcaster_masonry_view_runtime_tests)
set_tests_properties(ui.masonry_window_runtime PROPERTIES
    TIMEOUT 20 LABELS "ui;qt;scroll;magazine"
    ENVIRONMENT "QT_QPA_PLATFORM=minimal;QT_QUICK_BACKEND=software")

# Responsive single-image layouts use the *window* breakpoint, decode exact
# portrait ratios after the flick and never cap an image to a shallow card.
qt_add_executable(farcaster_mobile_gallery_runtime_tests
    mobile_gallery_runtime_tests.cpp)
target_compile_features(farcaster_mobile_gallery_runtime_tests PRIVATE cxx_std_26)
target_compile_definitions(farcaster_mobile_gallery_runtime_tests PRIVATE
    GENYCASTER_MEDIA_QML_DIR="${genycasterUiSourceDir}/components/media")
target_link_libraries(farcaster_mobile_gallery_runtime_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
farcaster_enable_warnings(farcaster_mobile_gallery_runtime_tests)
add_test(NAME ui.mobile_portrait_gallery COMMAND farcaster_mobile_gallery_runtime_tests)
set_tests_properties(ui.mobile_portrait_gallery PROPERTIES
    TIMEOUT 20 LABELS "ui;qt;mobile;media;scroll"
    ENVIRONMENT "QT_QPA_PLATFORM=minimal;QT_QUICK_BACKEND=software")

# Exercise production QML handlers and raster lifecycle with real Qt Quick
# events, without compiling the entire client or requiring a live account.
if(NOT CMAKE_CROSSCOMPILING)
    find_program(GENYCASTER_QML_TEST_RUNNER NAMES qmltestrunner
        HINTS "${Qt6_DIR}/../../../bin" NO_DEFAULT_PATH)
    if(GENYCASTER_QML_TEST_RUNNER)
        foreach(fixture IN ITEMS media_gallery_zoom_runtime shared_raster_geometry_runtime drawer_backdrop_runtime supporting_preview_lifecycle_runtime)
            add_test(NAME ui.${fixture}
                COMMAND "${CMAKE_COMMAND}" -E env
                    "QML_TEST_RUNNER=${GENYCASTER_QML_TEST_RUNNER}"
                    "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/${fixture}.py")
            set_tests_properties(ui.${fixture} PROPERTIES
                TIMEOUT 30 LABELS "ui;qt;media;scroll")
        endforeach()
    endif()
endif()

add_executable(farcaster_media_scroll_tests
    media_scroll_tests.cpp
    "${genycasterUiSourceDir}/app/social/feed_rows.cpp"
    "${genycasterUiSourceDir}/app/social/feed_rows.hpp"
    "${genycasterUiSourceDir}/render/sparkline.cpp"
    "${genycasterUiSourceDir}/render/sparkline.hpp"
    "${genycasterUiSourceDir}/render/superellipse.cpp")
target_include_directories(farcaster_media_scroll_tests PRIVATE
    "${genycasterUiSourceDir}/render")
target_link_libraries(farcaster_media_scroll_tests PRIVATE
    farcaster::presentation genycaster_web_wheel
    Qt6::Test Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia)
# Source-loaded native QML fixtures must mirror the flattened public resource
# names, even when production sources are grouped into feature directories.
set(genycasterPublicQmlFixtureDir "${CMAKE_CURRENT_BINARY_DIR}/qml-public-components")
# Reconfigure when a public alias is added, renamed or moved. file(READ)
# alone does not make Ninja watch the QML source map.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${genycasterUiSourceDir}/components/source_paths.json")
file(READ "${genycasterUiSourceDir}/components/source_paths.json" genycasterQmlSourcePaths)
string(JSON genycasterQmlSourceCount LENGTH "${genycasterQmlSourcePaths}")
if(genycasterQmlSourceCount LESS 1)
    message(FATAL_ERROR "QML public-source map must not be empty")
endif()
math(EXPR genycasterLastQmlSourceIndex "${genycasterQmlSourceCount} - 1")
foreach(genycasterQmlIndex RANGE 0 ${genycasterLastQmlSourceIndex})
    string(JSON genycasterQmlPublicName MEMBER "${genycasterQmlSourcePaths}" ${genycasterQmlIndex})
    string(JSON genycasterQmlPhysicalName GET "${genycasterQmlSourcePaths}" "${genycasterQmlPublicName}")
    configure_file("${genycasterUiSourceDir}/components/${genycasterQmlPhysicalName}"
        "${genycasterPublicQmlFixtureDir}/${genycasterQmlPublicName}" COPYONLY)
endforeach()
configure_file("${genycasterUiSourceDir}/components/composer/ProCastPolicy.js"
    "${genycasterPublicQmlFixtureDir}/ProCastPolicy.js" COPYONLY)
# The public NeonHalo.qml imports its JS geometry sibling by relative path.
# Production flattens that alias; the standalone native QML fixture must too.
configure_file("${genycasterUiSourceDir}/components/visual/NeonGeometry.js"
    "${genycasterPublicQmlFixtureDir}/NeonGeometry.js" COPYONLY)
unset(genycasterQmlIndex)
unset(genycasterQmlPublicName)
unset(genycasterQmlPhysicalName)
unset(genycasterQmlSourcePaths)
unset(genycasterQmlSourceCount)
unset(genycasterLastQmlSourceIndex)

target_compile_definitions(farcaster_media_scroll_tests PRIVATE
    MEDIA_COMPONENT_DIRECTORY="${genycasterPublicQmlFixtureDir}"
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
# Match the material resources used by the real source-loaded delegates.
qt_add_resources(farcaster_media_scroll_tests media_scroll_effects
    PREFIX "/genycaster/effects"
    BASE "${genycasterUiSourceDir}/shaders"
    FILES
        "${genycasterUiSourceDir}/shaders/transparent.png"
        "${genycasterUiSourceDir}/shaders/neon.frag.qsb"
        "${genycasterUiSourceDir}/shaders/reflection.frag.qsb"
        "${genycasterUiSourceDir}/shaders/gaussian.frag.qsb"
        "${genycasterUiSourceDir}/shaders/reflection-gaussian.frag.qsb")
# Verify precise appearance NOTIFY fanout using real Qt QSignalSpy without
# importing production-only wallet types into the isolated media fixture.
add_test(NAME ui.appearance_notify_smoke COMMAND farcaster_media_scroll_tests)
set_tests_properties(ui.appearance_notify_smoke PROPERTIES TIMEOUT 15
    LABELS "ui;appearance;qt;regression"
    ENVIRONMENT "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic;GENYCASTER_APPEARANCE_NOTIFY_SMOKE_ONLY=1")

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
add_test(NAME ui.profile_wheel COMMAND farcaster_media_scroll_tests)
set_tests_properties(ui.profile_wheel PROPERTIES TIMEOUT 15
    LABELS "ui;scroll;wasm"
    ENVIRONMENT "QT_QPA_PLATFORM=${farcasterHeadlessPlatform};QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1;GENYCASTER_PROFILE_WHEEL_SMOKE_ONLY=1")
if(APPLE)
    add_test(NAME ui.media_scroll_metal COMMAND farcaster_media_scroll_tests)
    set_tests_properties(ui.media_scroll_metal PROPERTIES TIMEOUT 45 RUN_SERIAL TRUE
        LABELS "ui;media;scroll;metal"
        ENVIRONMENT "QT_QPA_PLATFORM=cocoa;QSG_RHI_BACKEND=metal;QT_QUICK_BACKEND=;QT_QUICK_CONTROLS_STYLE=Basic;QT_FATAL_WARNINGS=1")
endif()
add_executable(farcaster_link_preview_shutdown_tests
    link_preview_shutdown_tests.cpp
    "${genycasterUiSourceDir}/app/network/link_preview.cpp")
target_include_directories(farcaster_link_preview_shutdown_tests PRIVATE
    "${genycasterUiSourceDir}/app")
target_link_libraries(farcaster_link_preview_shutdown_tests PRIVATE Qt6::Core Qt6::Network)
add_test(NAME ui.link_preview_shutdown COMMAND farcaster_link_preview_shutdown_tests)
set_tests_properties(ui.link_preview_shutdown PROPERTIES TIMEOUT 8
    LABELS "ui;network;shutdown")

add_executable(farcaster_emoji_ui_tests emoji_runtime_tests.cpp)
target_link_libraries(farcaster_emoji_ui_tests PRIVATE farcaster_emoji_runtime farcaster::provider_emoji Qt6::Test Qt6::Quick)
target_compile_definitions(farcaster_emoji_ui_tests PRIVATE
    EMOJI_COMPONENT_DIRECTORY="${genycasterUiSourceDir}/components/media"
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
