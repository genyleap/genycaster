add_library(farcaster_presentation STATIC
    qt/presentation_bridge.cpp
    qt/presentation_bridge.hpp
)
add_library(farcaster::presentation ALIAS farcaster_presentation)

target_include_directories(farcaster_presentation
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/qt"
)

target_link_libraries(farcaster_presentation
    PUBLIC
        farcaster::application
        Qt6::Core
    PRIVATE
        Qt6::Gui
)

target_compile_features(farcaster_presentation PUBLIC cxx_std_26)
set_property(TARGET farcaster_presentation PROPERTY CXX_SCAN_FOR_MODULES ON)
set_target_properties(farcaster_presentation PROPERTIES AUTOMOC ON)
farcaster_enable_warnings(farcaster_presentation)
