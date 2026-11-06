# Platform-scoped GenyCaster build behavior; not used on other targets.
if(ANDROID)
    # Old Qt Creator build directories can retain FARCASTER_BUILD_TESTS=ON
    # from desktop profiles. Building the APK must not inherit those tests.
    # Dedicated Android test configurations can explicitly opt back in.
    option(GENYCASTER_ANDROID_ENABLE_TESTS
        "Explicitly allow unit/contract test targets in Android cross-builds" OFF)
    if(NOT GENYCASTER_ANDROID_ENABLE_TESTS AND FARCASTER_BUILD_TESTS)
        message(STATUS
            "Genycaster Android: disabling cached FARCASTER_BUILD_TESTS=ON "
            "for the app build; opt in with GENYCASTER_ANDROID_ENABLE_TESTS=ON.")
        set(FARCASTER_BUILD_TESTS OFF CACHE BOOL
            "Build unit, contract, integration and architecture tests" FORCE)
    endif()
endif()
