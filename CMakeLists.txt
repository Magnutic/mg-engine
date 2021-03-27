cmake_minimum_required(VERSION 3.11)

project(mg_engine VERSION 0.1 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(MG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(MG_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(MG_HEADER_INSTALL_PATH "${CMAKE_INSTALL_INCLUDEDIR}/mg_engine-${mg_engine_VERSION_MAJOR}")
set(MG_LIB_INSTALL_PATH "${CMAKE_INSTALL_LIBDIR}/mg_engine-${mg_engine_VERSION_MAJOR}")
set(MG_CONFIG_INSTALL_PATH "${CMAKE_INSTALL_LIBDIR}/cmake/mg_engine-${mg_engine_VERSION_MAJOR}")

option(MG_BUILD_TESTS "Build Mg Engine's test suite." ON)
option(MG_BUILD_SAMPLES "Build Mg Engine sample applications." ON)
option(MG_BUILD_TOOLS "Build Mg Engine tools." ON)

# Sanitisers disabled by default to avoid compiler compatibility issues.
# (e.g. building the library with GCC then linking to an application built with Clang).
option(MG_DEBUG_SANITISERS "Enable sanitisers (ASan, UBSan) for debug builds." OFF)

option(MG_ENABLE_GFX_DEBUG_GROUPS "Enable the use of OpenGL debug groups to ease understanding of API traces" OFF)

# We want all executables in one, predictable location,
# since they use relative paths to find resources.
set(OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}/../libs/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}../libs/")

#---------------------------------------------------------------------------------------------------
# Dependency management
#---------------------------------------------------------------------------------------------------

# Add bundled CMake modules.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
option(MG_BUILD_DEPENDENCIES "Build and (locally) install Mg Engine's dependencies." OFF)
set(MG_BUILD_DEPENDENCIES_INSTALL_DIR "${MG_BUILD_DIR}/external" CACHE STRING "Where to install dependencies built when MG_BUILD_DEPENDENCIES is enabled.")

set(MG_DEPENDENCIES_ARCHIVE "${MG_SOURCE_DIR}/external/mg_dependencies.zip")
set(MG_DEPENDENCIES_SOURCE_DIR "${MG_SOURCE_DIR}/external/mg_dependencies")

# Trigger reconfiguration when dependencies archive changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${MG_DEPENDENCIES_ARCHIVE})

# Extract dependency archive. Even if we do not build the bundled dependencies, there are some
# header-only libraries within that we need.
file(REMOVE_RECURSE "${MG_DEPENDENCIES_SOURCE_DIR}")
file(MAKE_DIRECTORY "${MG_DEPENDENCIES_SOURCE_DIR}")
execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xf "${MG_DEPENDENCIES_ARCHIVE}" --format=zip
    WORKING_DIRECTORY "${MG_DEPENDENCIES_SOURCE_DIR}"
)

# Build and install dependencies if requested (off by default).
if(MG_BUILD_DEPENDENCIES)
    # Perform the build.
    include(cmake/build_dependencies.cmake)
endif()

# Set up all needed targets for dependencies.
include(cmake/include_dependencies.cmake)

#---------------------------------------------------------------------------------------------------

if(MG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(MG_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

if(MG_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

add_subdirectory(src)

# Installation.
write_basic_package_version_file(mg_engine-config-version.cmake
    VERSION ${mg_engine_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(EXPORT mg_engine_targets NAMESPACE mg_engine DESTINATION "${MG_CONFIG_INSTALL_PATH}")

install(
    FILES
        cmake/mg_engine-config.cmake
        "${CMAKE_CURRENT_BINARY_DIR}/mg_engine-config-version.cmake"
    DESTINATION
        "${MG_CONFIG_INSTALL_PATH}"
)
