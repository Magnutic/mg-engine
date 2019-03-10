cmake_minimum_required(VERSION 3.8.2)

set(MG_HEADER_DIR "${MG_ENGINE_SOURCE_DIR}/include/")

# Public headers
set(MG_HEADERS
    ${MG_HEADER_DIR}/mg/containers/mg_pooling_vector.h
    ${MG_HEADER_DIR}/mg/containers/mg_slot_map.h
    ${MG_HEADER_DIR}/mg/containers/mg_small_vector.h
    ${MG_HEADER_DIR}/mg/core/mg_config.h
    ${MG_HEADER_DIR}/mg/core/mg_file_loader.h
    ${MG_HEADER_DIR}/mg/core/mg_identifier.h
    ${MG_HEADER_DIR}/mg/core/mg_log.h
    ${MG_HEADER_DIR}/mg/core/mg_root.h
    ${MG_HEADER_DIR}/mg/core/mg_rotation.h
    ${MG_HEADER_DIR}/mg/core/mg_runtime_error.h
    ${MG_HEADER_DIR}/mg/core/mg_thread_pool.h
    ${MG_HEADER_DIR}/mg/core/mg_transform.h
    ${MG_HEADER_DIR}/mg/core/mg_window.h
    ${MG_HEADER_DIR}/mg/core/mg_window_settings.h
    ${MG_HEADER_DIR}/mg/ecs/mg_component.h
    ${MG_HEADER_DIR}/mg/ecs/mg_entity.h
    ${MG_HEADER_DIR}/mg/gfx/mg_billboard_renderer.h
    ${MG_HEADER_DIR}/mg/gfx/mg_blend_modes.h
    ${MG_HEADER_DIR}/mg/gfx/mg_buffer_texture.h
    ${MG_HEADER_DIR}/mg/gfx/mg_camera.h
    ${MG_HEADER_DIR}/mg/gfx/mg_debug_renderer.h
    ${MG_HEADER_DIR}/mg/gfx/mg_frustum.h
    ${MG_HEADER_DIR}/mg/gfx/mg_gfx_device.h
    ${MG_HEADER_DIR}/mg/gfx/mg_light.h
    ${MG_HEADER_DIR}/mg/gfx/mg_light_buffers.h
    ${MG_HEADER_DIR}/mg/gfx/mg_light_grid.h
    ${MG_HEADER_DIR}/mg/gfx/mg_material.h
    ${MG_HEADER_DIR}/mg/gfx/mg_material_repository.h
    ${MG_HEADER_DIR}/mg/gfx/mg_matrix_ubo.h
    ${MG_HEADER_DIR}/mg/gfx/mg_mesh_renderer.h
    ${MG_HEADER_DIR}/mg/gfx/mg_mesh_repository.h
    ${MG_HEADER_DIR}/mg/gfx/mg_post_process.h
    ${MG_HEADER_DIR}/mg/gfx/mg_pipeline.h
    ${MG_HEADER_DIR}/mg/gfx/mg_pipeline_repository.h
    ${MG_HEADER_DIR}/mg/gfx/mg_render_command_list.h
    ${MG_HEADER_DIR}/mg/gfx/mg_render_target.h
    ${MG_HEADER_DIR}/mg/gfx/mg_shader.h
    ${MG_HEADER_DIR}/mg/gfx/mg_texture2d.h
    ${MG_HEADER_DIR}/mg/gfx/mg_texture_repository.h
    ${MG_HEADER_DIR}/mg/gfx/mg_uniform_buffer.h
    ${MG_HEADER_DIR}/mg/gfx/mg_vertex.h
    ${MG_HEADER_DIR}/mg/gfx/mg_vertex_attribute.h
    ${MG_HEADER_DIR}/mg/input/mg_input.h
    ${MG_HEADER_DIR}/mg/input/mg_joy.h
    ${MG_HEADER_DIR}/mg/input/mg_keyboard.h
    ${MG_HEADER_DIR}/mg/input/mg_mouse.h
    ${MG_HEADER_DIR}/mg/mg_defs.h
    ${MG_HEADER_DIR}/mg/resource_cache/internal/mg_resource_entry.h
    ${MG_HEADER_DIR}/mg/resource_cache/internal/mg_resource_entry_base.h
    ${MG_HEADER_DIR}/mg/resource_cache/mg_resource_access_guard.h
    ${MG_HEADER_DIR}/mg/resource_cache/mg_resource_cache.h
    ${MG_HEADER_DIR}/mg/resource_cache/mg_resource_exceptions.h
    ${MG_HEADER_DIR}/mg/resource_cache/mg_resource_handle.h
    ${MG_HEADER_DIR}/mg/resource_cache/mg_resource_loading_input.h
    ${MG_HEADER_DIR}/mg/resources/mg_mesh_resource.h
    ${MG_HEADER_DIR}/mg/resources/mg_shader_enums.h
    ${MG_HEADER_DIR}/mg/resources/mg_shader_resource.h
    ${MG_HEADER_DIR}/mg/resources/mg_texture_resource.h
    ${MG_HEADER_DIR}/mg/utils/mg_assert.h
    ${MG_HEADER_DIR}/mg/utils/mg_binary_io.h
    ${MG_HEADER_DIR}/mg/utils/mg_fopen_utf8.h
    ${MG_HEADER_DIR}/mg/utils/mg_gsl.h
    ${MG_HEADER_DIR}/mg/utils/mg_hash_combine.h
    ${MG_HEADER_DIR}/mg/utils/mg_instance_counter.h
    ${MG_HEADER_DIR}/mg/utils/mg_macros.h
    ${MG_HEADER_DIR}/mg/utils/mg_math_utils.h
    ${MG_HEADER_DIR}/mg/utils/mg_opaque_handle.h
    ${MG_HEADER_DIR}/mg/utils/mg_ptr_math.h
    ${MG_HEADER_DIR}/mg/utils/mg_string_utils.h
    ${MG_HEADER_DIR}/mg/utils/mg_text_file_io.h
    ${MG_HEADER_DIR}/mg/utils/mg_vector_normalised.h
)

