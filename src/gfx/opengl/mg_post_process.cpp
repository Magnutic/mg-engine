//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_post_process.h"

#include "mg/containers/mg_small_vector.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_pipeline_pool.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/gfx/mg_shader_related_types.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/mg_defs.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

#include <array>

namespace Mg::gfx {

namespace {

const std::array<float, 12> quad_vertices = { -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,
                                              1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f };

// Texture units 8 & 9 are reserved for input colour and depth texture, respectively.
constexpr uint32_t k_sampler_colour_texture_unit = 8;
constexpr uint32_t k_sampler_depth_texture_unit = 9;

constexpr uint32_t k_material_parameters_binding_location = 0;
constexpr uint32_t k_frame_block_descriptor_location = 1;

struct FrameBlock {
    float z_near;
    float z_far;
};

constexpr const char* post_process_vs = R"(
#version 440 core

layout (location = 0) in vec2 v_pos;

out vec2 tex_coord;

void main() {
    gl_Position = vec4(v_pos, 0.0, 1.0);
    tex_coord = (v_pos + vec2(1.0)) * 0.5;
}
)";

constexpr const char* post_process_fs_preamble = R"(
#version 440 core

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

float linearize_depth_perspective(float depth) {
    return ZNEAR * ZFAR / (ZFAR + depth * (ZNEAR - ZFAR));
}

float linearize_depth_ortho(float depth) {
    return (2.0 * depth - 1.0) * (ZFAR - ZNEAR) + ZNEAR;
}
)";

constexpr const char* post_process_fs_fallback =
    R"(void main() { frag_out = vec4(1.0, 0.0, 1.0, 1.0); })";

PipelinePool make_post_process_pipeline_pool()
{
    MG_GFX_DEBUG_GROUP("make_post_process_pipeline_pool")

    PipelinePoolConfig config{};
    config.name = "PostProcessRenderer";

    config.preamble_shader_code = { VertexShaderCode{ post_process_vs },
                                    {},
                                    FragmentShaderCode{ post_process_fs_preamble } };

    config.on_error_shader_code = { {}, {}, FragmentShaderCode{ post_process_fs_fallback } };

    config.shared_input_layout = Array<PipelineInputDescriptor>::make(3);
    {
        PipelineInputDescriptor& frame_block_descriptor = config.shared_input_layout[0];
        frame_block_descriptor.input_name = "FrameBlock";
        frame_block_descriptor.type = PipelineInputType::UniformBuffer;
        frame_block_descriptor.location = k_frame_block_descriptor_location;
        frame_block_descriptor.mandatory = false;

        PipelineInputDescriptor& sampler_colour_descriptor = config.shared_input_layout[1];
        sampler_colour_descriptor.input_name = "sampler_colour";
        sampler_colour_descriptor.type = PipelineInputType::Sampler2D;
        sampler_colour_descriptor.location = k_sampler_colour_texture_unit;
        sampler_colour_descriptor.mandatory = false;

        PipelineInputDescriptor& sampler_depth_descriptor = config.shared_input_layout[2];
        sampler_depth_descriptor.input_name = "sampler_depth";
        sampler_depth_descriptor.type = PipelineInputType::Sampler2D;
        sampler_depth_descriptor.location = k_sampler_depth_texture_unit;
        sampler_depth_descriptor.mandatory = false;
    }

    config.material_parameters_binding_location = k_material_parameters_binding_location;

    return PipelinePool(std::move(config));
}

BindMaterialPipelineSettings pipeline_settings(const IRenderTarget& render_target,
                                               VertexArrayHandle vertex_array)
{
    BindMaterialPipelineSettings settings;
    settings.depth_test_condition = DepthTestCondition::always;
    settings.depth_write_enabled = false;
    settings.target_framebuffer = render_target.handle();
    settings.viewport_size = render_target.image_size();
    settings.vertex_array = vertex_array;
    return settings;
}

} // namespace

struct PostProcessRenderer::Impl {
    PipelinePool pipeline_pool = make_post_process_pipeline_pool();

    UniformBuffer frame_block_ubo{ sizeof(FrameBlock) };

