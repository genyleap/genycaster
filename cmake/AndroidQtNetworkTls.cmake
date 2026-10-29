# Fail closed for an Android Qt Network SDK compiled without SSL APIs.
# Dynamic OpenSSL libraries bundled in the APK cannot enable QT_FEATURE_ssl
# in a QtNetwork binary where this feature was disabled at build time.
function(genycaster_require_android_qt_tls qt_network_includes)
    set(_config "")
    foreach(_include IN LISTS qt_network_includes)
        if(EXISTS "${_include}/qtnetwork-config.h")
            set(_config "${_include}/qtnetwork-config.h")
            break()
        endif()
        if(EXISTS "${_include}/QtNetwork/qtnetwork-config.h")
            set(_config "${_include}/QtNetwork/qtnetwork-config.h")
            break()
        endif()
    endforeach()

    if(_config STREQUAL "")
        message(FATAL_ERROR
            "Cannot verify the Android Qt Network SSL feature: "
            "qtnetwork-config.h was not found in Qt6Network_INCLUDE_DIRS. "
            "Use a complete Qt 6.12 Android kit with TLS support.")
    endif()

    file(STRINGS "${_config}" _ssl_features
        REGEX "^#define[ \t]+QT_FEATURE_ssl[ \t]+[-0-9]+")
    if(NOT _ssl_features MATCHES
            "^#define[ \t]+QT_FEATURE_ssl[ \t]+1([ \t]|$)")
        message(FATAL_ERROR
            "This Qt Android SDK was built without SSL/TLS support: "
            "${_config} contains '${_ssl_features}'. "
            "GenyCaster requires HTTPS for its Farcaster providers. "
            "Install or rebuild a Qt 6.12 Android kit with QT_FEATURE_ssl=1. "
            "Bundling OpenSSL libssl_3.so and libcrypto_3.so does not "
            "enable disabled QtNetwork SSL APIs.")
    endif()

    message(STATUS "Verified Android Qt Network supports SSL: ${_config}")
endfunction()
