# Fetch and create archive for Mg Engine's dependencies.
# The resulting archive is intended to be bundled along with Mg Engine.

set(MG_DEPENDENCIES_ZIP_PATH "${MG_SOURCE_DIR}/external/mg_dependencies.zip")

message("-- NOTE: Generating ${MG_DEPENDENCIES_ZIP_PATH}: initialising submodules...")
foreach(SUBMODULE ${MG_DEPENDENCIES})
    execute_process(
        COMMAND git submodule update --init -- ${SUBMODULE}
        WORKING_DIRECTORY "${MG_SOURCE_DIR}/external/submodules"
    )
endforeach()

message("-- NOTE: Generating ${MG_DEPENDENCIES_ZIP_PATH}: zipping dependencies...")
"${MG_DEPENDENCIES}"
function(create_zip OUTPUT_FILE INPUT_FILES)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar "cf" "${OUTPUT_FILE}" --format=zip -- ${INPUT_FILES}
        WORKING_DIRECTORY "${MG_SOURCE_DIR}/external/submodules"
    )
endfunction()

message("-- NOTE: Generating ${MG_DEPENDENCIES_ZIP_PATH}: done.")
