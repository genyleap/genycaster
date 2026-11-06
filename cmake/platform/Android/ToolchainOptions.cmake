# Platform-scoped GenyCaster build behavior; not used on other targets.
if(ANDROID)
    # Qt for Android compiles its consumers with -pthread. Clang named-module
    # BMIs/PCMs must use the same POSIX threading mode as their importers;
    # otherwise imports of Qt-free modules (for example storage_policy) fail
    # with a PCH configuration mismatch. Apply the mode before any targets
    # are created so all C++ module producers and consumers agree.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-pthread>)

    # NDK r27 retains a 4 KiB ELF page-alignment default for some shared
    # module targets. Android 15+ devices can run with 16 KiB kernel pages:
    # every first-party .so must be loadable there, not only the Qt libraries.
    # The flags are additive on existing 4 KiB devices and apply before the
    # provider and QtKeychain targets are created.
    add_link_options(
        "-Wl,-z,max-page-size=16384"
        "-Wl,-z,common-page-size=16384")
endif()
