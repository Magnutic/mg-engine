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

#include "mg_mesh_shader_provider.h"

#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_light_grid.h"
#include "mg/gfx/mg_material.h"

#include "../mg_glad.h"

#include "shader_code/mg_mesh_framework_shader_code.h"

namespace Mg::gfx::mesh_renderer {

static constexpr const char* mesh_fs_fallback = R"(
float attenuate(float distance_sqr, float range_sqr_reciprocal) { return 1.0; }

vec3 light(const LightInput light, const SurfaceParams surface, const vec3 view_direction) {
    return vec3(0.0);
}

void final_colour(const SurfaceInput s_in, const SurfaceParams s, inout vec4 colour) {}

void surface(const SurfaceInput s_in, out SurfaceParams s_out) {
    s_out.albedo    = vec3(0.0);
    s_out.specular  = vec3(0.0);
    s_out.gloss     = 0.0;
    s_out.normal    = vec3(0.0);
    s_out.emission  = vec3(100.0, 0.0, 100.0);
    s_out.occlusion = 0.0;
    s_out.alpha     = 1.0;
}
)";

FrameBlock make_frame_block(const ICamera& camera, float current_time, float camera_exposure)
{
    std::array<int, 4> viewport_data{};
    glGetIntegerv(GL_VIEWPORT, &viewport_data[0]);
    glm::uvec2 viewport_size{ narrow<uint32_t>(viewport_data[2]),
                              narrow<uint32_t>(viewport_data[3]) };

    static const float scale = MG_LIGHT_GRID_DEPTH / std::log2(float(MG_LIGHT_GRID_FAR_PLANE));

    const auto depth_range = camera.depth_range();
    const auto z_near      = depth_range.near();
    const auto z_far       = depth_range.far();
    const auto c           = std::log2(2.0f * z_far * z_near);

    FrameBlock frame_block;
    frame_block.camera_position_and_time = glm::vec4(camera.get_position(), current_time);
    frame_block.viewport_size            = viewport_size;

    frame_block.cluster_grid_params.z_param = glm::vec2(z_near - z_far, z_near + z_far);
    frame_block.cluster_grid_params.scale   = -scale;
    frame_block.cluster_grid_params.bias    = float(MG_LIGHT_GRID_DEPTH_BIAS) + c * scale;

    frame_block.camera_exposure = camera_exposure;

    return frame_block;
}

} // namespace Mg::gfx::mesh_renderer

namespace Mg::gfx {

experimental::PipelineRepository make_mesh_pipeline_repository()
{
    using namespace experimental;
    using namespace mesh_renderer;

    PipelineRepository::Config config{};
    config.preamble_shader_code = { VertexShaderCode{ k_lit_mesh_framework_vertex_code },
                                    {},
                                    FragmentShaderCode{ k_lit_mesh_framework_fragment_code } };

    config.on_error_shader_code = { {}, {}, FragmentShaderCode{ mesh_fs_fallback } };

    config.pipeline_prototype.common_input_layout = {
        { "MatrixBlock", PipelineInputType::UniformBuffer, k_matrix_ubo_slot },
        { "FrameBlock", PipelineInputType::UniformBuffer, k_frame_ubo_slot },
        { "LightBlock", PipelineInputType::UniformBuffer, k_light_ubo_slot },
        { "MaterialParams", PipelineInputType::UniformBuffer, k_material_params_ubo_slot },
        { "_sampler_tile_data", PipelineInputType::BufferTexture, k_sampler_tile_data_index },
        { "_sampler_light_index", PipelineInputType::BufferTexture, k_sampler_light_index_index }
    };

    return PipelineRepository(config);
}

} // namespace Mg::gfx
