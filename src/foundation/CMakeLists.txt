add_library(farcaster_foundation)
add_library(farcaster::foundation ALIAS farcaster_foundation)

target_sources(farcaster_foundation
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            error.cppm
            result.cppm
            time.cppm
            utf8.cppm
    PRIVATE
        error.cpp
        time.cpp
        utf8.cpp
)

target_compile_features(farcaster_foundation PUBLIC cxx_std_26)
set_property(TARGET farcaster_foundation PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_foundation)
