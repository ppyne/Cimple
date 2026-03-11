cmake_minimum_required(VERSION 3.15)

# Helper function to add a .cim test
function(add_cim_test test_name cim_file expected_output)
    add_test(
        NAME "${test_name}"
        COMMAND cimple run "${CMAKE_CURRENT_SOURCE_DIR}/${cim_file}"
    )
    set_tests_properties("${test_name}" PROPERTIES
        PASS_REGULAR_EXPRESSION "${expected_output}"
    )
endfunction()

# Check-only tests (syntax/semantic validation)
function(add_cim_check_test test_name cim_file should_pass)
    if(should_pass)
        add_test(
            NAME "${test_name}"
            COMMAND cimple check "${CMAKE_CURRENT_SOURCE_DIR}/${cim_file}"
        )
    else()
        add_test(
            NAME "${test_name}"
            COMMAND cimple check "${CMAKE_CURRENT_SOURCE_DIR}/${cim_file}"
        )
        set_tests_properties("${test_name}" PROPERTIES WILL_FAIL TRUE)
    endif()
endfunction()

# Basic tests
add_cim_test(test_hello     hello.cim       "Hello, World!")
add_cim_test(test_arithmetic arithmetic.cim "42")
add_cim_test(test_strings   strings.cim     "Hello World")
add_cim_test(test_arrays    arrays.cim      "3")
add_cim_test(test_loops     loops.cim       "10")
add_cim_test(test_functions functions.cim   "55")
add_cim_test(test_booleans  booleans.cim    "true")
add_cim_test(test_math      math.cim        "3.141")
