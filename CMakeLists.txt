cmake_minimum_required(VERSION 3.8.2)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

project(MG_ENGINE VERSION 0.1 LANGUAGES C CXX)

# Add bundled CMake modules.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(MG_HEADER_INSTALL_PATH "${CMAKE_INSTALL_INCLUDEDIR}/mg_engine-${MG_ENGINE_VERSION}")
set(MG_LIB_INSTALL_PATH "${CMAKE_INSTALL_LIBDIR}/mg_engine-${MG_ENGINE_VERSION}")
set(MG_CONFIG_INSTALL_PATH "${CMAKE_INSTALL_LIBDIR}/cmake/mg_engine-${MG_ENGINE_VERSION}")

option(MG_BUILD_TESTS "Build Mg Engine's test suite" ON)
option(MG_BUILD_SAMPLES "Build Mg Engine sample applications" ON)

# Sanitisers disabled by default to avoid compiler compatibility issues.
# (e.g. building the library with GCC then linking to an application built with Clang).
option(MG_DEBUG_SANITISERS "Enable sanitisers (ASan, UBSan) for debug builds." OFF)

# We want all executables in one, predictable location,
# since they use relative paths to find resources.
set(OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}/../libs/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}../libs/")

# Include external dependencies.
include(external/find_dependencies.cmake)

if(MG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(MG_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

add_subdirectory(src)

# Installation.
write_basic_package_version_file(mg_engine-config-version.cmake
    VERSION ${MG_ENGINE_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(EXPORT mg_engine_targets DESTINATION "${MG_CONFIG_INSTALL_PATH}")

install(
    FILES
        cmake/mg_engine-config.cmake
        "${CMAKE_CURRENT_BINARY_DIR}/mg_engine-config-version.cmake"
    DESTINATION
        "${MG_CONFIG_INSTALL_PATH}"
)
