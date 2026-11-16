# Rebuild the committed, cross-platform ShaderEffect resource with Qt's own
# shader compiler. Use the host Qt 6.12 installation; shipping targets only
# consume the resulting QSB and do not need ShaderTools at runtime.
cmake_minimum_required(VERSION 3.24)
set(shader "${CMAKE_CURRENT_LIST_DIR}/../ui/shaders/neon.frag")
set(bundle "${shader}.qsb")
if(DEFINED QT_QSB_EXECUTABLE AND EXISTS "${QT_QSB_EXECUTABLE}")
    set(qsb "${QT_QSB_EXECUTABLE}")
else()
    find_program(qsb NAMES qsb
        HINTS "/opt/Qt/current/gcc_64/bin" "$ENV{QT_HOST_PATH}/bin"
              "$ENV{HOME}/Qt/6.12.0/gcc_64/bin")
endif()
if(NOT qsb)
    message(FATAL_ERROR "Qt 6.12 qsb not found; set -DQT_QSB_EXECUTABLE=/path/to/qsb")
endif()
execute_process(
    COMMAND "${qsb}" --glsl 100es,120,150 --hlsl 50 --msl 12
            -o "${bundle}" "${shader}"
    RESULT_VARIABLE code
    OUTPUT_VARIABLE details
    ERROR_VARIABLE errors)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "Unable to rebuild Neon QSB (exit ${code}): ${errors} ${details}")
endif()
execute_process(COMMAND "${qsb}" --dump "${bundle}"
    RESULT_VARIABLE dump_code OUTPUT_VARIABLE reflection ERROR_VARIABLE dump_error)
if(NOT dump_code EQUAL 0 OR NOT reflection MATCHES "cornerFlags")
    message(FATAL_ERROR "Neon QSB reflection missing cornerFlags uniform: ${dump_error}")
endif()
message(STATUS "Rebuilt and inspected portable Neon shader: ${bundle}")
