cmake_minimum_required(VERSION 3.11)
find_package(Git REQUIRED)

if(NOT DEFINED MG_SOURCE_DIR)
    message(FATAL_ERROR "init_submodules.cmake called without MG_SOURCE_DIR defined.")
endif()

set(SUBMODULES_DIR "${MG_SOURCE_DIR}/external/submodules")

execute_process(
    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
    WORKING_DIRECTORY "${MG_SOURCE_DIR}"
)

# For each submodule, write commit id to a file.
file(GLOB SUBMODULES_GLOB_RESULTS "${SUBMODULES_DIR}/*")
foreach(SUBMODULES_GLOB_RESULT ${SUBMODULES_GLOB_RESULTS})
    if(IS_DIRECTORY "${SUBMODULES_GLOB_RESULT}")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
            WORKING_DIRECTORY "${SUBMODULES_DIR}"
            OUTPUT_FILE "${SUBMODULES_DIR}/${SUBMODULE_NAME}_revision"
        )
    endif()
endforeach()
