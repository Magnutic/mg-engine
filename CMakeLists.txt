cmake_minimum_required(VERSION 3.11)

project(mg_engine VERSION 0.1 LANGUAGES C CXX)

set(MG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(MG_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

#---------------------------------------------------------------------------------------------------
# Configurable options
#---------------------------------------------------------------------------------------------------

option(MG_BUILD_TESTS "Build Mg Engine's test suite." ON)
option(MG_BUILD_SAMPLES "Build Mg Engine sample applications." ON)
option(MG_BUILD_TOOLS "Build Mg Engine tools." ON)

# Sanitizers disabled by default to avoid compiler compatibility issues.
# (e.g. building the library with GCC then linking to an application built with Clang).
option(MG_DEBUG_SANITIZERS "Enable sanitizers (ASan, UBSan) for debug builds." OFF)
option(MG_ENABLE_GFX_DEBUG_GROUPS "Enable the use of OpenGL debug groups to ease understanding of API traces" OFF)

# Dependency-handling options.
option(MG_USE_VENDORED_DEPENDENCIES "Use dependencies vendored with Mg Engine" ON)
option(MG_USE_VENDORED_HEADER_ONLY_DEPENDENCIES "Use header-only dependencies vendored with Mg Engine" ON)
set(MG_DEPENDENCIES_DIR "${MG_SOURCE_DIR}/external/mg_dependencies" CACHE PATH "Where Mg Engine should search for its dependencies.")
set(MG_HEADER_ONLY_DEPENDENCIES_DIR "${MG_DEPENDENCIES_DIR}" CACHE PATH "Where to find header-only dependencies needed by Mg Engine")

#---------------------------------------------------------------------------------------------------
# Misc. definitions
#---------------------------------------------------------------------------------------------------

# The mg_dependencies_src.zip archive holds zipped source code of all dependencies. The idea is that one
# can bundle the archive to avoid relying on git submodules.
set(MG_DEPENDENCIES_ARCHIVE "${MG_SOURCE_DIR}/external/mg_dependencies_src.zip")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Add bundled CMake modules.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(MG_HEADER_INSTALL_PATH "${CMAKE_INSTALL_INCLUDEDIR}/mg_engine-${mg_engine_VERSION_MAJOR}")
set(MG_LIB_INSTALL_PATH "${CMAKE_INSTALL_LIBDIR}/mg_engine-${mg_engine_VERSION_MAJOR}")
set(MG_CONFIG_INSTALL_PATH "${CMAKE_INSTALL_LIBDIR}/cmake/mg_engine-${mg_engine_VERSION_MAJOR}")

#---------------------------------------------------------------------------------------------------
# Dependency management
#---------------------------------------------------------------------------------------------------

if(MG_USE_VENDORED_DEPENDENCIES OR MG_USE_VENDORED_HEADER_ONLY_DEPENDENCIES)
    include(cmake/get_dependencies.cmake)

    # Add to prefix path, so that find_package() calls will find the dependencies.
    list(APPEND CMAKE_PREFIX_PATH ${MG_DEPENDENCIES_DIR})
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
