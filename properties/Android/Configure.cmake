# Qt's Android packager consumes this manifest and resource directory.
set_target_properties(genycaster PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/properties/Android")
