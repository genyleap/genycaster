# Application emoji subsystem: normalization, catalog, motion policy and the
# asset repository facade. Pure C++26 modules; no Qt, no provider imports.
add_library(farcaster_application_emoji)
add_library(farcaster::application_emoji ALIAS farcaster_application_emoji)

target_sources(farcaster_application_emoji
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            unicode_normalizer.cppm
            emoji_catalog.cppm
            animation_policy.cppm
            asset_repository.cppm
    PRIVATE
        unicode_normalizer.cpp
        emoji_catalog.cpp
        emoji_catalog_data.cpp
        animation_policy.cpp
        asset_repository.cpp
)

target_link_libraries(farcaster_application_emoji
    PUBLIC
        farcaster::capabilities
        farcaster::domain
        farcaster::foundation
)
target_compile_features(farcaster_application_emoji PUBLIC cxx_std_26)
set_property(TARGET farcaster_application_emoji PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_application_emoji)

# Regeneration is explicit developer tooling; normal builds use the committed
# generated data table so configure/build never depends on Python.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(farcaster_emoji_catalog_regenerate
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tools/generate_emoji_catalog.py"
            "${CMAKE_SOURCE_DIR}/ui/assets/emoji/noto_manifest.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/emoji_catalog_data.cpp"
        COMMENT "Regenerating emoji_catalog_data.cpp from the vendored Noto manifest"
    )
endif()