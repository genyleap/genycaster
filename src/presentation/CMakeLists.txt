add_library(farcaster_presentation STATIC
    qt/appearance_controller.cpp
    qt/appearance_controller.hpp
    qt/presentation_bridge.cpp
    qt/rendering_controller.cpp
    qt/rendering_controller.hpp
    qt/presentation_bridge.hpp
    qt/localization_controller.cpp
    qt/localization_controller.hpp
    qt/media_upload_controller.cpp
    qt/media_upload_controller.hpp
    qt/rich_composer_controller.cpp
    qt/rich_composer_controller.hpp
    qt/emoji_controller.cpp
    qt/emoji_controller.hpp
    qt/rich_inline_text_item.cpp
    qt/rich_inline_text_item.hpp
)
add_library(farcaster::presentation ALIAS farcaster_presentation)

target_include_directories(farcaster_presentation
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/qt"
)

target_link_libraries(farcaster_presentation
    PUBLIC
        farcaster::application
        farcaster::application_rich
        farcaster::application_emoji
        farcaster::execution
        farcaster::transport_qt_network
        Qt6::Core
        Qt6::Qml
    PRIVATE
        Qt6::Gui
        Qt6::Quick
)

target_compile_features(farcaster_presentation PUBLIC cxx_std_26)
set_property(TARGET farcaster_presentation PROPERTY CXX_SCAN_FOR_MODULES ON)
set_target_properties(farcaster_presentation PROPERTIES AUTOMOC ON)
# VectorImage track-matte composition needs Qt 6.11+ and the Qt Lottie vector-format plugin.
# Older/renderer-free SDKs retain the Unicode fallback without loading this type.
find_package(Qt6 QUIET COMPONENTS Lottie)
if(Qt6Lottie_FOUND AND Qt6_VERSION VERSION_GREATER_EQUAL 6.11)
    target_compile_definitions(farcaster_presentation PRIVATE FARCASTER_HAS_LOTTIE=1)
endif()

farcaster_enable_warnings(farcaster_presentation)
