# The 'cmake --install' command was introduced in 3.15 and is used by this script.
cmake_minimum_required(VERSION 3.15)

include(ProcessorCount)
ProcessorCount(NPROC)

# The header-only dependencies. These names correspond to submodules in external/submodules.
list(APPEND MG_HEADER_ONLY_DEPENDENCIES function2 plf_colony optional stb imgui utf8.h)

if (MG_USE_VENDORED_DEPENDENCIES)
    # List of dependencies to build. These names corresponds to submodules in external/submodules.
    # They have to be explicitly listed since order matters due to dependencies between them.
    list(APPEND MG_DEPENDENCIES_TO_BUILD zlib libzip fmt glfw glm openal-soft assimp bullet3)

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
    set(bullet3_EXTRA_BUILD_PARAMS "-DBUILD_CPU_DEMOS=0" "-DUSE_GLUT=0" "-DBUILD_ENET=0"
        "-DBUILD_CLSOCKET=0" "-DBUILD_OPENGL3_DEMOS=0" "-DBUILD_BULLET2_DEMOS=0" "-DBUILD_EXTRAS=0"
        "-DBUILD_UNIT_TESTS=0")

    # Only build bundled libsndfile if system libsndfile is not available -- prefer system version since
    # it otherwise causes conflicts with system-provided library.
    find_package(SndFile QUIET)
    if(NOT SndFile_FOUND)
        message("----- NOTE: System libsndfile not found. Building bundled libsndfile instead. -----")
        list(APPEND MG_DEPENDENCIES_TO_BUILD ogg vorbis libsndfile)
    endif()
endif()

get_filename_component(MG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
set(MG_DEPENDENCIES_ARCHIVE "${MG_SOURCE_DIR}/external/mg_dependencies_src.zip")
set(MG_DEPENDENCIES_SOURCE_DIR "${MG_SOURCE_DIR}/external/mg_dependencies_src")
set(MG_DEPENDENCIES_BUILD_DIR "${MG_SOURCE_DIR}/external/build")
set(MG_DEPENDENCIES_INSTALL_DIR "${MG_SOURCE_DIR}/external/mg_dependencies")

# Get dependencies, either from archive or from submodules.
if(NOT EXISTS "${MG_DEPENDENCIES_SOURCE_DIR}")
    message("----- NOTE: Getting dependencies -----")

    # Use bundled dependencies archive if instructed to.
    if (${MG_USE_VENDORED_DEPENDENCIES_ARCHIVE})
        if(EXISTS "${MG_DEPENDENCIES_ARCHIVE}")
            file(MAKE_DIRECTORY "${MG_DEPENDENCIES_SOURCE_DIR}")
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
            message(FATAL_ERROR "MG_USE_VENDORED_DEPENDENCIES_ARCHIVE is enabled but there is no archive at ${MG_DEPENDENCIES_ARCHIVE}")
        endif()

    else()

        # No dependency archive. Use submodules instead. Note that we set MG_DEPENDENCIES_SOURCE_DIR
        # to point to the submodules directory instead.
        message("${MG_DEPENDENCIES_ARCHIVE} does not exist. Using git submodules to get header-only dependencies...")
        include("${MG_SOURCE_DIR}/cmake/init_submodules.cmake")
        set(MG_DEPENDENCIES_SOURCE_DIR "${MG_SOURCE_DIR}/external/submodules")

    endif()
endif()

# If using vendored dependencies, build those.
if (MG_USE_VENDORED_DEPENDENCIES)
    message("----- NOTE: building dependencies -----")

    if (NOT IS_DIRECTORY "${MG_DEPENDENCIES_BUILD_DIR}")
        file(MAKE_DIRECTORY "${MG_DEPENDENCIES_BUILD_DIR}")
    endif()

    if (NOT IS_DIRECTORY "${MG_DEPENDENCIES_INSTALL_DIR}")
        file(MAKE_DIRECTORY "${MG_DEPENDENCIES_INSTALL_DIR}")
    endif()

    # Build and install dependencies by invoking CMake externally on them. This ad-hoc approach may be
    # an unusual method, but importantly, I can understand how it works and reason about what happens
    # unlike with more advanced dependency-management frameworks.

    # Get absolute path of install directory.
    message("----- NOTE: building dependencies and installing to ${MG_DEPENDENCIES_INSTALL_DIR} ----")

    function(build_dependency DEPENDENCY BUILD_CONFIG)
        set(DEPENDENCY_SOURCE_DIR "${MG_DEPENDENCIES_SOURCE_DIR}/${DEPENDENCY}")
        set(DEPENDENCY_BUILD_DIR "${MG_DEPENDENCIES_BUILD_DIR}/${BUILD_CONFIG}/${DEPENDENCY}")
        set(DEPENDENCY_SOURCE_REVISION_FILE "${MG_DEPENDENCIES_SOURCE_DIR}/${DEPENDENCY}_revision")
        set(DEPENDENCY_INSTALL_REVISION_FILE "${MG_DEPENDENCIES_INSTALL_DIR}/${DEPENDENCY}_revision_${BUILD_CONFIG}")

        if (EXISTS "${DEPENDENCY_SOURCE_REVISION_FILE}" AND EXISTS "${DEPENDENCY_INSTALL_REVISION_FILE}")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E compare_files
                "${DEPENDENCY_INSTALL_REVISION_FILE}"
                "${DEPENDENCY_SOURCE_REVISION_FILE}"
                RESULT_VARIABLE REVISIONS_EQUAL
            )
            if(REVISIONS_EQUAL EQUAL 0)
                message("Found dependency ${DEPENDENCY} already up-to-date for configuration ${BUILD_CONFIG} at ${DEPENDENCY_BUILD_DIR}")
                return()
            endif()
        endif()

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
        else()
            # Copy revision file to build directory, so as to not build unnecessarily next time.
            if (EXISTS "${DEPENDENCY_SOURCE_REVISION_FILE}")
                execute_process(COMMAND "${CMAKE_COMMAND}" -E copy "${DEPENDENCY_SOURCE_REVISION_FILE}" "${DEPENDENCY_INSTALL_REVISION_FILE}")
            endif()
        endif()
    endfunction()

    # Build each dependency.
    foreach(DEPENDENCY ${MG_DEPENDENCIES_TO_BUILD})
        build_dependency(${DEPENDENCY} "Debug")
        build_dependency(${DEPENDENCY} "Release")
    endforeach()

    message("Finished building dependencies.")
endif()

# Prepare header-only dependencies
foreach(DEPENDENCY ${MG_HEADER_ONLY_DEPENDENCIES})
    message("-- Copying header-only depencency ${DEPENDENCY} from ${MG_DEPENDENCIES_SOURCE_DIR}/${DEPENDENCY} to ${MG_DEPENDENCIES_INSTALL_DIR}")
    file(COPY "${MG_DEPENDENCIES_SOURCE_DIR}/${DEPENDENCY}" DESTINATION "${MG_DEPENDENCIES_INSTALL_DIR}")
endforeach()

