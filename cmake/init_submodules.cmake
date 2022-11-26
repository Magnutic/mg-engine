cmake_minimum_required(VERSION 3.11)
find_package(Git REQUIRED)

list(APPEND assimp_PATCHES "${MG_SOURCE_DIR}/external/patches/assimp/remove-werror.patch")

file(GLOB SUBMODULES_GLOB_RESULTS "${MG_SOURCE_DIR}/external/submodules/*")
foreach(SUBMODULES_GLOB_RESULT ${SUBMODULES_GLOB_RESULTS})
    if(IS_DIRECTORY "${SUBMODULES_GLOB_RESULT}")
        list(APPEND DEPENDENCY_SUBMODULES "${SUBMODULES_GLOB_RESULT}")
    endif()
endforeach()

if(NOT DEFINED MG_SOURCE_DIR)
    message(FATAL_ERROR "init_submodules.cmake called without MG_SOURCE_DIR defined.")
endif()

set(SUBMODULES_DIR "${MG_SOURCE_DIR}/external/submodules")

foreach(SUBMODULE ${DEPENDENCY_SUBMODULES})
    message(STATUS "Initializing submodule ${SUBMODULE}")
    file(RELATIVE_PATH SUBMODULE_NAME ${SUBMODULES_DIR} ${SUBMODULE})
    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive "${SUBMODULE}"
        WORKING_DIRECTORY "${SUBMODULES_DIR}"
        RESULT_VARIABLE SUCCESS
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C "${SUBMODULE}" checkout
        WORKING_DIRECTORY "${SUBMODULES_DIR}"
        RESULT_VARIABLE SUCCESS
    )
    if(NOT SUCCESS EQUAL 0)
        # Warn rather than error, since this can happen if you are working on different branches
        # where one branch added a new submodule -- when switching to the other, the submodule
        # repository is still checked out, but not known to Git.
        message(WARNING "Failed to init submodule ${SUBMODULE}.")
    else()
        # Apply patches, if any.
        file(GLOB PATCHES "${MG_SOURCE_DIR}/external/patches/${SUBMODULE_NAME}/*.patch")
        foreach(PATCH ${PATCHES})
            message(STATUS "Applying patch ${PATCH}...")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} -C "${SUBMODULE}" apply "${PATCH}"
                RESULT_VARIABLE PATCH_RESULT
            )
            if(NOT PATCH_RESULT EQUAL 0)
                message(FATAL_ERROR "**ERROR** failed to apply patch ${PATCH} to dependency ${DEPENDENCY}")
            endif()
        endforeach()

        # Write commit id to a file.
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
            WORKING_DIRECTORY "${SUBMODULES_DIR}"
            OUTPUT_FILE "${SUBMODULES_DIR}/${SUBMODULE_NAME}_revision"
        )
    endif()
endforeach()
