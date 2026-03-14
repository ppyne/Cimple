cmake_minimum_required(VERSION 3.15)

add_test(
    NAME cimple_test_suite
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(cimple_test_suite PROPERTIES
    ENVIRONMENT "CIMPLE_BIN=$<TARGET_FILE:cimple>"
    TIMEOUT 120
    LABELS "unit"
)

if(EMSCRIPTEN)
    add_test(
        NAME cimple_wasm_test_suite
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/wasm/run_wasm_tests.sh"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(cimple_wasm_test_suite PROPERTIES
        ENVIRONMENT "CIMPLE_BIN=$<TARGET_FILE:cimple>;CIMPLE_WASM=$<TARGET_FILE:cimple>"
        TIMEOUT 120
        LABELS "wasm"
    )
elseif(DEFINED CIMPLE_WASM_TEST_BIN AND NOT CIMPLE_WASM_TEST_BIN STREQUAL "")
    add_test(
        NAME cimple_wasm_test_suite
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/wasm/run_wasm_tests.sh"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(cimple_wasm_test_suite PROPERTIES
        ENVIRONMENT "CIMPLE_BIN=$<TARGET_FILE:cimple>;CIMPLE_WASM=${CIMPLE_WASM_TEST_BIN}"
        TIMEOUT 120
        LABELS "wasm"
    )
endif()
