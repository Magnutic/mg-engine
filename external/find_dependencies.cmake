# Find dependencies for Mg Engine

cmake_minimum_required(VERSION 3.1)

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR})

find_package(Threads REQUIRED)

####################################################################################################
# Helper functions
####################################################################################################

# Download library submodule. LIBRARY: path to submodule.
function(init_library_submodule LIBRARY)
    message("-- NOTE: ${LIBRARY} not found, using sub-repo...")
    execute_process(
        COMMAND git submodule update --init -- ${LIBRARY}
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    )
endfunction()

# Use a bundled CMake library. LIBRARY: library subdirectory containing CMakeLists.txt.
function(use_bundled_library LIBRARY)
    message("-- NOTE: Falling back to bundled version of ${LIBRARY}...")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/${LIBRARY})
endfunction()

# Include a header-only library.
# Params: LIBRARY: name of target to generate
# INCLUDE_DIR: include directory for target
# SUB_DIR_TO_INSTALL: subdirectory under INCLUDE_DIR to be installed. May be empty.
function(add_header_only_library LIBRARY INCLUDE_DIR SUB_DIR_TO_INSTALL)
    add_library(${LIBRARY} INTERFACE)

    target_include_directories(${LIBRARY} INTERFACE
        $<BUILD_INTERFACE:${INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )

    install(
        DIRECTORY ${INCLUDE_DIR}/${SUB_DIR_TO_INSTALL}
        DESTINATION "include/"
    )
endfunction()

####################################################################################################
# Libzip
# Zip-file handling library.

find_package(libzip 1.2 QUIET)

if (NOT LIBZIP_FOUND)
    # Require CMake >= 3.13
    # The bundled libzip version does not create an export set for its 'zip' target, which makes
    # CMake throw an error when I try to export mg_engine which has a dependency on libzip.
    # This can be fixed by using `install(TARGET zip EXPORT ...)` here, but before CMake 3.13 it was
    # not possible to use `install(TARGET)` with targets defined in a different directory.
    if(${CMAKE_VERSION} VERSION_LESS "3.13.0")
        message(FATAL_ERROR
"Libzip not found and CMake version is below 3.13 -- for convoluted
reasons, using the bundled copy of libzip does not work with earlier versions of CMake.
Please either provide an installed copy of libzip under CMAKE_PREFIX_PATH, or use a newer
version of CMake.")
    endif()

    # libzip depends on zlib
    find_package(ZLIB 1.1.2 QUIET)

    if (NOT ZLIB_FOUND)
        use_bundled_library(zlib)
    endif()

    init_library_submodule(libzip)

    # Mg Engine does not require support for encrypted zip-files.
    option(ENABLE_COMMONCRYPTO "" OFF)
    option(ENABLE_GNUTLS "" OFF)
    option(ENABLE_MBEDTLS "" OFF)
    option(ENABLE_OPENSSL "" OFF)
    option(ENABLE_WINDOWS_CRYPTO "" OFF)
    option(ENABLE_BZIP2 "" OFF)

    option(BUILD_TOOLS "" OFF)
    option(BUILD_REGRESS "" OFF)
    option(BUILD_EXAMPLES "" OFF)
    option(BUILD_DOC "" OFF)

    use_bundled_library(libzip)

    # Set up zip target to export properly
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        libzip-config-version.cmake
        VERSION "1.3" # This is bound to end up wrong but I see no convenient way to automate.
        COMPATIBILITY AnyNewerVersion
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libzip-config-version.cmake DESTINATION "lib/cmake/libzip/")

    install(TARGETS zip EXPORT zip_targets DESTINATION "lib/") # Only works on CMake >= 3.13
    install(EXPORT zip_targets FILE libzip-config.cmake DESTINATION "lib/cmake/libzip/")
else()
    # Install the Findlibzip module so that dependents can use it.
    install(FILES ${CMAKE_CURRENT_LIST_DIR}/../cmake/Findlibzip.cmake DESTINATION "lib/cmake/libzip/")
endif()

####################################################################################################
# GLAD
# OpenGL Loader-Generator based on the official specs.

# No attempt to find installed version -- GLAD is generated for each project, see
# http://glad.dav1d.de/

add_library(glad STATIC ${CMAKE_CURRENT_LIST_DIR}/glad/src/glad.c)

target_include_directories(glad PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/glad/include>
    $<INSTALL_INTERFACE:${MG_HEADER_INSTALL_PATH}/glad/include>
)

install(TARGETS glad EXPORT mg_engine_targets DESTINATION "${MG_LIB_INSTALL_PATH}")

####################################################################################################
# GLFW
# Window and input library.

find_package(glfw3 3.2 QUIET)

if (NOT glfw3_FOUND)
    init_library_submodule(glfw)
    option(GLFW_BUILD_EXAMPLES "" OFF)
    option(GLFW_BUILD_TESTS "" OFF)
    option(GLFW_BUILD_DOCS "" OFF)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/glfw)
endif()

####################################################################################################
# GLM
# GL Mathematics library.

find_package(glm 0.9.9 QUIET)

if (NOT glm_FOUND)
    init_library_submodule(glm)
    option(GLM_TEST_ENABLE "" OFF)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/glm)
endif()
