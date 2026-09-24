add_executable(farcaster_signer_attestation_contract_tests contract_tests.cpp)

target_link_libraries(farcaster_signer_attestation_contract_tests
    PRIVATE
        farcaster::provider_compatibility_signer_attestation
        farcaster::transport
)

target_compile_features(farcaster_signer_attestation_contract_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_signer_attestation_contract_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_signer_attestation_contract_tests)

add_test(NAME provider.signer_attestation.contract COMMAND farcaster_signer_attestation_contract_tests)
set_tests_properties(provider.signer_attestation.contract PROPERTIES LABELS "contract;provider;signer;security")
