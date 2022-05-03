cmake_minimum_required(VERSION 3.11)

set(MG_HEADER_DIR "${MG_SOURCE_DIR}/include/")

# Create mg_engine library
add_library(mg_engine
    audio/mg_audio_context.cpp
    audio/mg_sound_buffer_handle.cpp
    audio/mg_sound_source.cpp
    core/mg_application_context.cpp
    core/mg_config.cpp
    core/mg_curve.cpp
    core/mg_file_loader.cpp
    core/mg_identifier.cpp
    core/mg_log.cpp
    core/mg_rotation.cpp
    core/mg_window.cpp
    core/mg_window_settings.cpp
    ecs/mg_entity.cpp
    editor/mg_curve_editor.cpp
    gfx/mg_bitmap_font.cpp
    gfx/mg_gfx_object_handles.cpp
    gfx/mg_light_buffers.cpp
    gfx/mg_light_grid.cpp
    gfx/mg_material.cpp
    gfx/mg_material_pool.cpp
    gfx/mg_matrix_uniform_handler.cpp
    gfx/mg_render_command_list.cpp
    gfx/mg_skeleton.cpp
    gfx/mg_texture_pool.cpp
    gfx/mg_texture_related_types.cpp
    gfx/opengl/mg_billboard_renderer.cpp
    gfx/opengl/mg_buffer_texture.cpp
    gfx/opengl/mg_debug_renderer.cpp
    gfx/opengl/mg_gfx_debug_group.cpp
    gfx/opengl/mg_gfx_device.cpp
    gfx/opengl/mg_gl_debug.cpp
    gfx/opengl/mg_mesh_pool.cpp
    gfx/opengl/mg_mesh_renderer.cpp
    gfx/opengl/mg_opengl_shader.cpp
    gfx/opengl/mg_pipeline.cpp
    gfx/opengl/mg_pipeline_pool.cpp
    gfx/opengl/mg_post_process.cpp
    gfx/opengl/mg_render_target.cpp
    gfx/opengl/mg_shader.cpp
    gfx/opengl/mg_texture2d.cpp
    gfx/opengl/mg_ui_renderer.cpp
    gfx/opengl/mg_uniform_buffer.cpp
    gfx/opengl/shader_code/mg_mesh_renderer_shader_framework.cpp
    input/mg_input.cpp
    input/mg_joy.cpp
    input/mg_keyboard.cpp
    input/mg_mouse.cpp
    mg_bounding_volumes.cpp
    mg_imgui_overlay.cpp
    mg_unicode.cpp
    parser/mg_lexer.cpp
    parser/mg_parser.cpp
    physics/mg_character_controller.cpp
    physics/mg_physics.cpp
    physics/mg_physics_debug_renderer.cpp
    resource_cache/internal/mg_resource_entry_base.cpp
    resource_cache/mg_base_resource.cpp
    resource_cache/mg_resource_cache.cpp
    resources/mg_font_resource.cpp
    resources/mg_mesh_resource.cpp
    resources/mg_raw_resource.cpp
    resources/mg_shader_resource.cpp
    resources/mg_sound_resource.cpp
    resources/mg_texture_resource.cpp
    utils/mg_file_io.cpp
    utils/mg_file_time_helper.cpp
    utils/mg_rand.cpp
    utils/mg_string_utils.cpp
)

# Compile using standard C++17
target_compile_features(mg_engine PUBLIC cxx_std_17)
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

    # Use c++17
    target_compile_options(mg_engine PUBLIC /permissive-)

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

        if(${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
            # Clang also has instrumentation to detect unsigned overflow, which is not necessarily
            # incorrect code, but often indicative of bugs.
            set(SANITIZERS "${SANITIZERS},unsigned-integer-overflow")
        endif()

        if(MG_DEBUG_SANITIZERS)
            target_compile_options(mg_engine PRIVATE ${SANITIZERS})
            target_link_libraries(mg_engine PRIVATE ${SANITIZERS})
        endif()

        # Enable minimal optimisations for debug builds
        # target_compile_options(mg_engine PRIVATE -Og)
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
        fmt::fmt
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
)

# Add '_d' suffix to debug builds (i.e. (lib)mg_engine_d.{so,dll,lib})
# Allows simultaneous installation of debug and release builds.
set_target_properties(mg_engine PROPERTIES DEBUG_POSTFIX _d)

install(TARGETS mg_engine EXPORT mg_engine_targets DESTINATION "${MG_LIB_INSTALL_PATH}")
install(DIRECTORY "${MG_HEADER_DIR}" DESTINATION "${MG_HEADER_INSTALL_PATH}")
