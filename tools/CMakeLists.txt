# Tools for mg-engine.
cmake_minimum_required(VERSION 3.15)

set(MG_BINARY_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/")

function (mg_set_output_directory target_name)
    set_target_properties(${target_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${MG_BINARY_OUTPUT_DIR}")
    set_target_properties(${target_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${MG_BINARY_OUTPUT_DIR}")
    set_target_properties(${target_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${MG_BINARY_OUTPUT_DIR}")
endfunction()

add_subdirectory(shared)

add_subdirectory(mesh_converter)
