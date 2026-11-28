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
    "Stable code-signing certificate name or SHA-1 fingerprint for local macOS builds")
option(GENYCASTER_MACOS_AUTO_LOCAL_SIGNING
    "Select the sole valid Apple Development identity for local builds" ON)
# An ad-hoc designated requirement is tied to the executable hash. After a
# rebuild the login Keychain no longer recognizes the previous Always Allow
# decision. Choose a stable *existing* development certificate when unambiguous;
# never create credentials, change ACLs, or alter the release CI signing policy.
if(NOT GENYCASTER_MACOS_LOCAL_SIGNING_IDENTITY AND NOT DEFINED ENV{CI})
    if(GENYCASTER_MACOS_AUTO_LOCAL_SIGNING)
        execute_process(COMMAND /usr/bin/security find-identity -v -p codesigning
            RESULT_VARIABLE _genycasterIdentityResult
            OUTPUT_VARIABLE _genycasterIdentities ERROR_QUIET)
        if(_genycasterIdentityResult EQUAL 0)
            string(REGEX MATCHALL "[0-9A-Fa-f]+ \"Apple Development:[^\n\"]+\""
                _genycasterDevelopmentMatches "${_genycasterIdentities}")
            list(LENGTH _genycasterDevelopmentMatches _genycasterDevelopmentCount)
            if(_genycasterDevelopmentCount EQUAL 1)
                list(GET _genycasterDevelopmentMatches 0 _genycasterDevelopmentMatch)
                string(REGEX MATCH "^[0-9A-Fa-f]+" _genycasterDevelopmentFingerprint
                    "${_genycasterDevelopmentMatch}")
                string(LENGTH "${_genycasterDevelopmentFingerprint}" _genycasterFingerprintLength)
                if(_genycasterFingerprintLength EQUAL 40)
                    set(GENYCASTER_MACOS_LOCAL_SIGNING_IDENTITY
                        "${_genycasterDevelopmentFingerprint}" CACHE STRING
                        "Stable code-signing certificate name or SHA-1 fingerprint for local macOS builds" FORCE)
                    message(STATUS "Genycaster: selected and pinned the local Apple Development signing identity")
                endif()
            endif()
        endif()
    endif()
    if(NOT GENYCASTER_MACOS_LOCAL_SIGNING_IDENTITY)
        message(WARNING "Genycaster: no stable local signing identity selected. "
            "Ad-hoc rebuilds can invalidate Keychain Always Allow. "
            "Set GENYCASTER_MACOS_LOCAL_SIGNING_IDENTITY to an existing certificate fingerprint; "
            "see docs/MACOS_RELEASE_SIGNING.md.")
    endif()
endif()
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
