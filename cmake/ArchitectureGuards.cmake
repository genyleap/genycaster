function(farcaster_register_architecture_guard)
    if(NOT BUILD_TESTING)
        return()
    endif()

    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    add_test(
        NAME architecture_guard
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ci/check_architecture.py" "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    set_tests_properties(architecture_guard PROPERTIES LABELS "architecture")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ui/i18n/qml_en_US.ts")
        add_test(
            NAME localization_catalog_guard
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/ci/check_localization_catalog.py"
                "${CMAKE_CURRENT_SOURCE_DIR}"
        )
        set_tests_properties(localization_catalog_guard PROPERTIES LABELS "architecture;localization")
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/services/media-gateway/gateway.mjs")
        add_test(
            NAME media_upload_contract_guard
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/ci/check_media_upload_contract.py"
                "${CMAKE_CURRENT_SOURCE_DIR}"
        )
        set_tests_properties(media_upload_contract_guard PROPERTIES LABELS "architecture;media;upload")
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/ci/check_async_resource_contract.py")
        add_test(
            NAME async_resource_contract_guard
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/ci/check_async_resource_contract.py" "${CMAKE_SOURCE_DIR}"
        )
        set_tests_properties(async_resource_contract_guard PROPERTIES LABELS "architecture;async;resources")
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/ci/check_explorer_semantic_contract.py")
        add_test(
            NAME explorer_semantic_contract_guard
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/ci/check_explorer_semantic_contract.py" "${CMAKE_SOURCE_DIR}"
        )
        set_tests_properties(explorer_semantic_contract_guard PROPERTIES LABELS "architecture;explorer;network")
    endif()

endfunction()
