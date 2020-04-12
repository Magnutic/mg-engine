# The 'cmake --install' command was introduced in 3.15 and is used by this script.
cmake_minimum_required(VERSION 3.15)

# Build and install dependencies by invoking CMake externally on them. This ad-hoc approach may be
# an unusual method, but importantly, I can understand how it works and reason about what happens
# unlike with more advanced dependency-management frameworks.

if (NOT MG_BUILD_DEPENDENCIES_INSTALL_DIR)
    message(FATAL_ERROR "MG_BUILD_DEPENDENCIES is enabled, but MG_BUILD_DEPENDENCIES_INSTALL_DIR is not specified.")
endif()

# Get absolute path of install directory.
get_filename_component(DEPENDENCY_INSTALL_ROOT "${MG_BUILD_DEPENDENCIES_INSTALL_DIR}" ABSOLUTE BASE_DIR "${MG_BUILD_DIR}")

message("-- NOTE: building dependencies and installing to ${DEPENDENCY_INSTALL_ROOT}")

function(build_dependency DEPENDENCY BUILD_CONFIG)
    set(DEPENDENCY_SOURCE_DIR "${MG_DEPENDENCIES_SOURCE_DIR}/${DEPENDENCY}")
    set(DEPENDENCY_BUILD_DIR "${MG_DEPENDENCIES_BUILD_DIR}/${BUILD_CONFIG}/${DEPENDENCY}")

    message("  -- NOTE: building ${DEPENDENCY} in configuration ${BUILD_CONFIG}")

    # Configure
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPENDENCY_BUILD_DIR}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "${DEPENDENCY_SOURCE_DIR}"
            "-DCMAKE_BUILD_TYPE=${BUILD_CONFIG}"
            "-DCMAKE_PREFIX_PATH=${DEPENDENCY_INSTALL_ROOT}"
            "-DCMAKE_INSTALL_PREFIX=${DEPENDENCY_INSTALL_ROOT}"
            ${MG_DEPENDENCY_BUILD_EXTRA_PARAMS}
        WORKING_DIRECTORY ${DEPENDENCY_BUILD_DIR}
    )
    # Build
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build . -j 8 --config ${BUILD_CONFIG}
        WORKING_DIRECTORY ${DEPENDENCY_BUILD_DIR}
    )
    # Install
    message("  -- NOTE: installing ${DEPENDENCY} in configuration ${BUILD_CONFIG} from ${DEPENDENCY_BUILD_DIR} to ${DEPENDENCY_INSTALL_ROOT}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --install . --config ${BUILD_CONFIG}
        WORKING_DIRECTORY ${DEPENDENCY_BUILD_DIR}
    )
endfunction()

foreach(DEPENDENCY ${MG_DEPENDENCIES_TO_BUILD})
    build_dependency(${DEPENDENCY} "Debug")
    build_dependency(${DEPENDENCY} "Release")
endforeach()

# Add what we just installed to the prefix path, so that Mg can find the dependencies via
# find_package.
list(APPEND CMAKE_PREFIX_PATH "${DEPENDENCY_INSTALL_ROOT}")
