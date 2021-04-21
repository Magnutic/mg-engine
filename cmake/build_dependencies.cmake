# The 'cmake --install' command was introduced in 3.15 and is used by this script.
cmake_minimum_required(VERSION 3.15)

include(ProcessorCount)
ProcessorCount(NPROC)

get_filename_component(THIS_SCRIPT_DIR "${CMAKE_SCRIPT_MODE_FILE}" DIRECTORY)
get_filename_component(MG_SOURCE_DIR "${THIS_SCRIPT_DIR}/.." ABSOLUTE)
set(MG_DEPENDENCIES_ARCHIVE "${MG_SOURCE_DIR}/external/mg_dependencies_src.zip")
set(MG_DEPENDENCIES_SOURCE_DIR "${MG_SOURCE_DIR}/external/mg_dependencies_src")
set(MG_DEPENDENCIES_BUILD_DIR "${MG_SOURCE_DIR}/external/build")
set(MG_DEPENDENCIES_INSTALL_DIR "${MG_SOURCE_DIR}/external/mg_dependencies")

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
        file(GLOB DEPENDENCY_SUBMODULES "${MG_SOURCE_DIR}/external/submodules/*")
        include(cmake/init_submodules.cmake)
        set(MG_DEPENDENCIES_SOURCE_DIR "${MG_SOURCE_DIR}/external/submodules")

    endif()
endif()

file(REMOVE_RECURSE "${MG_DEPENDENCIES_BUILD_DIR}")
file(MAKE_DIRECTORY "${MG_DEPENDENCIES_BUILD_DIR}")

file(REMOVE_RECURSE "${MG_DEPENDENCIES_INSTALL_DIR}")
file(MAKE_DIRECTORY "${MG_DEPENDENCIES_INSTALL_DIR}")

message("----- NOTE: Preparing header-only dependencies -----")
include("${THIS_SCRIPT_DIR}/prepare_header_only_dependencies.cmake")

# Build and install dependencies by invoking CMake externally on them. This ad-hoc approach may be
# an unusual method, but importantly, I can understand how it works and reason about what happens
# unlike with more advanced dependency-management frameworks.

# Get absolute path of install directory.
message("----- NOTE: building dependencies and installing to ${MG_DEPENDENCIES_INSTALL_DIR} ----")

function(build_dependency DEPENDENCY BUILD_CONFIG)
    set(DEPENDENCY_SOURCE_DIR "${MG_DEPENDENCIES_SOURCE_DIR}/${DEPENDENCY}")
    set(DEPENDENCY_BUILD_DIR "${MG_DEPENDENCIES_BUILD_DIR}/${BUILD_CONFIG}/${DEPENDENCY}")

    message("----- NOTE: building ${DEPENDENCY} in configuration ${BUILD_CONFIG} with CMake parameters [${${DEPENDENCY}_EXTRA_BUILD_PARAMS}] -----")

    # Configure
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPENDENCY_BUILD_DIR}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "${DEPENDENCY_SOURCE_DIR}"
            "-DCMAKE_BUILD_TYPE=${BUILD_CONFIG}"
            "-DCMAKE_PREFIX_PATH=${MG_DEPENDENCIES_INSTALL_DIR}"
            "-DCMAKE_INSTALL_PREFIX=${MG_DEPENDENCIES_INSTALL_DIR}"
            ${${DEPENDENCY}_EXTRA_BUILD_PARAMS}
        WORKING_DIRECTORY ${DEPENDENCY_BUILD_DIR}
        RESULT_VARIABLE CONFIGURE_RESULT
    )
    if(NOT CONFIGURE_RESULT EQUAL 0)
        message(FATAL_ERROR "----- ERROR configuring dependency ${DEPENDENCY} -----")
    endif()

    # Build
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build . -j ${NPROC} --config ${BUILD_CONFIG}
        WORKING_DIRECTORY ${DEPENDENCY_BUILD_DIR}
        RESULT_VARIABLE BUILD_RESULT
    )
    if(NOT BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "----- ERROR building dependency ${DEPENDENCY} -----")
    endif()

    # Install
    message("----- NOTE: installing ${DEPENDENCY} in configuration ${BUILD_CONFIG} from ${DEPENDENCY_BUILD_DIR} to ${MG_DEPENDENCIES_INSTALL_DIR} -----")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --install . --config ${BUILD_CONFIG}
        WORKING_DIRECTORY ${DEPENDENCY_BUILD_DIR}
        RESULT_VARIABLE INSTALL_RESULT
    )
    if(NOT INSTALL_RESULT EQUAL 0)
        message(FATAL_ERROR "----- ERROR installing dependency ${DEPENDENCY} -----")
    endif()
endfunction()

# The list of dependencies. This corresponds to submodules in external/submodules.
# (They have to be explicitly listed since order matters due to dependencies between them.)
list(APPEND MG_DEPENDENCIES_TO_BUILD zlib libzip fmt glfw glm openal-soft assimp)

# Only build bundled libsndfile if system libsndfile is not available -- prefer system version since
# it otherwise causes conflicts with system-provided library.
find_package(SndFile QUIET)
if(NOT SndFile_FOUND)
    message("----- NOTE: System libsndfile not found. Building bundled libsndfile instead. -----")
    list(APPEND MG_DEPENDENCIES ogg vorbis libsndfile)
    list(APPEND MG_DEPENDENCIES_TO_BUILD ogg vorbis libsndfile)
endif()

# Some extra params passed into each build-configuration.
set(fmt_EXTRA_BUILD_PARAMS "-DFMT_TEST=0")
set(glfw_EXTRA_BUILD_PARAMS "-DGLFW_BUILD_EXAMPLES=0" "-DGLFW_BUILD_TESTS=0")
set(glm_EXTRA_BUILD_PARAMS "-DGLM_TEST_ENABLE=0")
set(libzip_EXTRA_BUILD_PARAMS "-DBUILD_TOOLS=0" "-DBUILD_REGRESS=0" "-DBUILD_EXAMPLES=0")
set(libsndfile_EXTRA_BUILD_PARAMS "-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=1" "-DBUILD_SHARED_LIBS=1"
    "-DBUILD_PROGRAMS=0" "-DBUILD_EXAMPLES=0" "-DBUILD_TESTING=0")
set(assimp_EXTRA_BUILD_PARAMS "-DASSIMP_BUILD_ASSIMP_TOOLS=0" "-DASSIMP_BUILD_TESTS=0"
    "-DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=0" "-DASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT=0"
    "-DASSIMP_BUILD_COLLADA_IMPORTER=1"
    "-DASSIMP_BUILD_OBJ_IMPORTER=1"
    "-DASSIMP_BUILD_FBX_IMPORTER=1"
    "-DASSIMP_BUILD_GLTF_IMPORTER=1")
set(openal-soft_EXTRA_BUILD_PARAMS "-DALSOFT_UTILS=0" "-DALSOFT_EXAMPLES=0")

# Build each dependency.
foreach(DEPENDENCY ${MG_DEPENDENCIES_TO_BUILD})
    build_dependency(${DEPENDENCY} "Debug")
    build_dependency(${DEPENDENCY} "Release")
endforeach()

message("Finished building dependencies.")
