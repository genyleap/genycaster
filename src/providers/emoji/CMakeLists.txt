# Concrete emoji asset providers and infrastructure (Noto mapping, bounded
# validation, disk cache). Pure C++26 modules; network/UI live in the Qt
# presentation boundary.
# Built-in provider adapters are statically composed into the client.
# On Windows, C++26 module exports in DLL targets are not portable ABI exports.
# Use an explicit archive regardless of BUILD_SHARED_LIBS/vcpkg triplet.
add_library(farcaster_provider_emoji STATIC)
add_library(farcaster::provider_emoji ALIAS farcaster_provider_emoji)

target_sources(farcaster_provider_emoji
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            noto_animated_emoji.cppm
            emoji_asset_validator.cppm
            emoji_cache.cppm
    PRIVATE
        noto_animated_emoji.cpp
        emoji_asset_validator.cpp
        emoji_cache.cpp
)

target_link_libraries(farcaster_provider_emoji
    PUBLIC
        farcaster::application_emoji
        farcaster::capabilities
        farcaster::domain
        farcaster::foundation
)
target_compile_features(farcaster_provider_emoji PUBLIC cxx_std_26)
set_property(TARGET farcaster_provider_emoji PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_provider_emoji)