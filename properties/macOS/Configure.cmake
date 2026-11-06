# macdeployqt requires an actual .app with Qt-generated Info.plist.
set_target_properties(genycaster PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Genycaster"
    MACOSX_BUNDLE_ICON_FILE "genycaster.icns")

set(_genycasterMacIcon "${CMAKE_SOURCE_DIR}/properties/macOS/genycaster.icns")
target_sources(genycaster PRIVATE "${_genycasterMacIcon}")
set_source_files_properties("${_genycasterMacIcon}"
    PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
unset(_genycasterMacIcon)

# Case-insensitive APFS may retain a pre-rename genycaster.app directory.
# Normalize physical casing only after the app has been linked.
add_custom_command(TARGET genycaster POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
        "-DGENYCASTER_BUNDLE_PATH=$<TARGET_BUNDLE_DIR:genycaster>"
        -P "${CMAKE_SOURCE_DIR}/cmake/FixMacOSBundleCase.cmake"
    VERBATIM)