    BufferHandle vbo;
    VertexArrayHandle vao;

    Opt<PipelineBindingContext> binding_context;
};

PostProcessRenderer::PostProcessRenderer()
{
    MG_GFX_DEBUG_GROUP("init PostProcessRenderer")

    GLuint vao_id = 0;
    GLuint vbo_id = 0;

    // Create mesh
    glGenVertexArrays(1, &vao_id);
    glBindVertexArray(vao_id);

    glGenBuffers(1, &vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);

    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    MG_CHECK_GL_ERROR();

    m_impl->vao.set(vao_id);
    m_impl->vbo.set(vbo_id);
}

PostProcessRenderer::~PostProcessRenderer()
{
    MG_GFX_DEBUG_GROUP("~PostProcessRenderer")
    const auto vbo_id = m_impl->vbo.as_gl_id();
    const auto vao_id = m_impl->vao.as_gl_id();

    glDeleteBuffers(1, &vbo_id);
    glDeleteVertexArrays(1, &vao_id);
}

PostProcessRenderer::Context PostProcessRenderer::make_context() noexcept
{
    return { *m_impl };
}

void PostProcessRenderer::post_process(const Context& context,
                                       const Material& material,
                                       const IRenderTarget& render_target,
                                       TextureHandle sampler_colour) noexcept
{
    MG_ASSERT(context.m_data == m_impl.get() &&
              "Context belongs to a different PostProcessRenderer");
    MG_GFX_DEBUG_GROUP("PostProcessRenderer::post_process")

    m_impl->pipeline_pool.bind_material_pipeline(material,
                                                 pipeline_settings(render_target, m_impl->vao),
                                                 m_impl->binding_context.value());

    std::array shared_input_bindings = {
        PipelineInputBinding{ k_frame_block_descriptor_location, m_impl->frame_block_ubo },
        PipelineInputBinding{
            k_sampler_colour_texture_unit, sampler_colour, shader::SamplerType::Sampler2D },
        PipelineInputBinding{ k_sampler_depth_texture_unit,
                              TextureHandle::null_handle(),
                              shader::SamplerType::Sampler2D }
    };

    Pipeline::bind_shared_inputs(shared_input_bindings);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    MG_CHECK_GL_ERROR();
}

void PostProcessRenderer::post_process(const Context& context,
                                       const Material& material,
                                       const IRenderTarget& render_target,
                                       TextureHandle sampler_colour,
                                       TextureHandle sampler_depth,
                                       float z_near,
                                       float z_far) noexcept
{
    MG_ASSERT(context.m_data == m_impl.get() &&
              "Context belongs to a different PostProcessRenderer");
    MG_GFX_DEBUG_GROUP("PostProcessRenderer::post_process")

    m_impl->pipeline_pool.bind_material_pipeline(material,
                                                 pipeline_settings(render_target, m_impl->vao),
                                                 m_impl->binding_context.value());

    FrameBlock frame_block{ z_near, z_far };
    m_impl->frame_block_ubo.set_data(byte_representation(frame_block));

    std::array shared_input_bindings = {
        PipelineInputBinding{ k_frame_block_descriptor_location, m_impl->frame_block_ubo },
        PipelineInputBinding{ k_sampler_colour_texture_unit, sampler_colour, shader::SamplerType::Sampler2D },
        PipelineInputBinding{ k_sampler_depth_texture_unit, sampler_depth, shader::SamplerType::Sampler2D }
    };

    Pipeline::bind_shared_inputs(shared_input_bindings);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    MG_CHECK_GL_ERROR();
}

void PostProcessRenderer::drop_shaders() noexcept
{
    MG_GFX_DEBUG_GROUP("PostProcessRenderer::drop_shaders")
    m_impl->pipeline_pool.drop_pipelines();
}

PostProcessRenderer::Context::Context(PostProcessRenderer::Impl& data) : m_data(&data)
{
    m_data->binding_context = PipelineBindingContext{};
}

PostProcessRenderer::Context::~Context()
{
    m_data->binding_context = nullopt;
}

} // namespace Mg::gfx
