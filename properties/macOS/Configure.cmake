# macdeployqt requires an actual .app with Qt-generated Info.plist.
set_target_properties(genycaster PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Genycaster"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.genyleap.genycaster"
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
        -P "${CMAKE_SOURCE_DIR}/cmake/platform/macOS/configure.cmake"
    VERBATIM)

# For repeated local Ninja builds, use the same real signing identity.
# CI signs its finalized bundle separately after macdeployqt.
set(GENYCASTER_MACOS_LOCAL_SIGNING_IDENTITY "" CACHE STRING
    "Optional Apple Development or Developer ID Application identity for local macOS builds")
if(GENYCASTER_MACOS_LOCAL_SIGNING_IDENTITY)
    if(GENYCASTER_MACOS_LOCAL_SIGNING_IDENTITY STREQUAL "-")
        message(FATAL_ERROR "Local stable Keychain identity requires a real signing certificate, not ad-hoc")
    endif()
    find_program(_genycasterCodesign codesign REQUIRED)
    add_custom_command(TARGET genycaster POST_BUILD
        COMMAND "${_genycasterCodesign}" --force
            --sign "${GENYCASTER_MACOS_LOCAL_SIGNING_IDENTITY}"
            "$<TARGET_BUNDLE_DIR:genycaster>"
        COMMAND "${_genycasterCodesign}" --verify --strict "$<TARGET_BUNDLE_DIR:genycaster>"
        VERBATIM)
endif()
