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

#include "mg_post_process_shader_provider.h"

#include "mg/gfx/mg_material.h"
#include "mg/resource_cache/mg_resource_access_guard.h"
#include "mg/resources/mg_shader_resource.h"

#include "../mg_opengl_shader.h"

namespace Mg::gfx::post_renderer {

static constexpr const char* post_process_vs = R"(
#version 330 core

layout (location = 0) in vec2 v_pos;

out vec2 tex_coord;

void main() {
    gl_Position = vec4(v_pos, 0.0, 1.0);
    tex_coord = (v_pos + vec2(1.0)) * 0.5;
}
)";

static constexpr const char* post_process_fs_preamble = R"(
#version 330 core

layout (location = 0) out vec4 frag_out;

in vec2 tex_coord;

layout(std140) uniform FrameBlock {
    float z_near;
    float z_far;
    // Add more as required
} _frame_block;

#define ZNEAR (_frame_block.z_near)
#define ZFAR (_frame_block.z_far)

uniform sampler2D sampler_colour;
uniform sampler2D sampler_depth;

float linearise_depth(float depth) {
    return ZNEAR * ZFAR / (ZFAR + depth * (ZNEAR - ZFAR));
}
)";

static constexpr const char* post_process_fs_fallback =
    R"(void main() { frag_out = vec4(1.0, 0.0, 1.0, 1.0); })";

} // namespace Mg::gfx::post_renderer

namespace Mg::gfx {

experimental::PipelineRepository make_post_process_pipeline_repository()
{
    using namespace experimental;
    using namespace post_renderer;

    PipelineRepository::Config config{};

    config.preamble_shader_code = { VertexShaderCode{ post_process_vs },
                                    {},
                                    FragmentShaderCode{ post_process_fs_preamble } };

    config.on_error_shader_code = { {}, {}, FragmentShaderCode{ post_process_fs_fallback } };

    config.pipeline_prototype.common_input_layout =
        { { "MaterialParams", PipelineInputType::UniformBuffer, k_material_params_ubo_slot },
          { "FrameBlock", PipelineInputType::UniformBuffer, k_frame_block_ubo_slot },
          { "sampler_colour", PipelineInputType::Sampler2D, k_input_colour_texture_unit },
          { "samler_depth", PipelineInputType::Sampler2D, k_input_depth_texture_unit } };

    return experimental::PipelineRepository(config);
}

} // namespace Mg::gfx
