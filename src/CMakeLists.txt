cmake_minimum_required(VERSION 3.11)

set(MG_HEADER_DIR "${MG_SOURCE_DIR}/include/")

# Create mg_engine library
file(GLOB_RECURSE MG_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(mg_engine ${MG_SOURCES})

# Compile using standard C++20
target_compile_features(mg_engine PUBLIC cxx_std_20)
set_target_properties(mg_engine PROPERTIES CXX_EXTENSIONS OFF)

# Put resulting libraries under libs/
set(MG_LIBRARY_OUTPUT_DIR "${CMAKE_BINARY_DIR}/libs/")
set_target_properties(mg_engine PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${MG_LIBRARY_OUTPUT_DIR}")
set_target_properties(mg_engine PROPERTIES LIBRARY_OUTPUT_DIRECTORY_DEBUG "${MG_LIBRARY_OUTPUT_DIR}")
set_target_properties(mg_engine PROPERTIES LIBRARY_OUTPUT_DIRECTORY_RELEASE "${MG_LIBRARY_OUTPUT_DIR}")

# Enable debug-logging for debug builds.
target_compile_definitions(mg_engine PUBLIC MG_ENABLE_DEBUG_LOGGING=$<CONFIG:Debug>)

if(MG_ENABLE_GFX_DEBUG_GROUPS)
    target_compile_definitions(mg_engine PRIVATE MG_ENABLE_GFX_DEBUG_GROUPS=1)
endif()

# Set up compiler options for MSVC
if(MSVC)
    if (${MSVC_VERSION} LESS 1914)
         message(FATAL_ERROR "Mg Engine requires Visual Studio 17 (v15.7.5) or higher.")
    endif()

    # Use C++20
    target_compile_options(mg_engine PUBLIC /permissive- /std:c++20)

    # Enable multi-core compilation and high warning level with MSVC
    # Ignore warnings about nameless struct/union (ubiquitous in GLM)
    target_compile_options(mg_engine PRIVATE /MP /W4 "/wd\"4201\"" "/wd\"4127\"")

    # Ignore warnings about not using non-standard extensions
    target_compile_definitions(mg_engine PRIVATE _SCL_SECURE_NO_WARNINGS _CRT_SECURE_NO_WARNINGS)
endif()

# To get better display in debuggers, prevent GLM from giving vector components multiple names.
target_compile_definitions(mg_engine PUBLIC GLM_FORCE_XYZW_ONLY=1)

# Set up compiler options for GCC & Clang
if((${CMAKE_COMPILER_IS_GNUCXX}) OR (${CMAKE_CXX_COMPILER_ID} MATCHES Clang))
    target_compile_options(mg_engine PRIVATE
        -Wall
        -Wextra
        -Wdeprecated
        -Wshadow
        -Wnon-virtual-dtor
        -Wsign-conversion
        -Wno-unused-const-variable
    )

    # Link std::experimental::filesystem
    target_link_libraries(mg_engine PUBLIC stdc++fs)

    # Set up sanitizers in debug builds
    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        set(SANITIZERS "-fsanitize=address,undefined")

        if(MG_DEBUG_SANITIZERS)
            target_compile_options(mg_engine PRIVATE ${SANITIZERS})
            target_link_libraries(mg_engine PRIVATE ${SANITIZERS})
        endif()

        # Enable minimal optimizations for debug builds
        # target_compile_options(mg_engine PRIVATE -Og)
    endif()

    if(MG_STDLIB_ASSERTIONS)
        target_compile_options(mg_engine PUBLIC "-D_GLIBCXX_ASSERTIONS=1")
    endif()
endif()

target_include_directories(mg_engine
    PUBLIC
        $<BUILD_INTERFACE:${MG_HEADER_DIR}>
        $<INSTALL_INTERFACE:${MG_HEADER_INSTALL_PATH}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mg_engine
    PUBLIC
        Threads::Threads
        glm::glm
        tl::optional
        imgui
    PRIVATE
        bullet::bullet
        function2
        plf_colony
        glad
        glfw
        libzip::zip
        stb
        utf8.h
        SndFile::sndfile
        OpenAL::OpenAL
        hjson
)

# Add '_d' suffix to debug builds (i.e. (lib)mg_engine_d.{so,dll,lib})
# Allows simultaneous installation of debug and release builds.
set_target_properties(mg_engine PROPERTIES DEBUG_POSTFIX _d)

install(TARGETS mg_engine EXPORT mg_engine_targets DESTINATION "${MG_LIB_INSTALL_PATH}")
install(DIRECTORY "${MG_HEADER_DIR}" DESTINATION "${MG_HEADER_INSTALL_PATH}")
