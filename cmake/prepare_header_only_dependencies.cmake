cmake_minimum_required(VERSION 3.11)

list(APPEND MG_HEADER_ONLY_DEPENDENCIES function2 plf_colony optional stb imgui)

get_filename_component(THIS_SCRIPT_DIR "${CMAKE_SCRIPT_MODE_FILE}" DIRECTORY)
get_filename_component(MG_SOURCE_DIR "${THIS_SCRIPT_DIR}/.." ABSOLUTE)
set(MG_DEPENDENCIES_ARCHIVE "${MG_SOURCE_DIR}/external/mg_dependencies_src.zip")
set(MG_DEPENDENCIES_INSTALL_DIR "${MG_SOURCE_DIR}/external/mg_dependencies")
set(MG_DEPENDENCIES_SOURCE_DIR "${MG_SOURCE_DIR}/external/mg_dependencies_src")

if(NOT EXISTS "${MG_DEPENDENCIES_SOURCE_DIR}")
    if(EXISTS "${MG_DEPENDENCIES_ARCHIVE}")
        message("${MG_DEPENDENCIES_ARCHIVE} found. Extracting...")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf "${MG_DEPENDENCIES_ARCHIVE}" --format=zip
            WORKING_DIRECTORY "${MG_DEPENDENCIES_SOURCE_DIR}"
            RESULT_VARIABLE EXTRACT_RESULT
        )
        if(NOT ${EXTRACT_RESULT} EQUAL 0)
            message(FATAL_ERROR "Failed to extract ${MG_DEPENDENCIES_ARCHIVE} to ${MG_DEPENDENCIES_SOURCE_DIR}")
        endif()

    else()
        message("${MG_DEPENDENCIES_ARCHIVE} does not exist. Using git submodules to get header-only dependencies...")
        set(DEPENDENCY_SUBMODULES ${MG_HEADER_ONLY_DEPENDENCIES})
        include(cmake/init_submodules.cmake)
        set(MG_DEPENDENCIES_SOURCE_DIR "${MG_SOURCE_DIR}/external/submodules")

    endif()
endif()

if(NOT EXISTS "${MG_DEPENDENCIES_INSTALL_DIR}")
    file(MAKE_DIRECTORY "${MG_DEPENDENCIES_INSTALL_DIR}")
endif()

foreach(DEPENDENCY ${MG_HEADER_ONLY_DEPENDENCIES})
    message("-- Copying header-only depencency ${DEPENDENCY} from ${MG_DEPENDENCIES_SOURCE_DIR}/${DEPENDENCY} to ${MG_DEPENDENCIES_INSTALL_DIR}")
    file(COPY "${MG_DEPENDENCIES_SOURCE_DIR}/${DEPENDENCY}" DESTINATION "${MG_DEPENDENCIES_INSTALL_DIR}")
endforeach()
