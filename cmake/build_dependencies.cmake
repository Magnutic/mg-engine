cmake_minimum_required(VERSION 3.19)

find_package(Git REQUIRED)
include(ProcessorCount)
ProcessorCount(NPROC)

get_filename_component(MG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
set(MG_DEPENDENCIES_BUILD_DIR "${MG_SOURCE_DIR}/external/build")
set(MG_DEPENDENCIES_INSTALL_DIR "${MG_SOURCE_DIR}/external/mg_dependencies")

# The header-only dependencies. These names correspond to submodules in external/submodules.
list(APPEND MG_HEADER_ONLY_DEPENDENCIES function2 plf_colony optional stb imgui utf8.h)

# List of dependencies to build. These names corresponds to submodules in external/submodules.
# They have to be explicitly listed since order matters due to dependencies between them.
list(APPEND MG_DEPENDENCIES_TO_BUILD zlib libzip fmt glfw glm openal-soft assimp bullet3 hjson-cpp)

# Some extra params passed into each build-configuration.
set(fmt_EXTRA_BUILD_PARAMS "-DFMT_TEST=0")

set(glfw_EXTRA_BUILD_PARAMS "-DGLFW_BUILD_EXAMPLES=0" "-DGLFW_BUILD_TESTS=0")

set(glm_EXTRA_BUILD_PARAMS "-DGLM_TEST_ENABLE=0")

set(libzip_EXTRA_BUILD_PARAMS "-DBUILD_TOOLS=0" "-DBUILD_REGRESS=0" "-DBUILD_EXAMPLES=0")

set(libsndfile_EXTRA_BUILD_PARAMS "-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=1" "-DBUILD_SHARED_LIBS=1"
    "-DBUILD_PROGRAMS=0" "-DBUILD_EXAMPLES=0" "-DBUILD_TESTING=0")

set(assimp_EXTRA_BUILD_PARAMS
    "-DASSIMP_BUILD_ASSIMP_TOOLS=0"
    "-DASSIMP_BUILD_TESTS=0"
    "-DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=0"
    "-DASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT=0"
    "-DASSIMP_BUILD_COLLADA_IMPORTER=1"
    "-DASSIMP_BUILD_OBJ_IMPORTER=1"
    "-DASSIMP_BUILD_FBX_IMPORTER=1"
    "-DASSIMP_BUILD_GLTF_IMPORTER=1"
    "-DASSIMP_WARNINGS_AS_ERRORS=0")

set(openal-soft_EXTRA_BUILD_PARAMS "-DALSOFT_UTILS=0" "-DALSOFT_EXAMPLES=0")

set(bullet3_EXTRA_BUILD_PARAMS "-DBUILD_CPU_DEMOS=0" "-DUSE_GLUT=0" "-DBUILD_ENET=0"
    "-DBUILD_CLSOCKET=0" "-DBUILD_OPENGL3_DEMOS=0" "-DBUILD_BULLET2_DEMOS=0" "-DBUILD_EXTRAS=0"
    "-DBUILD_UNIT_TESTS=0")

set(hjson-cpp_EXTRA_BUILD_PARAMS "-DHJSON_ENABLE_INSTALL=1")

# Only build bundled libsndfile if system libsndfile is not available -- prefer system version since
# it otherwise causes conflicts with system-provided library.
find_package(SndFile QUIET)
if(NOT SndFile_FOUND)
    message(STATUS "System libsndfile not found. Building bundled libsndfile instead.")
    list(APPEND MG_DEPENDENCIES_TO_BUILD ogg vorbis libsndfile)
endif()

# Get dependencies from submodules.
message(STATUS "Using git submodules to get dependencies for Mg Engine...")
execute_process(
    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
    WORKING_DIRECTORY "${MG_SOURCE_DIR}"
)
set(MG_DEPENDENCIES_SOURCE_DIR "${MG_SOURCE_DIR}/external/submodules")

message(STATUS "Building dependencies and installing to ${MG_DEPENDENCIES_INSTALL_DIR}")

function(build_dependency DEPENDENCY BUILD_CONFIG IS_HEADER_ONLY EXTRA_BUILD_PARAMS)
    set(DEPENDENCY_SOURCE_DIR "${MG_DEPENDENCIES_SOURCE_DIR}/${DEPENDENCY}")
    set(DEPENDENCY_BUILD_DIR "${MG_DEPENDENCIES_BUILD_DIR}/${BUILD_CONFIG}/${DEPENDENCY}")
    set(DEPENDENCY_INSTALL_REVISION_FILE "${MG_DEPENDENCIES_INSTALL_DIR}/${DEPENDENCY}_revision_${BUILD_CONFIG}")

    # Check if we already have the dependency installed at the same revision.
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${DEPENDENCY_SOURCE_DIR}
        OUTPUT_VARIABLE SOURCE_REVISION
    )

    if (EXISTS "${DEPENDENCY_INSTALL_REVISION_FILE}")
        file(READ "${DEPENDENCY_INSTALL_REVISION_FILE}" INSTALLED_REVISION)
        if(SOURCE_REVISION STREQUAL INSTALLED_REVISION)
            message(STATUS "Found dependency ${DEPENDENCY}:${BUILD_CONFIG} already up to date at ${DEPENDENCY_BUILD_DIR}")
            return()
        else()
            message(STATUS "Dependency ${DEPENDENCY}:${BUILD_CONFIG} installed, but revision has changed. Rebuilding.")
        endif()
    else()
        message(STATUS "Dependency ${DEPENDENCY}:${BUILD_CONFIG} is not yet built (no install-revision file found).")
    endif()

    if (IS_HEADER_ONLY)
        message(STATUS "Copying header-only dependency ${DEPENDENCY} from ${MG_DEPENDENCIES_SOURCE_DIR}/${DEPENDENCY} to ${MG_DEPENDENCIES_INSTALL_DIR}")
        file(COPY "${MG_DEPENDENCIES_SOURCE_DIR}/${DEPENDENCY}" DESTINATION "${MG_DEPENDENCIES_INSTALL_DIR}")
    else()
        set(CONFIGURE_LOG_FILE "${DEPENDENCY_BUILD_DIR}/configure.log")
        set(BUILD_LOG_FILE "${DEPENDENCY_BUILD_DIR}/build.log")
        set(INSTALL_LOG_FILE "${DEPENDENCY_BUILD_DIR}/install.log")
        message(STATUS "Building ${DEPENDENCY} in configuration ${BUILD_CONFIG} with CMake parameters [${EXTRA_BUILD_PARAMS}]")
        message(STATUS "Build logs will be written to: ${CONFIGURE_LOG_FILE}, ${BUILD_LOG_FILE}, ${INSTALL_LOG_FILE}")

        # Configure
        execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPENDENCY_BUILD_DIR}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -S "${DEPENDENCY_SOURCE_DIR}" -B "${DEPENDENCY_BUILD_DIR}"
                "-DCMAKE_BUILD_TYPE=${BUILD_CONFIG}"
                "-DCMAKE_PREFIX_PATH=${MG_DEPENDENCIES_INSTALL_DIR}"
                "-DCMAKE_INSTALL_PREFIX=${MG_DEPENDENCIES_INSTALL_DIR}"
                ${EXTRA_BUILD_PARAMS}
            OUTPUT_FILE "${CONFIGURE_LOG_FILE}"
            ERROR_FILE "${CONFIGURE_LOG_FILE}"
            COMMAND_ERROR_IS_FATAL ANY
        )

        # Build
        execute_process(
            COMMAND "${CMAKE_COMMAND}" --build . -j ${NPROC} --config ${BUILD_CONFIG}
            WORKING_DIRECTORY ${DEPENDENCY_BUILD_DIR}
            OUTPUT_FILE "${BUILD_LOG_FILE}"
            ERROR_FILE "${BUILD_LOG_FILE}"
            COMMAND_ERROR_IS_FATAL ANY
        )

        # Install
        message(STATUS "Installing ${DEPENDENCY} in configuration ${BUILD_CONFIG} from ${DEPENDENCY_BUILD_DIR} to ${MG_DEPENDENCIES_INSTALL_DIR}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" --install . --config ${BUILD_CONFIG}
            WORKING_DIRECTORY ${DEPENDENCY_BUILD_DIR}
            OUTPUT_FILE "${INSTALL_LOG_FILE}"
            ERROR_FILE "${INSTALL_LOG_FILE}"
            COMMAND_ERROR_IS_FATAL ANY
        )
    endif()

    # Write revision file to build directory, so as to not build unnecessarily next time.
    file(WRITE "${DEPENDENCY_INSTALL_REVISION_FILE}" "${SOURCE_REVISION}")
endfunction()

# Build each dependency.
foreach(DEPENDENCY ${MG_DEPENDENCIES_TO_BUILD})
    build_dependency(${DEPENDENCY} "Debug" FALSE "${${DEPENDENCY}_EXTRA_BUILD_PARAMS}")
    build_dependency(${DEPENDENCY} "Release" FALSE "${${DEPENDENCY}_EXTRA_BUILD_PARAMS}")
endforeach()

# Prepare header-only dependencies
foreach(DEPENDENCY ${MG_HEADER_ONLY_DEPENDENCIES})
    build_dependency(${DEPENDENCY} "All" TRUE "")
endforeach()

message(STATUS "Finished building dependencies.")
