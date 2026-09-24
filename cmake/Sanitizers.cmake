# Optional developer diagnostics for memory, undefined-behavior and data-race bugs.
# Keep this opt-in so release binaries and unsupported toolchains are unaffected.
set(GENYCASTER_SANITIZER "none" CACHE STRING
    "Runtime sanitizer for developer/test builds: none, address, undefined, thread")
set_property(CACHE GENYCASTER_SANITIZER PROPERTY STRINGS none address undefined thread)

set(_genycasterKnownSanitizers none address undefined thread)
if(NOT GENYCASTER_SANITIZER IN_LIST _genycasterKnownSanitizers)
    message(FATAL_ERROR
        "Unsupported GENYCASTER_SANITIZER='${GENYCASTER_SANITIZER}'. "
        "Expected one of: ${_genycasterKnownSanitizers}")
endif()

if(NOT GENYCASTER_SANITIZER STREQUAL "none")
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|AppleClang|GNU)$")
        message(FATAL_ERROR
            "GENYCASTER_SANITIZER=${GENYCASTER_SANITIZER} currently requires Clang, AppleClang, or GNU.")
    endif()

    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:-fsanitize=${GENYCASTER_SANITIZER}>"
        "$<$<COMPILE_LANGUAGE:CXX>:-fno-omit-frame-pointer>"
    )
    add_link_options("-fsanitize=${GENYCASTER_SANITIZER}")
    message(STATUS "Genycaster sanitizer enabled: ${GENYCASTER_SANITIZER}")
endif()

unset(_genycasterKnownSanitizers)
