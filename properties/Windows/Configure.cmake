# --- Windows application identity and embedded resources ---------------------
set(GENYCASTER_WINDOWS_ICON_PATH "${CMAKE_SOURCE_DIR}/properties/Windows/genycaster.ico")
configure_file("${CMAKE_SOURCE_DIR}/properties/Windows/Genycaster.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/genycaster-app.rc" @ONLY)
target_sources(genycaster PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/genycaster-app.rc")
set_target_properties(genycaster PROPERTIES WIN32_EXECUTABLE TRUE)
unset(GENYCASTER_WINDOWS_ICON_PATH)

# --- CPack / NSIS application installer -------------------------------------
# This is an installable deployment, not a ZIP containing a naked .exe.
# The CI staging directory receives windeployqt, QML plugins, keychain and all
# vcpkg runtime DLLs. CPack installs exactly that verified payload.
set(GENYCASTER_WINDOWS_PACKAGE_ARCH "" CACHE STRING "Windows release target: x86_64 or arm64")
if(GENYCASTER_WINDOWS_PACKAGE_ARCH)
    if(NOT GENYCASTER_WINDOWS_PACKAGE_ARCH MATCHES "^(x86_64|arm64)$")
        message(FATAL_ERROR "Unsupported GenyCaster Windows installer architecture")
    endif()
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_NAME "GenyCaster")
    set(CPACK_PACKAGE_VENDOR "Genyleap Labs")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://genycaster.xyz")
    set(CPACK_PACKAGE_CONTACT "info@genyleap.com")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "GenyCaster — native, local-first decentralized social client")
    set(CPACK_PACKAGE_FILE_NAME
        "GenyCaster-v${GENYCASTER_RELEASE_VERSION}-windows-${GENYCASTER_WINDOWS_PACKAGE_ARCH}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "GenyCaster")
    set(CPACK_NSIS_DISPLAY_NAME "GenyCaster")
    set(CPACK_NSIS_PACKAGE_NAME "GenyCaster")
    # The staged application executable is at $INSTDIR, not $INSTDIR/bin.
    # CPack otherwise builds an invalid Start Menu shortcut by default.
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "Genycaster.exe")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://genycaster.xyz")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/properties/Windows/genycaster.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/properties/Windows/genycaster.ico")
    set(CPACK_NSIS_MENU_LINKS "Genycaster.exe" "GenyCaster")
    # Separate the publisher/brand introduction (Welcome) from the actual
    # GPLv3 acceptance page. Never prepend a mixed-brand preface to the
    # legally operative full license text in a single scrolling control.
    set(CPACK_NSIS_WELCOME_TITLE "Welcome to GenyCaster - Genyleap Labs")
    set(CPACK_NSIS_BRANDING_TEXT "Genyleap Labs | https://genycaster.xyz")
    # These NSIS commands are loaded after CPackConfig.cmake is parsed.
    # Bracket-quoted commands preserve backslashes and shortcut quoting.
    if(GENYCASTER_WINDOWS_PACKAGE_ARCH STREQUAL "arm64")
        set(GENYCASTER_VC_REDIST_SUFFIX "arm64")
    else()
        set(GENYCASTER_VC_REDIST_SUFFIX "x64")
    endif()
    configure_file(
        "${CMAKE_SOURCE_DIR}/properties/Windows/NSISOptions.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/GenyCasterNSISOptions.cmake"
        @ONLY)
    set(CPACK_PROJECT_CONFIG_FILE
        "${CMAKE_CURRENT_BINARY_DIR}/GenyCasterNSISOptions.cmake")
    unset(GENYCASTER_VC_REDIST_SUFFIX)

    # GPLv3 has its own complete, readable acceptance step. The separate
    # Welcome page above introduces the publisher and official website.
    # Other notices remain installed under THIRD_PARTY_NOTICES.md.
    file(READ "${CMAKE_SOURCE_DIR}/LICENSE" _genycasterGplText)
    set(_genycasterLicensePage "${CMAKE_CURRENT_BINARY_DIR}/GenyCaster-Installer-License.txt")
    file(WRITE "${_genycasterLicensePage}" "${_genycasterGplText}")
    set(CPACK_RESOURCE_FILE_LICENSE "${_genycasterLicensePage}")
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION "." RENAME "LICENSE.txt" COMPONENT Runtime)
    install(FILES "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        DESTINATION "." COMPONENT Runtime)
    # Include actual redistribution notices, not only hyperlinks to source
    # paths that do not exist in an installed Windows application.
    install(FILES
        "${CMAKE_SOURCE_DIR}/ui/assets/emoji/LICENSE.NotoAnimatedEmoji.txt"
        "${CMAKE_SOURCE_DIR}/ui/assets/fonts/OFL-NotoEmoji.txt"
        "${CMAKE_SOURCE_DIR}/ui/assets/fonts/OFL-Vazirmatn.txt"
        "${CMAKE_SOURCE_DIR}/ui/assets/icons/LICENSE.Font-Awesome.txt"
        "${CMAKE_SOURCE_DIR}/ui/assets/icons/genyleap/SOLAR_ICON_SET.md"
        "${CMAKE_SOURCE_DIR}/ui/assets/icons/genyleap/TRADEMARKS.md"
        DESTINATION "licenses" COMPONENT Runtime)
    unset(_genycasterGplText)
    unset(_genycasterLicensePage)
    set(CPACK_MONOLITHIC_INSTALL ON)
    # Package only GenyCaster's explicit Runtime component. QtKeychain and
    # other dependency subprojects install SDK headers, import libraries and
    # CMake metadata in the default Unspecified component; those are not part
    # of the deployable app and must never enter the NSIS payload.
    set(CPACK_INSTALL_CMAKE_PROJECTS
        "${CMAKE_BINARY_DIR};${PROJECT_NAME};Runtime;/")

    # All dependencies must already have been deployed to this directory.
    # Directory contents are read by cpack at install time, after CI staging.
    set(_genycasterWindowsStage "${CMAKE_SOURCE_DIR}/dist/app")
    install(DIRECTORY "${_genycasterWindowsStage}/" DESTINATION "."
        COMPONENT Runtime)
    unset(_genycasterWindowsStage)
    include(CPack)
endif()
