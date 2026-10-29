# Android/WASM already provide the portable Boost::headers target; avoid
# asking the IDE's package provider to locate a host BoostConfig package.
if(NOT TARGET Boost::headers)
    find_package(Boost 1.75 REQUIRED)
endif()

if(FARCASTER_ENABLE_BROWSER_WALLET_AUTHORIZER)
    add_executable(farcaster_provider_browser_wallet_contract_tests contract_tests.cpp)
    target_link_libraries(farcaster_provider_browser_wallet_contract_tests
        PRIVATE
            farcaster::provider_compatibility_browser_wallet
            farcaster::capabilities
            farcaster::domain
            farcaster::foundation
            farcaster::protocol
            Boost::headers
    )
    target_compile_features(farcaster_provider_browser_wallet_contract_tests PRIVATE cxx_std_26)
    farcaster_enable_warnings(farcaster_provider_browser_wallet_contract_tests)
    add_test(NAME provider.browser_wallet.contract COMMAND farcaster_provider_browser_wallet_contract_tests)
    set_tests_properties(provider.browser_wallet.contract PROPERTIES LABELS "provider;contract;signer;wallet")
endif()

if(FARCASTER_ENABLE_BROWSER_WALLET_BRIDGE)
    add_executable(farcaster_provider_browser_wallet_bridge_contract_tests bridge_contract_tests.cpp)
    target_link_libraries(farcaster_provider_browser_wallet_bridge_contract_tests
        PRIVATE
            farcaster::provider_compatibility_browser_wallet
            farcaster::capabilities
            farcaster::foundation
            Boost::headers
    )
    target_compile_definitions(farcaster_provider_browser_wallet_bridge_contract_tests PRIVATE BOOST_JSON_NO_LIB)
    target_compile_features(farcaster_provider_browser_wallet_bridge_contract_tests PRIVATE cxx_std_26)
    farcaster_enable_warnings(farcaster_provider_browser_wallet_bridge_contract_tests)
    add_test(NAME provider.browser_wallet.bridge.contract COMMAND farcaster_provider_browser_wallet_bridge_contract_tests)
    set_tests_properties(provider.browser_wallet.bridge.contract PROPERTIES LABELS "provider;contract;wallet;miniapp")
endif()
