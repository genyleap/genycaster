if(EMSCRIPTEN)
    # Browser builds have portable Boost headers but no native BoostConfig package.
    include("${CMAKE_SOURCE_DIR}/cmake/platform/WebAssembly/BoostHeaders.cmake")
elseif(NOT TARGET Boost::headers)
    # Android defines Boost::headers at the composition root. Native hosts
    # continue to discover their installed Boost package normally.
    find_package(Boost 1.75 REQUIRED)
endif()

# Boost.JSON's compiled implementation is shared by the independent Snapchain
# signer-registry and canonical-write adapters. Keeping one archive prevents
# duplicate implementation symbols when shipping composition links both.
add_library(farcaster_snapchain_boost_json STATIC boost_json.cpp)
target_link_libraries(farcaster_snapchain_boost_json PRIVATE Boost::headers)
target_compile_definitions(farcaster_snapchain_boost_json PRIVATE BOOST_JSON_NO_LIB)
target_compile_features(farcaster_snapchain_boost_json PRIVATE cxx_std_26)
farcaster_enable_warnings(farcaster_snapchain_boost_json)

if(FARCASTER_ENABLE_SNAPCHAIN_READ_PROVIDER)
    # Built-in provider adapters are statically composed into the client.
# On Windows, C++26 module exports in DLL targets are not portable ABI exports.
# Use an explicit archive regardless of BUILD_SHARED_LIBS/vcpkg triplet.
add_library(farcaster_provider_snapchain_read STATIC)
    add_library(farcaster::provider_snapchain_read ALIAS farcaster_provider_snapchain_read)
    target_sources(farcaster_provider_snapchain_read
        PUBLIC FILE_SET CXX_MODULES FILES read.cppm
        PRIVATE read.cpp
    )
    target_link_libraries(farcaster_provider_snapchain_read
        PUBLIC
            farcaster::application
            farcaster::capabilities
            farcaster::domain
            farcaster::provider
            farcaster::storage
            farcaster::transport
            farcaster::execution
        PRIVATE farcaster_snapchain_boost_json Boost::headers
    )
    target_compile_definitions(farcaster_provider_snapchain_read PRIVATE BOOST_JSON_NO_LIB)
    target_compile_features(farcaster_provider_snapchain_read PUBLIC cxx_std_26)
    set_property(TARGET farcaster_provider_snapchain_read PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_provider_snapchain_read)
endif()

if(FARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER)
    # Built-in provider adapters are statically composed into the client.
# On Windows, C++26 module exports in DLL targets are not portable ABI exports.
# Use an explicit archive regardless of BUILD_SHARED_LIBS/vcpkg triplet.
add_library(farcaster_provider_snapchain_signer STATIC)
    add_library(farcaster::provider_snapchain_signer ALIAS farcaster_provider_snapchain_signer)

    target_sources(farcaster_provider_snapchain_signer
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            signer.cppm
    PRIVATE
        signer.cpp
    )

    target_link_libraries(farcaster_provider_snapchain_signer
    PUBLIC
        farcaster::capabilities
        farcaster::domain
        farcaster::transport
    PRIVATE
        farcaster::protocol
        farcaster_snapchain_boost_json
        Boost::headers
    )

    target_compile_definitions(farcaster_provider_snapchain_signer PRIVATE BOOST_JSON_NO_LIB)
    target_compile_features(farcaster_provider_snapchain_signer PUBLIC cxx_std_26)
    set_property(TARGET farcaster_provider_snapchain_signer PROPERTY CXX_SCAN_FOR_MODULES ON)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS farcaster_gcc14_snapchain_modules=1)
    set_property(TARGET farcaster_provider_snapchain_signer PROPERTY JOB_POOL_COMPILE farcaster_gcc14_snapchain_modules)
    endif()

    farcaster_enable_warnings(farcaster_provider_snapchain_signer)
endif()

if(FARCASTER_ENABLE_SNAPCHAIN_WRITE_PROVIDER)
    # Built-in provider adapters are statically composed into the client.
# On Windows, C++26 module exports in DLL targets are not portable ABI exports.
# Use an explicit archive regardless of BUILD_SHARED_LIBS/vcpkg triplet.
add_library(farcaster_provider_snapchain_write STATIC)
    add_library(farcaster::provider_snapchain_write ALIAS farcaster_provider_snapchain_write)

    target_sources(farcaster_provider_snapchain_write
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            write.cppm
    PRIVATE
        write.cpp
    )

    target_link_libraries(farcaster_provider_snapchain_write
    PUBLIC
        farcaster::capabilities
        farcaster::domain
        farcaster::transport
    PRIVATE
        farcaster_snapchain_boost_json
        Boost::headers
    )

    target_compile_definitions(farcaster_provider_snapchain_write PRIVATE BOOST_JSON_NO_LIB)
    target_compile_features(farcaster_provider_snapchain_write PUBLIC cxx_std_26)
    set_property(TARGET farcaster_provider_snapchain_write PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_provider_snapchain_write)
endif()
