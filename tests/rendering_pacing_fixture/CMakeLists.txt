cmake_minimum_required(VERSION 3.24)
project(GenycasterRenderingPacingFixture LANGUAGES CXX)
find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick Test)
set(CMAKE_AUTOMOC ON)
set(root "${CMAKE_CURRENT_LIST_DIR}/../..")
add_executable(genycaster_rendering_pacing_fixture
  "${root}/tests/ui/rendering_pacing_runtime.cpp"
  "${root}/src/presentation/qt/rendering_controller.cpp"
  "${root}/src/presentation/qt/rendering_controller.hpp")
target_include_directories(genycaster_rendering_pacing_fixture PRIVATE "${root}/src/presentation/qt")
target_compile_features(genycaster_rendering_pacing_fixture PRIVATE cxx_std_23)
target_link_libraries(genycaster_rendering_pacing_fixture PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::Test)
enable_testing()
add_executable(genycaster_viewport_work_budget_fixture "${root}/tests/ui/viewport_work_budget_tests.cpp")
set_target_properties(genycaster_viewport_work_budget_fixture PROPERTIES AUTOMOC OFF)
target_compile_features(genycaster_viewport_work_budget_fixture PRIVATE cxx_std_20)
add_test(NAME viewport_work_budget COMMAND genycaster_viewport_work_budget_fixture)
add_executable(genycaster_feed_rows_fixture
  "${root}/tests/ui/feed_rows_model_tests.cpp"
  "${root}/ui/app/social/feed_rows.cpp"
  "${root}/ui/app/social/feed_rows.hpp")
