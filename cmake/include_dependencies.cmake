# Include all dependencies needed to build Mg Engine.

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

message("-- Mg-Engine is looking for its dependencies:")
message("-- CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

# Prefer CMake config files to find modules in find_package. This is for example needed to prefer
# conan-generated config files to system packages found via CMake's bundled find modules.
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

# Include a header-only library.
# Params: LIBRARY: name of target to generate
# INCLUDE_DIR: include directory for target
function(add_private_header_only_library LIBRARY INCLUDE_DIR)
    message("-- Using ${LIBRARY} at ${INCLUDE_DIR}")
    add_library(${LIBRARY} INTERFACE)
    target_include_directories(${LIBRARY} SYSTEM INTERFACE
        $<BUILD_INTERFACE:${INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    # CMake will shout at us if we do not set up an install for this target, since mg_engine links
    # to this. So we have to set up an empty installation just to be allowed to build.
    install(TARGETS ${LIBRARY} EXPORT mg_engine_targets DESTINATION "${MG_LIB_INSTALL_PATH}")
endfunction()

# Include a header-only library.
# Params: LIBRARY: name of target to generate
# INCLUDE_DIR: include directory for target
# SUB_DIR_TO_INSTALL: subdirectory under INCLUDE_DIR to be installed. May be empty.
function(add_public_header_only_library LIBRARY INCLUDE_DIR SUB_DIR_TO_INSTALL)
    message("-- Using ${LIBRARY} at ${INCLUDE_DIR}")
    add_library(${LIBRARY} INTERFACE)
    target_include_directories(${LIBRARY} SYSTEM INTERFACE
        $<BUILD_INTERFACE:${INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    install(
        DIRECTORY ${INCLUDE_DIR}/${SUB_DIR_TO_INSTALL}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(TARGETS ${LIBRARY} EXPORT mg_engine_targets DESTINATION "${MG_LIB_INSTALL_PATH}")
endfunction()

# These mappings prevent CMake from linking debug libraries in RelWithDebInfo/MinSizeRel builds,
# which fails to compile under MSVC, and which would give misleading results if using RelWithDebInfo
# builds for profiling.
#
# Note that we map to empty string _and_ Release: this allows CMake to use IMPORTED targets that
# only set the generic IMPORTED_LOCATION property (as opposed to IMPORTED_LOCATION_<CONFIGURATION>
# for all configurations). See point 4 here: https://gitlab.kitware.com/cmake/cmake/-/issues/20440
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL "" Release)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "" Release)

#---------------------------------------------------------------------------------------------------

# GLAD
# OpenGL Loader-Generator based on the official specs.
# No attempt to find installed version -- GLAD is generated for each project, see
# http://glad.dav1d.de/
add_library(glad STATIC "${MG_SOURCE_DIR}/external/glad/src/glad.c")
target_include_directories(glad SYSTEM PUBLIC
    $<BUILD_INTERFACE:${MG_SOURCE_DIR}/external/glad/include>
    $<INSTALL_INTERFACE:${MG_HEADER_INSTALL_PATH}/glad/include>
)
set_target_properties(glad PROPERTIES DEBUG_POSTFIX _d)
install(TARGETS glad EXPORT mg_engine_targets DESTINATION "${MG_LIB_INSTALL_PATH}")
find_package(Threads REQUIRED)

# Libzip
# Zip-file handling library.
find_package(libzip 1.7.3 REQUIRED)

# fmt
# Text formatting
find_package(fmt 7.1 REQUIRED)

# GLFW
# Window and input library.
find_package(glfw3 3.3.8 REQUIRED)

# GLM
# GL Mathematics library.
find_package(glm 0.9.9 REQUIRED)
if(NOT TARGET glm::glm AND TARGET glm)
    set_target_properties(glm PROPERTIES IMPORTED_GLOBAL TRUE)
    add_library(glm::glm ALIAS glm)
endif()

# optional
# Implementation of std::optional with additional features.
find_package(tl-optional QUIET)
if (NOT tl-optional_FOUND)
    add_public_header_only_library(tl-optional "${MG_HEADER_ONLY_DEPENDENCIES_DIR}/optional/include" "tl")
    add_library(tl::optional ALIAS tl-optional)
endif()

# plf_colony
# Implementation of the colony data structure.
add_private_header_only_library(plf_colony "${MG_HEADER_ONLY_DEPENDENCIES_DIR}/plf_colony" "")

# function2
# Improved alternative to std::function.
# This dependency may be removed if equivalents of unique_function and function_view land in the
# standard library.
add_private_header_only_library(function2 "${MG_HEADER_ONLY_DEPENDENCIES_DIR}/function2/include" "")

# OpenAL-soft
# Sound library
find_package(OpenAL REQUIRED)

# libsndfile
# Sound file loading library
if (NOT TARGET SndFile::sndfile)
    find_package(SndFile REQUIRED)
endif()

# stb
# Sean Barrett's single-file libraries
add_private_header_only_library(stb "${MG_HEADER_ONLY_DEPENDENCIES_DIR}/stb" "")

# Bullet physics
# Physics and collision detection engine
find_package(Bullet MODULE REQUIRED)
add_library(bullet::bullet INTERFACE IMPORTED)
target_include_directories(bullet::bullet INTERFACE "${BULLET_INCLUDE_DIRS}")
target_link_libraries(bullet::bullet INTERFACE "${BULLET_LIBRARIES}")

# Imgui
# Immediate mode GUI
# Does not come with any build files, so we have to set up the target ourselves.
set(MG_IMGUI_DIR "${MG_HEADER_ONLY_DEPENDENCIES_DIR}/imgui")
message("-- Using imgui at ${MG_IMGUI_DIR}")
add_library(imgui STATIC 
    "${MG_IMGUI_DIR}/imgui.cpp"
    "${MG_IMGUI_DIR}/imgui_demo.cpp"
    "${MG_IMGUI_DIR}/imgui_draw.cpp"
    "${MG_IMGUI_DIR}/imgui_tables.cpp"
    "${MG_IMGUI_DIR}/imgui_widgets.cpp"
    "${MG_IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${MG_IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(imgui SYSTEM PUBLIC
    $<BUILD_INTERFACE:${MG_IMGUI_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(imgui PUBLIC glfw glad)
install(
    FILES "${MG_IMGUI_DIR}/imgui.h" "${MG_IMGUI_DIR}/imconfig.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(TARGETS imgui EXPORT mg_engine_targets DESTINATION "${MG_LIB_INSTALL_PATH}")

# utf8.h
# UTF-8 handling functions with API similar to string.h.
add_private_header_only_library(utf8.h "${MG_HEADER_ONLY_DEPENDENCIES_DIR}/utf8.h" "")

#---------------------------------------------------------------------------------------------------
# Dependencies for tools
#---------------------------------------------------------------------------------------------------

if (MG_BUILD_TOOLS)
    # AssImp
    # Asset importer for various 3D model formats.
    find_package(assimp REQUIRED)
endif()
