# Configure version information and icon from the package-owned RC template.
set(GENYCASTER_WINDOWS_ICON_PATH "${CMAKE_SOURCE_DIR}/properties/Windows/genycaster.ico")
configure_file("${CMAKE_SOURCE_DIR}/properties/Windows/Genycaster.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/genycaster-app.rc" @ONLY)
target_sources(genycaster PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/genycaster-app.rc")
set_target_properties(genycaster PROPERTIES WIN32_EXECUTABLE TRUE)
unset(GENYCASTER_WINDOWS_ICON_PATH)
