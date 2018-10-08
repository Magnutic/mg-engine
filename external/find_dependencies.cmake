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

# Include a header only library.
# Params: LIBRARY: name of target to generate
# INCLUDE_DIR: include directory for target
# SUB_DIR_TO_INSTALL: subdirectory under INCLUDE_DIR to be installed. May be empty.
function(add_header_only_library LIBRARY INCLUDE_DIR SUB_DIR_TO_INSTALL)
    add_library(${LIBRARY} INTERFACE)

    target_include_directories(${LIBRARY} INTERFACE
        $<BUILD_INTERFACE:${INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${MG_HEADER_INSTALL_PATH}/external>
    )

    install(
        DIRECTORY ${INCLUDE_DIR}/${SUB_DIR_TO_INSTALL}
        DESTINATION ${MG_HEADER_INSTALL_PATH}/external
    )
endfunction()

####################################################################################################
# Zlib
# Compression / decompression library, dependency of libzip.

find_package(ZLIB 1.1.2 QUIET)

if (NOT ZLIB_FOUND)
    use_bundled_library(zlib)
endif()

####################################################################################################
# Libzip
# Zip-file handling library.

find_package(libzip 1.2 QUIET)

if (NOT LIBZIP_FOUND)
    use_bundled_library(libzip)
endif()

####################################################################################################
# GLAD
# OpenGL Loader-Generator based on the official specs.

# No attempt to find installed version -- GLAD is generated for each project, see
# http://glad.dav1d.de/

add_library(glad ${CMAKE_CURRENT_LIST_DIR}/glad/src/glad.c)

target_include_directories(glad PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/glad/include>
    $<INSTALL_INTERFACE:${MG_HEADER_INSTALL_PATH}/glad/include>
)

install(TARGETS glad EXPORT mg_engine DESTINATION ${MG_LIB_INSTALL_PATH})

####################################################################################################
# GLFW
# Window and input library.
# GLFW provides an excellent CMakeLists.txt - just include and use, as it should be.

find_package(glfw3 3.2)

if (NOT glfw3_FOUND)
    init_library_submodule(glfw)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/glfw)
endif()

####################################################################################################
# GLM
# GL Mathematics library.

find_path(GLM_INCLUDE_DIR glm/glm.hpp)

if ((NOT GLM_INCLUDE_DIR) OR (NOT EXISTS ${GLM_INCLUDE_DIR}))
    init_library_submodule(glm)
    set(GLM_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/glm)
    add_header_only_library(glm ${GLM_INCLUDE_DIR} glm)
else()
    add_library(glm INTERFACE IMPORTED)
    set_target_properties(glm PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${GLM_INCLUDE_DIR})
endif()

install(TARGETS glm EXPORT mg_engine DESTINATION ${MG_LIB_INSTALL_PATH})
