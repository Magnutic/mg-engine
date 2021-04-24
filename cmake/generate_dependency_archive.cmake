cmake_minimum_required(VERSION 3.11)

find_package(Git REQUIRED)

if (DEFINED CMAKE_SCRIPT_MODE_FILE)
    get_filename_component(THIS_SCRIPT_DIR "${CMAKE_SCRIPT_MODE_FILE}" DIRECTORY)
    set(MG_SOURCE_DIR "${THIS_SCRIPT_DIR}/..")
    get_filename_component(MG_SOURCE_DIR "${MG_SOURCE_DIR}" ABSOLUTE)
    set(MG_DEPENDENCIES_ARCHIVE "${MG_SOURCE_DIR}/external/mg_dependencies_src.zip")
endif()

file(GLOB DEPENDENCY_SUBMODULES "${MG_SOURCE_DIR}/external/submodules/*")
message("----- Generating ${MG_DEPENDENCIES_ARCHIVE} from submodules: ${DEPENDENCY_SUBMODULES} -----")
include("${THIS_SCRIPT_DIR}/init_submodules.cmake")

message("Compressing archive ${MG_DEPENDENCIES_ARCHIVE}")
execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar cf "${MG_DEPENDENCIES_ARCHIVE}" --format=zip -- ${DEPENDENCY_SUBMODULES}
    WORKING_DIRECTORY "${SUBMODULES_DIR}"
    RESULT_VARIABLE COMPRESS_RESULT
)
if (COMPRESS_RESULT EQUAL 0)
    message("Successfully created ${MG_DEPENDENCIES_ARCHIVE}.")
else()
    message(FATAL_ERROR "Failed to create ${MG_DEPENDENCIES_ARCHIVE}")
endif()
