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
    set(CPACK_PACKAGE_VENDOR "Genyleap")
    set(CPACK_PACKAGE_CONTACT "info@genyleap.com")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "GenyCaster — native, local-first decentralized social client")
    set(CPACK_PACKAGE_FILE_NAME
        "GenyCaster-v${GENYCASTER_RELEASE_VERSION}-windows-${GENYCASTER_WINDOWS_PACKAGE_ARCH}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "GenyCaster")
    set(CPACK_NSIS_DISPLAY_NAME "GenyCaster")
    set(CPACK_NSIS_PACKAGE_NAME "GenyCaster")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/properties/Windows/genycaster.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/properties/Windows/genycaster.ico")
    set(CPACK_NSIS_MENU_LINKS "Genycaster.exe" "GenyCaster")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
    set(CPACK_MONOLITHIC_INSTALL ON)

    # All dependencies must already have been deployed to this directory.
    # Directory contents are read by cpack at install time, after CI staging.
    set(_genycasterWindowsStage "${CMAKE_SOURCE_DIR}/dist/app")
    install(DIRECTORY "${_genycasterWindowsStage}/" DESTINATION "."
        COMPONENT Runtime)
    unset(_genycasterWindowsStage)
    include(CPack)
endif()
