add_subdirectory(unit)
add_subdirectory(protocol)
add_subdirectory(providers)

if(FARCASTER_BUILD_UI)
    add_subdirectory(ui)
endif()
