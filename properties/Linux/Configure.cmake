# The same desktop/icon files are consumed by CMake installs and AppImage CI.
include(GNUInstallDirs)
install(FILES "${CMAKE_SOURCE_DIR}/properties/Linux/genycaster.png"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps")
install(FILES "${CMAKE_SOURCE_DIR}/properties/Linux/genycaster.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
