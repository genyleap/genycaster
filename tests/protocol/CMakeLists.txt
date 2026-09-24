add_executable(farcaster_protocol_contract_tests contract_tests.cpp)

target_link_libraries(farcaster_protocol_contract_tests
    PRIVATE
        farcaster::domain
        farcaster::foundation
        farcaster::protocol
)

target_compile_features(farcaster_protocol_contract_tests PRIVATE cxx_std_26)
set_property(TARGET farcaster_protocol_contract_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_protocol_contract_tests)

add_test(NAME protocol.message.contract COMMAND farcaster_protocol_contract_tests)
set_tests_properties(protocol.message.contract PROPERTIES LABELS "contract;protocol;signer")

if(FARCASTER_ENABLE_PROTOCOL_SIGNING)
    add_executable(farcaster_protocol_write_contract_tests write_contract_tests.cpp)
    target_link_libraries(farcaster_protocol_write_contract_tests PRIVATE farcaster::protocol_write)
    target_compile_features(farcaster_protocol_write_contract_tests PRIVATE cxx_std_26)
    set_property(TARGET farcaster_protocol_write_contract_tests PROPERTY CXX_SCAN_FOR_MODULES ON)
    farcaster_enable_warnings(farcaster_protocol_write_contract_tests)
    add_test(NAME protocol.write.contract COMMAND farcaster_protocol_write_contract_tests)
    set_tests_properties(protocol.write.contract PROPERTIES LABELS "contract;protocol;write")
endif()
