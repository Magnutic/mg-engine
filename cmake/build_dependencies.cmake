# The 'cmake --install' command was introduced in 3.15 and is used by this script.
cmake_minimum_required(VERSION 3.15)

include(ProcessorCount)
ProcessorCount(NPROC)

set(MG_DEPENDENCIES_BUILD_DIR "${MG_BUILD_DIR}/mg_dependencies/build")

# Build and install dependencies by invoking CMake externally on them. This ad-hoc approach may be
# an unusual method, but importantly, I can understand how it works and reason about what happens
# unlike with more advanced dependency-management frameworks.

if(NOT MG_BUILD_DEPENDENCIES_INSTALL_DIR)
    message(FATAL_ERROR "MG_BUILD_DEPENDENCIES is enabled, but MG_BUILD_DEPENDENCIES_INSTALL_DIR is not specified.")
endif()

# Get absolute path of install directory.
get_filename_component(DEPENDENCY_INSTALL_ROOT "${MG_BUILD_DEPENDENCIES_INSTALL_DIR}" ABSOLUTE BASE_DIR "${MG_BUILD_DIR}")

message("-- NOTE: building dependencies and installing to ${DEPENDENCY_INSTALL_ROOT}")

function(build_dependency DEPENDENCY BUILD_CONFIG)
    set(DEPENDENCY_SOURCE_DIR "${MG_DEPENDENCIES_SOURCE_DIR}/${DEPENDENCY}")
    set(DEPENDENCY_BUILD_DIR "${MG_DEPENDENCIES_BUILD_DIR}/${BUILD_CONFIG}/${DEPENDENCY}")

    message("  -- NOTE: building ${DEPENDENCY} in configuration ${BUILD_CONFIG} with CMake parameters [${${DEPENDENCY}_EXTRA_BUILD_PARAMS}]")

    # Configure
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPENDENCY_BUILD_DIR}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "${DEPENDENCY_SOURCE_DIR}"
            "-DCMAKE_BUILD_TYPE=${BUILD_CONFIG}"
            "-DCMAKE_PREFIX_PATH=${DEPENDENCY_INSTALL_ROOT}"
            "-DCMAKE_INSTALL_PREFIX=${DEPENDENCY_INSTALL_ROOT}"
            ${${DEPENDENCY}_EXTRA_BUILD_PARAMS}
        WORKING_DIRECTORY ${DEPENDENCY_BUILD_DIR}
        RESULT_VARIABLE CONFIGURE_RESULT
    )
    if(NOT CONFIGURE_RESULT EQUAL 0)
        message(FATAL_ERROR "Error configuring dependency ${DEPENDENCY}")
    endif()

    # Build
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build . -j ${NPROC} --config ${BUILD_CONFIG}
        WORKING_DIRECTORY ${DEPENDENCY_BUILD_DIR}
        RESULT_VARIABLE BUILD_RESULT
    )
    if(NOT BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "Error building dependency ${DEPENDENCY}")
    endif()
    # Install
    message("  -- NOTE: installing ${DEPENDENCY} in configuration ${BUILD_CONFIG} from ${DEPENDENCY_BUILD_DIR} to ${DEPENDENCY_INSTALL_ROOT}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --install . --config ${BUILD_CONFIG}
        WORKING_DIRECTORY ${DEPENDENCY_BUILD_DIR}
        RESULT_VARIABLE INSTALL_RESULT
    )
    if(NOT INSTALL_RESULT EQUAL 0)
        message(FATAL_ERROR "Error installing dependency ${DEPENDENCY}")
    endif()
endfunction()

# The list of dependencies. This corresponds to submodules in external/submodules.
# (They have to be explicitly listed since order matters due to dependencies between them.)
list(APPEND MG_DEPENDENCIES zlib libzip fmt glfw glm function2 plf_colony optional openal-soft stb)

# Dependencies that will be built and locally installed using CMake.
list(APPEND MG_DEPENDENCIES_TO_BUILD zlib libzip fmt glfw glm openal-soft)

# If building tools, we need some more dependencies.
if(MG_BUILD_TOOLS)
    list(APPEND MG_DEPENDENCIES assimp)
    list(APPEND MG_DEPENDENCIES_TO_BUILD assimp)
endif()

# Only build bundled libsndfile if system libsndfile is not available -- prefer system version since
# it otherwise causes conflicts with system provided library.
find_package(SndFile QUIET)
if(NOT SndFile_FOUND)
    message("-- NOTE: System libsndfile not found. Building bundled libsndfile instead.")
    list(APPEND MG_DEPENDENCIES ogg vorbis libsndfile)
    list(APPEND MG_DEPENDENCIES_TO_BUILD ogg vorbis libsndfile)
endif()

# Some extra params passed into each build-configuration.
set(fmt_EXTRA_BUILD_PARAMS "-DFMT_TEST=0")
set(glfw_EXTRA_BUILD_PARAMS "-DGLFW_BUILD_EXAMPLES=0" "-DGLFW_BUILD_TESTS=0")
set(glm_EXTRA_BUILD_PARAMS "-DGLM_TEST_ENABLE=0")
set(libzip_EXTRA_BUILD_PARAMS "-DBUILD_TOOLS=0" "-DBUILD_REGRESS=0" "-DBUILD_EXAMPLES=0")
set(libsndfile_EXTRA_BUILD_PARAMS "-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=1" "-DBUILD_SHARED_LIBS=1" "-DBUILD_PROGRAMS=0" "-DBUILD_EXAMPLES=0" "-DBUILD_TESTING=0")
set(assimp_EXTRA_BUILD_PARAMS "-DASSIMP_BUILD_ASSIMP_TOOLS=0" "-DASSIMP_BUILD_TESTS=0")
set(openal-soft_EXTRA_BUILD_PARAMS "-DALSOFT_UTILS=0" "-DALSOFT_EXAMPLES=0" "-DALSOFT_TESTS=0")

# Build each dependency.
foreach(DEPENDENCY ${MG_DEPENDENCIES_TO_BUILD})
    build_dependency(${DEPENDENCY} "Debug")
    build_dependency(${DEPENDENCY} "Release")
endforeach()

# Store a hash of the original dependencies archive so we can detect if the archive has changed
# since build.
file(SHA1 "${MG_DEPENDENCIES_ARCHIVE}" MG_DEPENDENCIES_ZIP_HASH)
file(WRITE "${DEPENDENCY_INSTALL_ROOT}/.mg_dependencies_zip_hash" ${MG_DEPENDENCIES_ZIP_HASH})

# Add what we just installed to the prefix path, so that Mg can find the dependencies via
# find_package.
list(APPEND CMAKE_PREFIX_PATH "${DEPENDENCY_INSTALL_ROOT}")
