add_library(farcaster_platform_signer)
add_library(farcaster::platform_signer ALIAS farcaster_platform_signer)

target_sources(farcaster_platform_signer
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            signer_vault.cppm
)

if(APPLE)
    find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
    find_library(FARCASTER_SECURITY_FRAMEWORK Security REQUIRED)
    target_sources(farcaster_platform_signer
        PRIVATE
            signer_keychain_boundary.cpp
            signer_vault_macos.cpp
    )
    # Apple's Security/CoreFoundation headers still use CF_ENUM forward
    # declarations that Clang 22 rejects under C++26 + -Wpedantic. Keep the
    # compatibility exception confined to the textual C/ABI boundary.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set_property(SOURCE
            signer_keychain_boundary.cpp
            APPEND PROPERTY COMPILE_OPTIONS
                "-Wno-elaborated-enum-base"
                "-Wno-invalid-utf8"
                "-Wno-nullability-extension"
                "-Wno-variadic-macros"
        )
    endif()
    target_link_libraries(farcaster_platform_signer
        PUBLIC farcaster::capabilities farcaster::domain farcaster::foundation
        PRIVATE OpenSSL::Crypto "${FARCASTER_SECURITY_FRAMEWORK}"
    )
else()
    target_sources(farcaster_platform_signer PRIVATE signer_vault_unavailable.cpp)
    target_link_libraries(farcaster_platform_signer
        PUBLIC farcaster::capabilities farcaster::domain farcaster::foundation
    )
endif()

target_compile_features(farcaster_platform_signer PUBLIC cxx_std_26)
set_property(TARGET farcaster_platform_signer PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_platform_signer)