target_compile_features(genycaster_feed_rows_fixture PRIVATE cxx_std_23)
target_link_libraries(genycaster_feed_rows_fixture PRIVATE Qt6::Core Qt6::Qml Qt6::Test)
add_test(NAME feed_rows_model COMMAND genycaster_feed_rows_fixture)
set_tests_properties(viewport_work_budget feed_rows_model PROPERTIES TIMEOUT 30)
add_test(NAME pacing_software COMMAND genycaster_rendering_pacing_fixture --synthetic)
set_tests_properties(pacing_software PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QSG_RENDER_LOOP=basic" TIMEOUT 20)
# Keep the standalone cache usable across source revisions. Register
# real QML-engine and JS contract tests here as well as native C++ tests.
add_executable(genycaster_profile_row_order_fixture "${root}/tests/ui/profile_row_order_tests.cpp")
target_compile_features(genycaster_profile_row_order_fixture PRIVATE cxx_std_23)
target_link_libraries(genycaster_profile_row_order_fixture PRIVATE Qt6::Core Qt6::Qml)
add_test(NAME profile_row_order COMMAND genycaster_profile_row_order_fixture)
set_tests_properties(profile_row_order PROPERTIES TIMEOUT 45)

add_executable(genycaster_qml_shutdown_fixture "${root}/tests/ui/qml_engine_shutdown_tests.cpp")
target_compile_features(genycaster_qml_shutdown_fixture PRIVATE cxx_std_23)
target_link_libraries(genycaster_qml_shutdown_fixture PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
add_test(NAME qml_engine_shutdown COMMAND genycaster_qml_shutdown_fixture)
set_tests_properties(qml_engine_shutdown PROPERTIES TIMEOUT 15
  ENVIRONMENT "QT_QPA_PLATFORM=minimal;QT_FATAL_WARNINGS=1")

find_package(Python3 QUIET COMPONENTS Interpreter)
find_program(QML_TEST_RUNNER NAMES qmltestrunner HINTS "${Qt6_DIR}/../../../bin" NO_DEFAULT_PATH)
if(Python3_Interpreter_FOUND AND QML_TEST_RUNNER)
  add_test(NAME feed_projection_qml_runtime
    COMMAND "${Python3_EXECUTABLE}" "${root}/tests/ui/feed_projection_work_runtime.py")
  set_tests_properties(feed_projection_qml_runtime PROPERTIES TIMEOUT 45
    ENVIRONMENT "QML_TEST_RUNNER=${QML_TEST_RUNNER}")
endif()

find_program(NODE_EXECUTABLE NAMES node nodejs)
if(NODE_EXECUTABLE)
  set(projection_contracts
    feed_projection_work_budget_20261128
    feed_publication_lifecycle_20261128
    cast_media_projection_stability_20261128
    semantic_cast_renderer_identity_20261128
    profile_header_first_20261128
    profile_lazy_header_20261128
    video_transport_lifetime_20261128
    derived_interest_scan_coalescing_20261128
    magazine_skeleton_lifetime_20261128
    notification_native_projection_20261128
    feed_profile_runtime_layout_regressions)
  foreach(contract IN LISTS projection_contracts)
    add_test(NAME "contract.${contract}"
      COMMAND "${NODE_EXECUTABLE}" --test "${root}/tests/wasm/${contract}.test.mjs")
    set_tests_properties("contract.${contract}" PROPERTIES TIMEOUT 30 LABELS "source-contract")
  endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_program(XVFB_RUN xvfb-run)
  if(XVFB_RUN)
    add_test(NAME pacing_opengl COMMAND "${XVFB_RUN}" -a $<TARGET_FILE:genycaster_rendering_pacing_fixture>)
    set_tests_properties(pacing_opengl PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=xcb;QSG_RHI_BACKEND=opengl;QT_QUICK_BACKEND=rhi;LIBGL_ALWAYS_SOFTWARE=1" TIMEOUT 20)
    find_program(QT_QSB_EXECUTABLE NAMES qsb HINTS "${Qt6_DIR}/../../../bin" NO_DEFAULT_PATH)
    if(Python3_Interpreter_FOUND AND QML_TEST_RUNNER AND QT_QSB_EXECUTABLE)
      foreach(fixture IN ITEMS gaussian_shader_equivalence_runtime reflection_capture_runtime drawer_backdrop_runtime)
        add_test(NAME "${fixture}" COMMAND "${XVFB_RUN}" -a
          "${Python3_EXECUTABLE}" "${root}/tests/ui/${fixture}.py")
        set_tests_properties("${fixture}" PROPERTIES
          ENVIRONMENT "QT_QPA_PLATFORM=xcb;QSG_RHI_BACKEND=opengl;QT_QUICK_BACKEND=rhi;LIBGL_ALWAYS_SOFTWARE=1;QML_TEST_RUNNER=${QML_TEST_RUNNER};QT_QSB_EXECUTABLE=${QT_QSB_EXECUTABLE}"
          TIMEOUT 90)
      endforeach()
    endif()
  endif()
endif()

# Optional production bridge integration, without booting the app or fetching
# dependencies. C++ modules require a compiler/generator with scan support.
option(GENYCASTER_FIXTURE_PRESENTATION_BRIDGE "Compile the actual PresentationBridge for local snapshot regression tests" OFF)
if(GENYCASTER_FIXTURE_PRESENTATION_BRIDGE)
  if(CMAKE_VERSION VERSION_LESS 3.28)
    message(FATAL_ERROR "PresentationBridge fixture requires CMake 3.28+ and C++ module scanning")
  endif()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(fixture_generated "${CMAKE_CURRENT_BINARY_DIR}/bridge-generated")
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${root}/ci/generate_deployment_config.py"
    --out-dir "${fixture_generated}"
    --set GENYCASTER_PUBLIC_ORIGIN=https://app.example.invalid
    --set GENYCASTER_SIWF_ORIGIN=https://siwf.example.invalid
    --set GENYCASTER_OPENPROOF_ISSUER=https://auth.example.invalid
    --set GENYCASTER_OPENPROOF_BROWSER_BASE=https://auth.example.invalid/openproof
    --set GENYCASTER_BROWSER_GATEWAY_ORIGIN=https://browser.example.invalid
    --set GENYCASTER_MEDIA_PUBLIC_ORIGIN=https://media.example.invalid
    --set GENYCASTER_SNAPCHAIN_ORIGIN=https://snap.example.invalid
    --set GENYCASTER_HYPERSNAP_ORIGIN=https://hyper.example.invalid
    --set GENYCASTER_COMPANY_ORIGIN=https://company.example.invalid
    COMMAND_ERROR_IS_FATAL ANY)
  foreach(feature IN ITEMS SOCIAL FEED CHANNELS MINIAPPS CHAT SEARCH NOTIFICATIONS WALLET)
    set(FARCASTER_ENABLE_${feature}_CPP true)
  endforeach()
  set(FARCASTER_DEPLOYMENT_PROFILE_CPP Standard)
  set(FARCASTER_DEFAULT_SIWF_DOMAIN siwf.example.invalid)
  set(FARCASTER_DEFAULT_SIWF_URI https://siwf.example.invalid)
  set(FARCASTER_DEFAULT_SNAPCHAIN_BASE_URL https://snap.example.invalid)
  set(FARCASTER_DEFAULT_MEDIA_GATEWAY_BASE_URL https://media.example.invalid)
  set(FARCASTER_DEFAULT_APP_FID 1)
  set(FARCASTER_DEFAULT_APP_NAME GenycasterFixture)
  configure_file("${root}/src/application/configured_features.cpp.in"
    "${fixture_generated}/configured_features.cpp" @ONLY)
  add_library(genycaster_fixture_configuration STATIC)
  set_target_properties(genycaster_fixture_configuration PROPERTIES AUTOMOC OFF CXX_SCAN_FOR_MODULES ON)
  target_sources(genycaster_fixture_configuration
    PUBLIC FILE_SET CXX_MODULES FILES "${root}/src/application/configuration.cppm"
    PRIVATE "${fixture_generated}/configured_features.cpp")
  target_compile_features(genycaster_fixture_configuration PUBLIC cxx_std_23)
  add_library(genycaster_fixture_bridge STATIC
    "${root}/src/presentation/qt/presentation_bridge.cpp"
    "${root}/src/presentation/qt/presentation_bridge.hpp")
  set_target_properties(genycaster_fixture_bridge PROPERTIES CXX_SCAN_FOR_MODULES ON)
  target_compile_features(genycaster_fixture_bridge PUBLIC cxx_std_23)
  target_include_directories(genycaster_fixture_bridge PUBLIC "${root}/src/presentation/qt"
    PRIVATE "${fixture_generated}")
  target_link_libraries(genycaster_fixture_bridge PUBLIC genycaster_fixture_configuration
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
  foreach(name IN ITEMS profile_bridge_snapshot presentation_row_notifications)
    if(name STREQUAL "profile_bridge_snapshot")
      set(test_source "${root}/tests/ui/profile_bridge_snapshot_tests.cpp")
    else()
      set(test_source "${root}/tests/unit/presentation_row_notifications_tests.cpp")
    endif()
    add_executable(genycaster_${name}_fixture "${test_source}")
    target_compile_features(genycaster_${name}_fixture PRIVATE cxx_std_23)
    target_link_libraries(genycaster_${name}_fixture PRIVATE genycaster_fixture_bridge)
    # Existing assertions must remain active in the cached Release fixture.
    if(MSVC)
      target_compile_options(genycaster_${name}_fixture PRIVATE /UNDEBUG)
    else()
      target_compile_options(genycaster_${name}_fixture PRIVATE -UNDEBUG)
    endif()
    add_test(NAME "${name}" COMMAND genycaster_${name}_fixture)
    set_tests_properties("${name}" PROPERTIES TIMEOUT 30)
  endforeach()
endif()
