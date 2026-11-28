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
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_program(XVFB_RUN xvfb-run)
  if(XVFB_RUN)
    add_test(NAME pacing_opengl COMMAND "${XVFB_RUN}" -a $<TARGET_FILE:genycaster_rendering_pacing_fixture>)
    set_tests_properties(pacing_opengl PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=xcb;QSG_RHI_BACKEND=opengl;QT_QUICK_BACKEND=rhi;LIBGL_ALWAYS_SOFTWARE=1" TIMEOUT 20)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    find_program(QML_TEST_RUNNER NAMES qmltestrunner HINTS "${Qt6_DIR}/../../../bin" NO_DEFAULT_PATH)
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
