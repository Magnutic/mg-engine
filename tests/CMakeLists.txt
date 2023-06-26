# Unit tests
cmake_minimum_required(VERSION 3.8.2)

# Find / download Catch 2
set(CATCH_HEADER "${CMAKE_CURRENT_LIST_DIR}/catch.hpp")

set(CATCH_URL "https://github.com/catchorg/Catch2/releases/download/v2.13.10/catch.hpp")
set(CATCH_HASH_EXPECTED "3725c0f0a75f376a5005dde31ead0feb8f7da7507644c201b814443de8355170")

if (EXISTS ${CATCH_HEADER})
    file(SHA256 ${CATCH_HEADER} CATCH_HASH)
endif()

if (NOT EXISTS ${CATCH_HEADER} OR NOT ${CATCH_HASH} STREQUAL ${CATCH_HASH_EXPECTED})
    message(STATUS "Catch2 header (${CATCH_HEADER}) not found, downloading...")
    file(DOWNLOAD ${CATCH_URL} ${CATCH_HEADER} EXPECTED_HASH "SHA256=${CATCH_HASH_EXPECTED}")
    file(SHA256 ${CATCH_HEADER} CATCH_HASH)
endif()

if (NOT EXISTS ${CATCH_HEADER} OR NOT ${CATCH_HASH} STREQUAL ${CATCH_HASH_EXPECTED})
    message(STATUS "CATCH_HEADER: ${CATCH_HEADER}")
    message(STATUS "CATCH_HASH: ${CATCH_HASH}")
    message(STATUS "CATCH_HASH_EXPECTED: ${CATCH_HASH_EXPECTED}")
    message(FATAL_ERROR "Failed to download Catch2 header.")
endif()

message(STATUS "Using Catch2 at ${CATCH_HEADER}")

# Catch main
add_library(catch_main catch_main.cpp)

if (MSVC)
    target_compile_options(catch_main PUBLIC /std:c++20 /permissive-)
else()
    target_compile_options(catch_main PUBLIC -std=c++20)
endif()

# Tests

set(MG_TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/test/")

function(add_mg_test name)
    add_executable(${name} ${name}.cpp)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unittest-cpp)
    target_link_libraries(${name} catch_main mg_engine)

    if (MSVC)
        # Ignore warnings about not using non-standard extensions
        target_compile_definitions(${name} PRIVATE _SCL_SECURE_NO_WARNINGS _CRT_SECURE_NO_WARNINGS)
    endif()

    # Hack to make ctest build the tests before running them
    add_test(ctest_build_${name} "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target ${name} -j 8)

    add_test(NAME ${name}
        COMMAND "${MG_TEST_OUTPUT_DIR}${name}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${MG_TEST_OUTPUT_DIR}")
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${MG_TEST_OUTPUT_DIR}")
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${MG_TEST_OUTPUT_DIR}")

    set_tests_properties(${name} PROPERTIES DEPENDS ctest_build_${name})
endfunction()

add_mg_test(slot_map_test)

add_mg_test(string_hash_test)

add_mg_test(utils_test)

add_mg_test(config_test)

add_mg_test(entity_test)

add_mg_test(resource_cache_test)

add_mg_test(thread_pool_test)
target_link_libraries(thread_pool_test function2)

add_mg_test(observer_test)

add_mg_test(unicode_test)
