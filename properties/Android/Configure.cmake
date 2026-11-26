# Qt's Android packager consumes the project resources and this kit's
# own Gradle template. Add the pinned AndroidX WebKit dependency only.
set(_genycasterAndroidPackage "${CMAKE_CURRENT_BINARY_DIR}/android-package")
get_filename_component(_genyAndroidSdk "${Qt6Core_DIR}/../../.." ABSOLUTE)
set(_genyGradle "${_genyAndroidSdk}/src/android/templates/build.gradle")
if(NOT EXISTS "${_genyGradle}")
    message(FATAL_ERROR "Qt Android Gradle template missing: ${_genyGradle}")
endif()
file(MAKE_DIRECTORY "${_genycasterAndroidPackage}")
file(COPY "${CMAKE_SOURCE_DIR}/properties/Android/" DESTINATION "${_genycasterAndroidPackage}")
file(GLOB_RECURSE _genyAndroidSources CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/properties/Android/src/*.java")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_genyAndroidSources}
    "${CMAKE_SOURCE_DIR}/properties/Android/AndroidManifest.xml"
    "${CMAKE_SOURCE_DIR}/properties/Android/res/xml/miniapp_network_security.xml")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_genyGradle}")
file(READ "${_genyGradle}" _genyGradleContents)
string(APPEND _genyGradleContents "\n dependencies { implementation 'androidx.webkit:webkit:1.12.1' }\n")
file(WRITE "${_genycasterAndroidPackage}/build.gradle" "${_genyGradleContents}")

set_target_properties(genycaster PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR "${_genycasterAndroidPackage}"
    QT_ANDROID_APP_NAME "Genycaster")
