# Unit tests
cmake_minimum_required(VERSION 3.8.2)

# Find / download Catch 2
set(CATCH_HEADER "${CMAKE_CURRENT_LIST_DIR}/catch.hpp")

if (NOT EXISTS ${CATCH_HEADER})
    message("-- Catch2 header (${CATCH_HEADER}) not found, downloading...")
    file(
        DOWNLOAD "https://github.com/catchorg/Catch2/releases/download/v2.7.2/catch.hpp" ${CATCH_HEADER}
        EXPECTED_HASH SHA256=725a002a3837e343056ead2c2ffad6c1d9680b675635e81d53e7fd765be27909)
endif()

# Catch main
add_library(catch_main catch_main.cpp)

if (MSVC)
    target_compile_options(catch_main PUBLIC /std:c++17 /permissive-)
else()
    target_compile_options(catch_main PUBLIC -std=c++17)
endif()

# Tests

function(add_mg_test name)
    add_executable(${name} ${name}.cpp)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unittest-cpp)
    target_link_libraries(${name} catch_main mg_engine)

    if (MSVC)
        # Ignore warnings about not using non-standard extensions
        target_compile_definitions(${name} PRIVATE _SCL_SECURE_NO_WARNINGS _CRT_SECURE_NO_WARNINGS)
    endif()

    # Hack to make ctest build the tests before running them
    add_test(ctest_build_${name} "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target ${name})

    add_test(NAME ${name}
        COMMAND "${OUTPUT_DIRECTORY}${name}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set_tests_properties(${name} PROPERTIES DEPENDS ctest_build_${name})
endfunction()

add_mg_test(slot_map_test)

add_mg_test(string_hash_test)

add_mg_test(utils_test)

add_mg_test(config_test)

add_mg_test(entity_test)

add_mg_test(resource_cache_test)

add_mg_test(thread_pool_test)

add_mg_test(gsl_test)

add_mg_test(observer_test)

