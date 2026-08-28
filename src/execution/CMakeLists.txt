add_library(farcaster_execution)
add_library(farcaster::execution ALIAS farcaster_execution)

target_sources(farcaster_execution
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            serial.cppm
    PRIVATE
        serial.cpp
)

target_link_libraries(farcaster_execution PUBLIC farcaster::application_executor farcaster::foundation)
target_compile_features(farcaster_execution PUBLIC cxx_std_26)
set_property(TARGET farcaster_execution PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_execution)
