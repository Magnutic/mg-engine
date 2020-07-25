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

option(MG_GENERATE_DEPENDENCY_ARCHIVE "Generate mg_dependencies.zip from submodules (not usually needed, mg_dependencies.zip is bundled)." OFF)
option(MG_BUILD_DEPENDENCIES "Build and (locally) install Mg Engine's dependencies." OFF)
set(MG_BUILD_DEPENDENCIES_INSTALL_DIR "${MG_BUILD_DIR}/external" CACHE STRING "Where to install dependencies built when MG_BUILD_DEPENDENCIES is enabled.")

# The list of dependencies. This corresponds to submodules in external/submodules.
# (They have to be explicitly listed since order matters due to dependencies between them.)
list(APPEND MG_DEPENDENCIES zlib libzip fmt glfw glm function2 plf_colony optional openal-soft)

# Dependencies that will be built and installed using CMake.
list(APPEND MG_DEPENDENCIES_TO_BUILD zlib libzip fmt glfw glm openal-soft)

# Only build bundled libsndfile if system libsndfile is not available -- prefer system version since
# it otherwise causes conflicts with system provided library.
find_package(SndFile QUIET)
if (NOT SndFile_FOUND)
    list(APPEND MG_DEPENDENCIES ogg vorbis libsndfile)
    list(APPEND MG_DEPENDENCIES_TO_BUILD ogg vorbis libsndfile)
endif()

set(MG_DEPENDENCIES_SOURCE_DIR "${MG_SOURCE_DIR}/external/mg_dependencies")
set(MG_DEPENDENCIES_BUILD_DIR "${MG_BUILD_DIR}/mg_dependencies/build")

# Optionally, generate mg_dependencies.zip from submodules.
# (Off by default; mg_dependencies.zip is intended to be bundled -- but this is how it is generated.)
if(MG_GENERATE_DEPENDENCY_ARCHIVE)
    include(cmake/generate_dependency_archive.cmake)
endif()

# Extract dependency archive. Even if we do not build the bundled dependencies, there are some
# header-only libraries within that we need.
file(MAKE_DIRECTORY "${MG_DEPENDENCIES_SOURCE_DIR}")
file(SHA1 "${MG_SOURCE_DIR}/external/mg_dependencies.zip" MG_DEPENDENCY_ZIP_HASH)

if(EXISTS "${MG_DEPENDENCIES_SOURCE_DIR}/zip_hash")
    file(READ "${MG_DEPENDENCIES_SOURCE_DIR}/zip_hash" MG_DEPENDENCY_EXTRACTED_HASH)
endif()

if(MG_DEPENDENCY_EXTRACTED_HASH STREQUAL MG_DEPENDENCY_ZIP_HASH)
    message("-- NOTE: using already extracted dependencies at ${MG_DEPENDENCIES_SOURCE_DIR}")
else()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xf "${MG_SOURCE_DIR}/external/mg_dependencies.zip" --format=zip
        WORKING_DIRECTORY "${MG_DEPENDENCIES_SOURCE_DIR}"
    )
    file(WRITE "${MG_DEPENDENCIES_SOURCE_DIR}/zip_hash" "${MG_DEPENDENCIES_SOURCE_DIR}/zip_hash")
endif()

# Build and install dependencies if requested (off by default).
if(MG_BUILD_DEPENDENCIES)
    # Some extra params passed into each build-configuration.
    set(fmt_EXTRA_BUILD_PARAMS "-DFMT_TEST=0")
    set(glfw_EXTRA_BUILD_PARAMS "-DGLFW_BUILD_EXAMPLES=0" "-DGLFW_BUILD_TESTS=0")
    set(glm_EXTRA_BUILD_PARAMS "-DGLM_TEST_ENABLE=0")
    set(libzip_EXTRA_BUILD_PARAMS "-DBUILD_TOOLS=0" "-DBUILD_REGRESS=0" "-DBUILD_EXAMPLES=0")
    set(libsndfile_EXTRA_BUILD_PARAMS "-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=1" "-DBUILD_SHARED_LIBS=1" "-DBUILD_PROGRAMS=0" "-DBUILD_EXAMPLES=0" "-DBUILD_TESTING=0")
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
