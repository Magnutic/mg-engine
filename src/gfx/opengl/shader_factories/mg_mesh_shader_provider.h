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

/** @file mg_mesh_shader_provider.
 * Creates shader programs for MeshRenderer.
 */

#pragma once

#include "../../mg_shader_factory.h"
#include "../mg_gl_gfx_device.h"

#include "mg/gfx/mg_texture_related_types.h"
#include "mg/gfx/mg_uniform_buffer.h"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace Mg::gfx {

class ICamera;

class MeshShaderProvider : public IShaderProvider {
public:
    ShaderCode on_error_shader_code() const override;
    ShaderCode make_shader_code(const Material& material) const override;
    void       setup_shader_state(ShaderHandle program, const Material& material) const override;
};

inline ShaderFactory make_mesh_shader_factory()
{
    return ShaderFactory{ std::make_unique<MeshShaderProvider>() };
}

} // namespace Mg::gfx

namespace Mg::gfx::mesh_renderer {

static constexpr UniformBufferSlot k_matrix_ubo_slot{ 0 };
static constexpr UniformBufferSlot k_frame_ubo_slot{ 1 };
static constexpr UniformBufferSlot k_light_ubo_slot{ 2 };
static constexpr UniformBufferSlot k_material_params_ubo_slot{ 3 };

static constexpr TextureUnit k_sampler_tile_data_index{ 8 };
static constexpr TextureUnit k_sampler_light_index_index{ 9 }; // Index of sampler for light indices

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

FrameBlock make_frame_block(const ICamera& camera, float current_time, float camera_exposure);

} // namespace Mg::gfx::mesh_renderer
