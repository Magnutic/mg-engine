//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2026, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_block_scene_editor_render_common.h
 * Common utilities for rendering the block scene editor.
 */

#pragma once

#include "mg/core/gfx/mg_pipeline.h"
#include "mg/core/gfx/mg_uniform_buffer.h"

#include <glm/ext/vector_int2.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Mg {

namespace gfx {
class ICamera;
class IRenderTarget;
} // namespace gfx

class EditModeRenderCommon {
public:
    void draw_box_outline(const gfx::ICamera& cam,
                          gfx::IRenderTarget& render_target,
                          int coord_x,
                          int coord_y,
                          float z_min,
                          float z_max,
                          float block_size,
                          glm::vec4 colour);

    void draw_grid_around_point(const gfx::ICamera& cam,
                                gfx::IRenderTarget& render_target,
                                glm::ivec2 point,
                                float elevation,
                                float block_size);

private:
    gfx::Pipeline::Settings configure_pipeline(const gfx::ICamera& cam,
                                               gfx::IRenderTarget& render_target,
                                               glm::vec3 position,
                                               glm::vec3 scale,
                                               glm::vec4 colour,
                                               gfx::VertexArrayHandle vao);

    // Block of shader uniforms.
    struct DrawParamsBlock {
        glm::vec4 colour;
        glm::mat4 MVP;
    };

    // RAII owner for OpenGL mesh buffers
    struct MeshBuffers {
        gfx::VertexArrayHandle::Owner vao;
        gfx::BufferHandle::Owner vbo;
        gfx::BufferHandle::Owner ibo;

        int num_indices;
    };

    static MeshBuffers make_cube_mesh();
    static MeshBuffers make_grid_mesh();

    static gfx::Pipeline make_pipeline();

    MeshBuffers m_cube_mesh{ make_cube_mesh() };
    MeshBuffers m_grid_mesh{ make_grid_mesh() };
    gfx::UniformBuffer m_draw_params_ubo{ sizeof(DrawParamsBlock) };
    gfx::Pipeline m_pipeline = make_pipeline();
};

} // namespace Mg
