# Regenerate the portable volumetric brand artwork used by native Qt and WASM.
# No runtime ShaderTools dependency: all targets use the checked-in QSB.
cmake_minimum_required(VERSION 3.24)
set(shader "${CMAKE_CURRENT_LIST_DIR}/../ui/shaders/setup_glass_g.frag")
set(bundle "${shader}.qsb")
if(DEFINED QT_QSB_EXECUTABLE AND EXISTS "${QT_QSB_EXECUTABLE}")
    set(qsb "${QT_QSB_EXECUTABLE}")
else()
    find_program(qsb NAMES qsb
        HINTS "/opt/Qt/6.12.0/gcc_64/bin" "/opt/Qt/current/gcc_64/bin"
              "$ENV{QT_HOST_PATH}/bin" "$ENV{HOME}/Qt/6.12.0/gcc_64/bin")
endif()
if(NOT qsb)
    message(FATAL_ERROR "Qt qsb missing. Set -DQT_QSB_EXECUTABLE=/path/to/qsb")
endif()
execute_process(
    COMMAND "${qsb}" --glsl 100es,120,150 --hlsl 50 --msl 12
            -o "${bundle}" "${shader}"
    RESULT_VARIABLE status OUTPUT_VARIABLE details ERROR_VARIABLE errors)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Setup studio shader failed: ${errors} ${details}")
endif()
execute_process(COMMAND "${qsb}" --dump "${bundle}"
    RESULT_VARIABLE inspection OUTPUT_VARIABLE reflection ERROR_VARIABLE reflectionError)
if(NOT inspection EQUAL 0 OR NOT reflection MATCHES "logoDistanceMap"
        OR NOT reflection MATCHES "logoNormalMap")
    message(FATAL_ERROR "Setup studio QSB has invalid reflection: ${reflectionError}")
endif()
message(STATUS "Rebuilt portable setup studio QSB: ${bundle}")
