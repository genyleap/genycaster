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
    # CPack's menu links are Start Menu entries, NOT Desktop shortcuts.
    # Create the desktop link on installation and always remove it on uninstall.
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut \"$DESKTOP\\GenyCaster.lnk\" \"$INSTDIR\\Genycaster.exe\"")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete \"$DESKTOP\\GenyCaster.lnk\"")

    # Qt/MSVC needs the Microsoft Visual C++ runtime. Qt's windeployqt
    # stages the official redistributable but does not install it.
    if(GENYCASTER_WINDOWS_PACKAGE_ARCH STREQUAL "arm64")
        set(_genycasterVCRedistSuffix "arm64")
    else()
        set(_genycasterVCRedistSuffix "x64")
    endif()
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "IfFileExists \"$INSTDIR\\vc_redist.${_genycasterVCRedistSuffix}.exe\" +2 0\n"
        "Abort \"Microsoft Visual C++ runtime installer is missing\"\n"
        "DetailPrint \"Installing Microsoft Visual C++ runtime\"\n"
        "ExecWait '\"$INSTDIR\\vc_redist.${_genycasterVCRedistSuffix}.exe\" /install /quiet /norestart' $0\n"
        "StrCmp $0 0 genycaster_vc_runtime_done\n"
        "StrCmp $0 1638 genycaster_vc_runtime_done\n"
        "StrCmp $0 3010 genycaster_vc_runtime_reboot\n"
        "MessageBox MB_ICONSTOP|MB_OK \"Microsoft Visual C++ runtime installation failed (code $0).\"\n"
        "Abort\n"
        "genycaster_vc_runtime_reboot:\n"
        "SetRebootFlag true\n"
        "genycaster_vc_runtime_done:\n")
    unset(_genycasterVCRedistSuffix)

    # A visible publisher/copyright preface complements (never replaces)
    # the complete GPLv3 text. Brand identity is not relicensed by the GPL.
    file(READ "${CMAKE_SOURCE_DIR}/LICENSE" _genycasterGplText)
    set(_genycasterLicensePage "${CMAKE_CURRENT_BINARY_DIR}/GenyCaster-Installer-License.txt")
    file(WRITE "${_genycasterLicensePage}"
        "GenyCaster - Software License and Publisher Notice\r\n"
        "Copyright (C) 2026 Genyleap Labs.\r\n"
        "Official website: https://genycaster.xyz\r\n"
        "GenyCaster source code is distributed under the GNU General Public License version 3.\r\n"
        "The Genyleap and GenyCaster names, logos and trademarks are not granted for trademark use by this code license.\r\n"
        "Third-party components keep their respective licenses; see THIRD_PARTY_NOTICES.md in the installation.\r\n"
        "\r\n"
        "GNU GENERAL PUBLIC LICENSE\r\n"
        "--------------------------\r\n"
        "${_genycasterGplText}")
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

    # All dependencies must already have been deployed to this directory.
    # Directory contents are read by cpack at install time, after CI staging.
    set(_genycasterWindowsStage "${CMAKE_SOURCE_DIR}/dist/app")
    install(DIRECTORY "${_genycasterWindowsStage}/" DESTINATION "."
        COMPONENT Runtime)
    unset(_genycasterWindowsStage)
    include(CPack)
endif()
