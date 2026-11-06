# Platform-scoped GenyCaster build behavior; not used on other targets.
if(ANDROID)
    # The bundled SQLite amalgamation is C and must use the Android NDK toolchain.
    # Native desktop builds continue to use their existing SQLite integration.
    enable_language(C)
endif()
