cmake_minimum_required(VERSION 3.11)
find_package(Git REQUIRED)

if(NOT DEFINED DEPENDENCY_SUBMODULES)
    message(FATAL_ERROR "init_submodules.cmake called without DEPENDENCY_SUBMODULES defined.")
endif()

set(SUBMODULES_DIR "${MG_SOURCE_DIR}/external/submodules")

foreach(SUBMODULE ${DEPENDENCY_SUBMODULES})
    message("Initialising submodule ${SUBMODULE}")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init "${SUBMODULE}"
        WORKING_DIRECTORY "${SUBMODULES_DIR}"
        RESULT_VARIABLE SUCCESS
    )
    if (NOT SUCCESS EQUAL 0)
        # Warn rather than error, since this can happen if you are working on different branches
        # where one branch added a new submodule -- when switching to the other, the submodule
        # repository is still checked out, but not known to Git.
        message(WARNING "Failed to init submodule ${SUBMODULE}.")
    endif()
endforeach()
