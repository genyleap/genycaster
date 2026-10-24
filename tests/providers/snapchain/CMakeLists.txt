if(FARCASTER_ENABLE_SNAPCHAIN_READ_PROVIDER)
    add_executable(farcaster_snapchain_read_contract_tests read_contract_tests.cpp)
    target_link_libraries(farcaster_snapchain_read_contract_tests
        PRIVATE farcaster::provider_snapchain_read farcaster::transport
    )
    target_compile_features(farcaster_snapchain_read_contract_tests PRIVATE cxx_std_26)
    set_property(TARGET farcaster_snapchain_read_contract_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_snapchain_read_contract_tests)
    add_test(NAME provider.snapchain.raw_read.contract COMMAND farcaster_snapchain_read_contract_tests)
    set_tests_properties(provider.snapchain.raw_read.contract PROPERTIES LABELS "contract;provider;read")
endif()

if(FARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER)
    add_executable(farcaster_snapchain_signer_contract_tests contract_tests.cpp)

    target_link_libraries(farcaster_snapchain_signer_contract_tests
    PRIVATE
        farcaster::provider_snapchain_signer
        farcaster::transport
    )

    target_compile_features(farcaster_snapchain_signer_contract_tests PRIVATE cxx_std_26)
    set_property(TARGET farcaster_snapchain_signer_contract_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_snapchain_signer_contract_tests)

    add_test(NAME provider.snapchain.signer.contract COMMAND farcaster_snapchain_signer_contract_tests)
    set_tests_properties(provider.snapchain.signer.contract PROPERTIES LABELS "contract;provider;signer")
endif()

if(FARCASTER_ENABLE_SNAPCHAIN_WRITE_PROVIDER)
    add_executable(farcaster_snapchain_write_contract_tests write_contract_tests.cpp)
    target_link_libraries(farcaster_snapchain_write_contract_tests
    PRIVATE
        farcaster::provider_snapchain_write
        farcaster::transport
    )
    target_compile_features(farcaster_snapchain_write_contract_tests PRIVATE cxx_std_26)
    set_property(TARGET farcaster_snapchain_write_contract_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_snapchain_write_contract_tests)
    add_test(NAME provider.snapchain.write.contract COMMAND farcaster_snapchain_write_contract_tests)
    set_tests_properties(provider.snapchain.write.contract PROPERTIES LABELS "contract;provider;write")
endif()
