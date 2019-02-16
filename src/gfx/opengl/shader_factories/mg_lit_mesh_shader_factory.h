//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

/** @file mg_lit_mesh_shader_factory.
 * Creates shader programs for MeshRenderer.
 */

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "../../mg_shader_factory.h"

namespace Mg::gfx {

class ICamera;

class MeshShaderProvider : public IShaderProvider {
public:
    ShaderCode make_shader_code(const Material& material) const override;
    void       setup_shader_state(ShaderProgram& program, const Material& material) const override;
};

inline ShaderFactory make_mesh_shader_factory()
{
    return ShaderFactory{ std::make_unique<MeshShaderProvider>() };
}

} // namespace Mg::gfx

namespace Mg::gfx::mesh_renderer {

static constexpr uint32_t k_matrix_ubo_index{ 0 };
static constexpr uint32_t k_frame_ubo_index{ 1 };
static constexpr uint32_t k_light_ubo_index{ 2 };
static constexpr uint32_t k_material_params_ubo_index{ 3 };

static constexpr int32_t k_sampler_tile_data_index   = 8;
static constexpr int32_t k_sampler_light_index_index = 9; // Index of sampler for light indices.

/** Location of '_matrix_index' vertex attribute in shader code. */
static constexpr uint32_t k_matrix_index_vertex_attrib_location = 8;

// Parameters used to calculate cluster slice from fragment depth
struct ClusterGridParams {
    glm::vec2 z_param;
    float     scale = 0.0f;
    float     bias  = 0.0f;
};

/** Frame-global UBO block */
struct FrameBlock {
    ClusterGridParams cluster_grid_params;

    // .xyz: camera_position; .w: time. vec4 for alignment purposes.
    glm::vec4 camera_position_and_time;

    glm::uvec2 viewport_size;

    float camera_exposure = 0.0f;
};

FrameBlock make_frame_block(const ICamera& camera);

} // namespace Mg::gfx::mesh_renderer
