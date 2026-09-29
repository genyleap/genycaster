add_executable(openproof_auth_contract_tests contract_tests.cpp)

target_link_libraries(openproof_auth_contract_tests
    PRIVATE
        farcaster::provider_openproof_auth
        farcaster::transport
)

target_compile_features(openproof_auth_contract_tests PRIVATE cxx_std_26)
set_property(TARGET openproof_auth_contract_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(openproof_auth_contract_tests)

add_test(NAME provider.openproof_auth.contract COMMAND openproof_auth_contract_tests)
set_tests_properties(provider.openproof_auth.contract PROPERTIES LABELS "contract;provider;auth;openproof")
