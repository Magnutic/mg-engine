//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_post_process.h"

#include "mg/containers/mg_small_vector.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_pipeline_pool.h"
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

constexpr uint32_t k_material_params_ubo_slot = 0;
constexpr uint32_t k_frame_block_ubo_slot = 1;

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

    config.preamble_shader_code = { VertexShaderCode{ post_process_vs },
                                    {},
                                    FragmentShaderCode{ post_process_fs_preamble } };

    config.on_error_shader_code = { {}, {}, FragmentShaderCode{ post_process_fs_fallback } };

    config.shared_input_layout = Array<PipelineInputDescriptor>::make(3);
    {
        PipelineInputDescriptor& frame_block_descriptor = config.shared_input_layout[0];
        frame_block_descriptor.input_name = "FrameBlock";
        frame_block_descriptor.type = PipelineInputType::UniformBuffer;
        frame_block_descriptor.location = k_frame_block_ubo_slot;
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

    config.material_params_ubo_slot = k_material_params_ubo_slot;

    return PipelinePool(std::move(config));
}

Pipeline::Settings pipeline_settings()
{
    Pipeline::Settings settings;
    settings.depth_test_condition = DepthTestCondition::always;
    settings.depth_write_enabled = false;
    return settings;
}

} // namespace

struct PostProcessRendererData {
    PipelinePool pipeline_pool = make_post_process_pipeline_pool();

    UniformBuffer frame_block_ubo{ sizeof(FrameBlock) };

    BufferHandle vbo;
    VertexArrayHandle vao;

    Opt<PipelineBindingContext> binding_context;
};

PostProcessRenderer::PostProcessRenderer() : PImplMixin()
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

    impl().vao.set(vao_id);
    impl().vbo.set(vbo_id);
}

PostProcessRenderer::~PostProcessRenderer()
{
    MG_GFX_DEBUG_GROUP("~PostProcessRenderer")
    const auto vbo_id = impl().vbo.as_gl_id();
    const auto vao_id = impl().vao.as_gl_id();

    glDeleteBuffers(1, &vbo_id);
    glDeleteVertexArrays(1, &vao_id);
}

PostProcessRenderer::Context PostProcessRenderer::make_context() noexcept
{
    return { impl() };
}

void PostProcessRenderer::post_process(const Context& context,
                                       const Material& material,
                                       TextureHandle sampler_colour) noexcept
{
    MG_ASSERT(context.m_data == &impl() && "Context belongs to a different PostProcessRenderer");
    MG_GFX_DEBUG_GROUP("PostProcessRenderer::post_process")

    impl().pipeline_pool.bind_material_pipeline(material,
                                                pipeline_settings(),
                                                impl().binding_context.value());

    std::array shared_input_bindings = {
        PipelineInputBinding{ k_frame_block_ubo_slot, impl().frame_block_ubo },
        PipelineInputBinding{ k_sampler_colour_texture_unit, sampler_colour },
        PipelineInputBinding{ k_sampler_depth_texture_unit, TextureHandle::null_handle() }
    };

    Pipeline::bind_shared_inputs(shared_input_bindings);

    const auto vao_id = impl().vao.as_gl_id();

    glBindVertexArray(vao_id);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    MG_CHECK_GL_ERROR();
}

void PostProcessRenderer::post_process(const Context& context,
                                       const Material& material,
                                       TextureHandle sampler_colour,
                                       TextureHandle sampler_depth,
                                       float z_near,
                                       float z_far) noexcept
{
    MG_ASSERT(context.m_data == &impl() && "Context belongs to a different PostProcessRenderer");
    MG_GFX_DEBUG_GROUP("PostProcessRenderer::post_process")

    impl().pipeline_pool.bind_material_pipeline(material,
                                                pipeline_settings(),
                                                impl().binding_context.value());

    FrameBlock frame_block{ z_near, z_far };
    impl().frame_block_ubo.set_data(byte_representation(frame_block));

    std::array shared_input_bindings = {
        PipelineInputBinding{ k_frame_block_ubo_slot, impl().frame_block_ubo },
        PipelineInputBinding{ k_sampler_colour_texture_unit, sampler_colour },
        PipelineInputBinding{ k_sampler_depth_texture_unit, sampler_depth }
    };

    Pipeline::bind_shared_inputs(shared_input_bindings);

    const auto vao_id = impl().vao.as_gl_id();

    glBindVertexArray(vao_id);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    MG_CHECK_GL_ERROR();
}

void PostProcessRenderer::drop_shaders() noexcept
{
    MG_GFX_DEBUG_GROUP("PostProcessRenderer::drop_shaders")
    impl().pipeline_pool.drop_pipelines();
}

PostProcessRenderer::Context::Context(PostProcessRendererData& data) : m_data(&data)
{
    m_data->binding_context = PipelineBindingContext{};
}

PostProcessRenderer::Context::~Context()
{
    m_data->binding_context = nullopt;
}

} // namespace Mg::gfx
