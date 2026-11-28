add_subdirectory(unit)
add_subdirectory(protocol)
add_subdirectory(providers)

add_test(NAME macos_local_signing_identity
    COMMAND "${CMAKE_COMMAND}" -P
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos_local_signing_test.cmake")

if(FARCASTER_BUILD_UI)
    add_subdirectory(ui)
endif()
