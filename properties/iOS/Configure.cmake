# Qt generates the iOS Info.plist; keep signing/target metadata with the
# platform package, while leaving the shared app and C++ target unchanged.
set_target_properties(genycaster PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Genycaster"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.genyleap.genycaster"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")

set(_genycasterIOSCatalog "${CMAKE_SOURCE_DIR}/properties/iOS/Assets.xcassets")
target_sources(genycaster PRIVATE "${_genycasterIOSCatalog}")
set_source_files_properties("${_genycasterIOSCatalog}"
    PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
unset(_genycasterIOSCatalog)
