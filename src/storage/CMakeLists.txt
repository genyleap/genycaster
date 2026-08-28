# SQLite is a C boundary. On Apple, do not use CMake's SQLite::SQLite3 imported
# target because FindSQLite3 may propagate the SDK's usr/include directory as
# an explicit -isystem path. With non-Apple Clang toolchains (for example
# Homebrew LLVM), that can bypass Clang's resource headers and break Darwin
# typedef discovery while compiling C++ module units. The SDK is already
# searched through CMAKE_OSX_SYSROOT, so only the library itself is required.
if(APPLE)
    find_library(FARCASTER_SQLITE3_LIBRARY NAMES sqlite3 REQUIRED)
else()
    find_package(SQLite3 REQUIRED)
endif()

add_library(farcaster_storage)
add_library(farcaster::storage ALIAS farcaster_storage)

target_sources(farcaster_storage
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            entity_store.cppm
            timeline_store.cppm
            continuation_store.cppm
            checkpoint_store.cppm
            read_store.cppm
            session_store.cppm
            signer_store.cppm
    PRIVATE
        sqlite_boundary.cpp
        sqlite_boundary.hpp
        entity_store.cpp
        timeline_store.cpp
        continuation_store.cpp
        checkpoint_store.cpp
        read_store.cpp
        session_store.cpp
        signer_store.cpp
)

target_include_directories(farcaster_storage PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(farcaster_storage
    PUBLIC
        farcaster::foundation
        farcaster::capabilities
        farcaster::local_state
        farcaster::provider
        farcaster::sync
)

if(APPLE)
    target_link_libraries(farcaster_storage PRIVATE "${FARCASTER_SQLITE3_LIBRARY}")
else()
    target_link_libraries(farcaster_storage PRIVATE SQLite::SQLite3)
endif()

target_compile_features(farcaster_storage PUBLIC cxx_std_26)
set_property(TARGET farcaster_storage PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_storage)