# Create mg_engine library
add_library(mg_engine
    ${MG_HEADERS} # Including headers in library target allows them to show up in IDE project trees.
    core/mg_config.cpp
    core/mg_file_loader.cpp
    core/mg_identifier.cpp
    core/mg_log.cpp
    core/mg_root.cpp
    core/mg_rotation.cpp
    core/mg_window.cpp
    core/mg_window_settings.cpp
    ecs/mg_entity.cpp
    gfx/mg_light_buffers.cpp
    gfx/mg_light_grid.cpp
    gfx/mg_material_repository.cpp
    gfx/mg_shader_factory.cpp
    gfx/opengl/mg_billboard_renderer.cpp
    gfx/opengl/mg_buffer_texture.cpp
    gfx/opengl/mg_debug_renderer.cpp
    gfx/opengl/mg_gfx_device.cpp
    gfx/opengl/mg_gl_debug.cpp
    gfx/opengl/mg_material.cpp
    gfx/opengl/mg_matrix_ubo.cpp
    gfx/opengl/mg_mesh_renderer.cpp
    gfx/opengl/mg_mesh_repository.cpp
    gfx/opengl/mg_opengl_shader.cpp
    gfx/opengl/mg_post_process.cpp
    gfx/opengl/mg_pipeline.cpp
    gfx/opengl/mg_pipeline_repository.cpp
    gfx/opengl/mg_render_command_list.cpp
    gfx/opengl/mg_render_target.cpp
    gfx/opengl/mg_shader.cpp
    gfx/opengl/mg_texture2d.cpp
    gfx/opengl/mg_texture_repository.cpp
    gfx/opengl/mg_uniform_buffer.cpp
    gfx/opengl/shader_factories/mg_mesh_shader_provider.cpp
    gfx/opengl/shader_factories/mg_post_process_shader_provider.cpp
    input/mg_input.cpp
    input/mg_joy.cpp
    input/mg_keyboard.cpp
    input/mg_mouse.cpp
    resource_cache/internal/mg_resource_entry_base.cpp
    resource_cache/mg_base_resource.cpp
    resource_cache/mg_resource_cache.cpp
    resources/mg_mesh_resource.cpp
    resources/mg_raw_resource.cpp
    resources/mg_shader_resource.cpp
    resources/mg_texture_resource.cpp
    utils/mg_fopen_utf8.cpp
    utils/mg_string_utils.cpp
    utils/mg_text_file_io.cpp
)

# Compile using standard C++17
target_compile_features(mg_engine PUBLIC cxx_std_17)
set_target_properties(mg_engine PROPERTIES CXX_EXTENSIONS OFF)

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

    # Ignore warnings about <codecvt>, deprecated functionality for which the standard library has
    # no substitute
    target_compile_definitions(mg_engine PRIVATE _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
endif()

# Configuration defines for dependencies
target_compile_definitions(mg_engine PUBLIC
    GLM_ENABLE_EXPERIMENTAL # Use experimental (GTX) features in GLM (required for GLM > v0.9.9.0)
)

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

    # Set up sanitisers in debug builds
    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        set(SANITISERS "-fsanitize=address,undefined")

        if(${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
            # Clang also has instrumentation to detect unsigned overflow, which is not necessarily
            # incorrect code, but often indicative of bugs.
            set(SANITISERS "${SANITISERS},unsigned-integer-overflow")
        endif()

        if(MG_DEBUG_SANITISERS)
            target_compile_options(mg_engine PRIVATE ${SANITISERS})
            target_link_libraries(mg_engine PRIVATE ${SANITISERS})
        endif()
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
        glm
        fmt::fmt
    PRIVATE
        glad
        glfw
        zip
)

# Add '_d' suffix to debug builds (i.e. (lib)mg_engine_d.{so,dll,lib})
# Allows simultaneous installation of debug and release builds.
set_target_properties(mg_engine PROPERTIES DEBUG_POSTFIX _d)

install(TARGETS mg_engine EXPORT mg_engine_targets DESTINATION "${MG_LIB_INSTALL_PATH}")
install(DIRECTORY "${MG_HEADER_DIR}" DESTINATION "${MG_HEADER_INSTALL_PATH}")
