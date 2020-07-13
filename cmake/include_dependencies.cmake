# Include all dependencies needed to build Mg Engine.

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Prefer CMake config files to find modules in find_package. This is for example needed to prefer
# conan-generated config files to system packages found via CMake's bundled find modules.
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

# Include a header-only library.
# Params: LIBRARY: name of target to generate
# INCLUDE_DIR: include directory for target
function(add_private_header_only_library LIBRARY INCLUDE_DIR)
    add_library(${LIBRARY} INTERFACE)
    # CMake will shout at us if we do not set up an install for this target, since mg_engine links
    # to this. So we have to set up an empty installation just to be allowed to build.
    target_include_directories(${LIBRARY} INTERFACE
        $<BUILD_INTERFACE:${INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    install(TARGETS ${LIBRARY} EXPORT mg_engine_targets DESTINATION "${MG_LIB_INSTALL_PATH}")
endfunction()

# Include a header-only library.
# Params: LIBRARY: name of target to generate
# INCLUDE_DIR: include directory for target
# SUB_DIR_TO_INSTALL: subdirectory under INCLUDE_DIR to be installed. May be empty.
function(add_public_header_only_library LIBRARY INCLUDE_DIR SUB_DIR_TO_INSTALL)
    add_library(${LIBRARY} INTERFACE)
    target_include_directories(${LIBRARY} INTERFACE
        $<BUILD_INTERFACE:${INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    install(
        DIRECTORY ${INCLUDE_DIR}/${SUB_DIR_TO_INSTALL}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(TARGETS ${LIBRARY} EXPORT mg_engine_targets DESTINATION "${MG_LIB_INSTALL_PATH}")
endfunction()

# These functions prevent CMake from linking debug libraries to RelWithDebInfo/MinSizeRel builds,
# which fails to compile under MSVC.
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Release)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release) 

#---------------------------------------------------------------------------------------------------

# GLAD
# OpenGL Loader-Generator based on the official specs.
# No attempt to find installed version -- GLAD is generated for each project, see
# http://glad.dav1d.de/
add_library(glad STATIC "${MG_SOURCE_DIR}/external/glad/src/glad.c")
target_include_directories(glad PUBLIC
    $<BUILD_INTERFACE:${MG_SOURCE_DIR}/external/glad/include>
    $<INSTALL_INTERFACE:${MG_HEADER_INSTALL_PATH}/glad/include>
)
set_target_properties(glad PROPERTIES DEBUG_POSTFIX _d)
install(TARGETS glad EXPORT mg_engine_targets DESTINATION "${MG_LIB_INSTALL_PATH}")
find_package(Threads REQUIRED)

# Libzip
# Zip-file handling library.
find_package(libzip 1.6.1 REQUIRED)

# fmt
# Text formatting
find_package(fmt 6.0 REQUIRED)

# GLFW
# Window and input library.
find_package(glfw3 3.3 REQUIRED)

# GLM
# GL Mathematics library.
find_package(glm 0.9.9 REQUIRED)
if(NOT TARGET glm::glm)
    # I have come across GLM configurations that
    #  - define only target glm, or
    #  - define both targets glm and glm::glm, the latter an alias of the former, or
    #  - define only target glm::glm.
    # So there is no universal name for the glm target. As a work-around, we make sure glm::glm
    # exists.
    set_target_properties(glm PROPERTIES IMPORTED_GLOBAL TRUE)
    add_library(glm::glm ALIAS glm)
endif()

# optional
# Implementation of std::optional with additional features.
find_package(tl-optional QUIET)
if (NOT tl-optional_FOUND)
    add_public_header_only_library(tl-optional "${MG_DEPENDENCIES_SOURCE_DIR}/optional/include" "tl")
    add_library(tl::optional ALIAS tl-optional)
endif()

# plf_colony
# Implementation of the colony data structure.
add_private_header_only_library(plf_colony "${MG_DEPENDENCIES_SOURCE_DIR}/plf_colony" "")

# function2
# Improved alternative to std::function.
# This dependency may be removed if equivalents of unique_function and function_view land in the
# standard library.
add_private_header_only_library(function2 "${MG_DEPENDENCIES_SOURCE_DIR}/function2/include" "")

# OpenAL-soft
# Sound library
find_package(OpenAL REQUIRED)
