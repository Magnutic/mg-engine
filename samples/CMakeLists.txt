# Interactive samples / tests

set(MG_BINARY_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/")

function (add_mg_sample name)
    add_executable (${name} ${name}.cpp)
    target_link_libraries (${name} mg_engine)
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${MG_BINARY_OUTPUT_DIR}")
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${MG_BINARY_OUTPUT_DIR}")
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${MG_BINARY_OUTPUT_DIR}")
endfunction()

add_mg_sample(test_scene)

add_mg_sample(input)

