add_executable(farcaster_auth_contract_tests contract_tests.cpp)

target_link_libraries(farcaster_auth_contract_tests
    PRIVATE
        farcaster::provider_compatibility_auth
        farcaster::transport
)

target_compile_features(farcaster_auth_contract_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_auth_contract_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_auth_contract_tests)

add_test(NAME provider.farcaster_auth.contract COMMAND farcaster_auth_contract_tests)
set_tests_properties(provider.farcaster_auth.contract PROPERTIES LABELS "contract;provider;auth")
