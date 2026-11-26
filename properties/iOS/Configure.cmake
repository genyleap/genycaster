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

# The isolated wallet approval WebView stays in-process on mobile.
# Never weaken ATS for arbitrary web content.
get_target_property(_genyInfoTemplate genycaster MACOSX_BUNDLE_INFO_PLIST)
if(NOT _genyInfoTemplate)
    set(_genyInfoTemplate "${CMAKE_ROOT}/Modules/MacOSXBundleInfo.plist.in")
endif()
file(READ "${_genyInfoTemplate}" _genyInfo)
if(_genyInfo MATCHES "<key>NSAppTransportSecurity</key>")
    message(FATAL_ERROR "Review existing iOS ATS policy before adding wallet loopback exception")
endif()
string(REGEX REPLACE "</dict>[ \t\r\n]*</plist>[ \t\r\n]*$"
    "<key>NSAppTransportSecurity</key><dict><key>NSExceptionDomains</key><dict><key>127.0.0.1</key><dict><key>NSExceptionAllowsInsecureHTTPLoads</key><true/></dict></dict></dict></dict></plist>"
    _genyWalletInfo "${_genyInfo}")
if(_genyWalletInfo STREQUAL _genyInfo)
    message(FATAL_ERROR "Could not add scoped wallet ATS exception")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/GenycasterMobileInfo.plist.in" "${_genyWalletInfo}")
set_target_properties(genycaster PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/GenycasterMobileInfo.plist.in")
