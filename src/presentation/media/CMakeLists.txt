# Canonical embed-media classification. Pure C++26 module, no Qt and no
# provider imports: the same rules drive feed row building, EmbedPreview
# dispatch and rich-composer media blocks.
# This is a pure C++26 module consumed by the native client, not a DLL API.
# With Qt's BUILD_SHARED_LIBS on Windows, an implicit SHARED target exports
# module interfaces but not implementation-unit symbols. Link the complete
# module and implementation as one static archive on every platform.
add_library(farcaster_presentation_media STATIC)
add_library(farcaster::presentation_media ALIAS farcaster_presentation_media)

target_sources(farcaster_presentation_media
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            classification.cppm
            storage_policy.cppm
    PRIVATE
        classification.cpp
        storage_policy.cpp
)

target_compile_features(farcaster_presentation_media PUBLIC cxx_std_26)
set_property(TARGET farcaster_presentation_media PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_presentation_media)
