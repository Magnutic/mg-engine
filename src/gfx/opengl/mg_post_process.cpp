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

#include "mg/gfx/mg_post_process.h"

#include "mg/containers/mg_small_vector.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_pipeline_repository.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/mg_defs.h"
#include "mg/utils/mg_opaque_handle.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

namespace Mg::gfx {

namespace {

const float quad_vertices[] = { -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,
                                1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f };

// Texture units 8 & 9 are reserved for input colour and depth texture, respectively.
constexpr uint32_t k_input_colour_texture_unit = 8;
constexpr uint32_t k_input_depth_texture_unit  = 9;

constexpr uint32_t k_material_params_ubo_slot = 0;
constexpr uint32_t k_frame_block_ubo_slot     = 1;

struct FrameBlock {
    float z_near;
    float z_far;
};

constexpr const char* post_process_vs = R"(
#version 330 core

layout (location = 0) in vec2 v_pos;

out vec2 tex_coord;

void main() {
    gl_Position = vec4(v_pos, 0.0, 1.0);
    tex_coord = (v_pos + vec2(1.0)) * 0.5;
}
)";

constexpr const char* post_process_fs_preamble = R"(
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

constexpr const char* post_process_fs_fallback =
    R"(void main() { frag_out = vec4(1.0, 0.0, 1.0, 1.0); })";

PipelineRepository make_post_process_pipeline_repository()
{
    PipelineRepository::Config config{};

    config.preamble_shader_code = { VertexShaderCode{ post_process_vs },
                                    {},
                                    FragmentShaderCode{ post_process_fs_preamble } };

    config.on_error_shader_code = { {}, {}, FragmentShaderCode{ post_process_fs_fallback } };

    config.pipeline_prototype.common_input_layout =
        { { "MaterialParams", PipelineInputType::UniformBuffer, k_material_params_ubo_slot },
          { "FrameBlock", PipelineInputType::UniformBuffer, k_frame_block_ubo_slot },
          { "sampler_colour", PipelineInputType::Sampler2D, k_input_colour_texture_unit },
          { "sampler_depth", PipelineInputType::Sampler2D, k_input_depth_texture_unit } };

    config.material_params_ubo_slot = k_material_params_ubo_slot;

    return PipelineRepository(config);
}

} // namespace

struct PostProcessRendererData {
    PipelineRepository pipeline_repository = make_post_process_pipeline_repository();

    UniformBuffer frame_block_ubo{ sizeof(FrameBlock) };

    OpaqueHandle vbo;
    OpaqueHandle vao;
};

namespace {

void init(PostProcessRendererData& data)
{
    GLuint vao_id = 0;
    GLuint vbo_id = 0;

    // Create mesh
    glGenVertexArrays(1, &vao_id);
    glBindVertexArray(vao_id);

    glGenBuffers(1, &vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);

    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    MG_CHECK_GL_ERROR();

    // N.B. cannot directly use these as arguments to OpenGL functions, as they expect pointer to
    // GLuint (uint32_t) whereas these are OpaqueHandle::Value (an opaque typedef of uint64_t).
    data.vao = vao_id;
    data.vbo = vao_id;
}

void setup_render_pipeline(PostProcessRendererData& data,
                           const Material&          material,
                           TextureHandle            input_colour,
                           Opt<TextureHandle>       input_depth,
                           float                    z_near,
                           float                    z_far)
{
    FrameBlock frame_block{ z_near, z_far };
    data.frame_block_ubo.set_data(byte_representation(frame_block));

    small_vector<PipelineInputBinding, 3> input_bindings = { { k_frame_block_ubo_slot,
                                                               data.frame_block_ubo },
                                                             { k_input_colour_texture_unit,
                                                               input_colour } };

    if (input_depth.has_value()) {
        input_bindings.push_back({ k_input_depth_texture_unit, input_depth.value() });
    }

    PipelineRepository::BindingContext binding_context = data.pipeline_repository.binding_context(
        input_bindings);

    data.pipeline_repository.bind_pipeline(material, binding_context);
}

} // namespace

PostProcessRenderer::PostProcessRenderer() : PimplMixin()
{
    init(data());
}

PostProcessRenderer::~PostProcessRenderer()
{
    GLuint vao_id = static_cast<uint32_t>(data().vao.value);
    GLuint vbo_id = static_cast<uint32_t>(data().vbo.value);

    glDeleteVertexArrays(1, &vao_id);
    glDeleteBuffers(1, &vbo_id);
}

void PostProcessRenderer::post_process(const Material& material, TextureHandle input_colour)
{
    setup_render_pipeline(data(), material, input_colour, nullopt, 0.0f, 0.0f);

    GLuint vao_id = static_cast<GLuint>(data().vao.value);

    glBindVertexArray(vao_id);
    glDrawArrays(GL_TRIANGLES, 0, 12);
    MG_CHECK_GL_ERROR();
}

void PostProcessRenderer::post_process(const Material& material,
                                       TextureHandle   input_colour,
                                       TextureHandle   input_depth,
                                       float           z_near,
                                       float           z_far)
{
    setup_render_pipeline(data(), material, input_colour, input_depth, z_near, z_far);

    GLuint vao_id = static_cast<GLuint>(data().vao.value);

    glBindVertexArray(vao_id);
    glDrawArrays(GL_TRIANGLES, 0, 12);
    MG_CHECK_GL_ERROR();
}

void PostProcessRenderer::drop_shaders()
{
    data().pipeline_repository.drop_pipelines();
}

} // namespace Mg::gfx
