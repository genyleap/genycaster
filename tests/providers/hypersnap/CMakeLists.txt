# The contract suite currently exercises the libcurl transport adapter.
# Android intentionally uses Qt Network and does not build transport_curl.
# Do not create a dangling link target in Android test configurations.
if(NOT TARGET farcaster::transport_curl)
    message(STATUS
        "Skipping Hypersnap curl contract tests: farcaster::transport_curl is unavailable")
    return()
endif()

add_executable(farcaster_hypersnap_contract_tests
    contract_tests.cpp
    fixtures.cpp
    fake_transport.cpp
)

target_sources(farcaster_hypersnap_contract_tests
    PRIVATE
        FILE_SET CXX_MODULES
        FILES
            fixtures.cppm
            fake_transport.cppm
)

target_link_libraries(farcaster_hypersnap_contract_tests
    PRIVATE
        farcaster::provider_hypersnap
        farcaster::storage
        farcaster::transport
        farcaster::transport_curl
)

target_compile_features(farcaster_hypersnap_contract_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_hypersnap_contract_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_hypersnap_contract_tests)

add_test(NAME provider.hypersnap.contract COMMAND farcaster_hypersnap_contract_tests)
set_tests_properties(provider.hypersnap.contract PROPERTIES LABELS "contract;provider")
