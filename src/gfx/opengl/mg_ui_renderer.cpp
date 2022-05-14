//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_ui_renderer.h"

#include "../mg_shader.h"

#include "mg/containers/mg_array.h"
#include "mg/gfx/mg_bitmap_font.h"
#include "mg/gfx/mg_blend_modes.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_pipeline_pool.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/gfx/mg_uniform_buffer.h"

#include <glm/mat2x2.hpp>
#include <glm/mat4x4.hpp>

#include "mg_gl_debug.h"
#include "glm/fwd.hpp"
#include <glad/glad.h>

namespace Mg::gfx {

namespace {

using glm::ivec2;
using glm::mat4;
using glm::vec2;
using glm::vec4;

const std::array<float, 8> quad_vertices = { { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f } };

// Binding slots for UniformBufferObjects.
constexpr uint32_t k_draw_params_ubo_slot = 0;
constexpr uint32_t k_material_params_ubo_slot = 1;

struct DrawParamsBlock {
    mat4 M;
};

//--------------------------------------------------------------------------------------------------
// Shader code for UI rendering
//--------------------------------------------------------------------------------------------------

constexpr auto ui_vertex_shader = R"(
#version 330 core

layout(location = 0) in vec2 v_position;

layout(std140) uniform DrawParamsBlock {
    uniform mat4 M;
};

out vec2 tex_coord;

void main() {
    gl_Position = M * vec4(v_position, 0.0, 1.0);
    tex_coord = v_position;
}
)";

constexpr auto ui_fragment_shader_preamble = R"(
#version 330 core

layout (location = 0) out vec4 frag_out;

in vec2 tex_coord;
)";

constexpr auto ui_fragment_shader_fallback = R"(
void main() {
    frag_out = vec4(1.0, 0.0, 1.0, 1.0);
}
)";

constexpr auto text_shader_vs = R"(
#version 330 core

layout(location = 0) in vec2 v_position;
layout(location = 1) in vec2 v_texcoord;

layout(std140) uniform DrawParamsBlock {
    uniform mat4 M;
};

out vec2 tex_coord;

void main() {
    gl_Position = M * vec4(v_position, 0.0, 1.0);
    tex_coord = v_texcoord;
}
)";

constexpr auto text_shader_fs = R"(
#version 330 core
layout (location = 0) out vec4 frag_out;
in vec2 tex_coord;
uniform sampler2D font_texture;
void main() {
    frag_out = texture(font_texture, tex_coord).rrrr;
}
)";

//--------------------------------------------------------------------------------------------------

Pipeline::Settings pipeline_settings(Opt<BlendMode> blend_mode)
{
    Pipeline::Settings settings;
    settings.depth_test_condition = DepthTestCondition::always;
    settings.blending_enabled = true;
    settings.depth_write_enabled = false;

    if (blend_mode) {
        settings.blend_mode = blend_mode.value();
        settings.blending_enabled = true;
    }
    else {
        settings.blending_enabled = false;
    }

    return settings;
}

PipelinePool make_ui_pipeline_factory()
{
    PipelinePoolConfig config = {};

    config.shared_input_layout = Array<PipelineInputDescriptor>::make(1);
    {
        PipelineInputDescriptor& draw_params_block_descriptor = config.shared_input_layout[0];
        draw_params_block_descriptor.input_name = "DrawParamsBlock";
        draw_params_block_descriptor.type = PipelineInputType::UniformBuffer;
        draw_params_block_descriptor.location = k_draw_params_ubo_slot;
        draw_params_block_descriptor.mandatory = true;
    }

    config.preamble_shader_code = { VertexShaderCode{ ui_vertex_shader },
                                    {},
                                    FragmentShaderCode{ ui_fragment_shader_preamble } };

    config.on_error_shader_code = { {}, {}, FragmentShaderCode{ ui_fragment_shader_fallback } };

    config.material_params_ubo_slot = k_material_params_ubo_slot;

    return PipelinePool(std::move(config));
}

Pipeline make_text_pipeline()
{
    Opt<VertexShaderHandle> vs = compile_vertex_shader(text_shader_vs);
    Opt<FragmentShaderHandle> fs = compile_fragment_shader(text_shader_fs);

    Pipeline::Params params;
    params.vertex_shader = vs.value();
    params.fragment_shader = fs.value();
    std::array<PipelineInputDescriptor, 2> input_descriptors;
    input_descriptors[0] = { "DrawParamsBlock", PipelineInputType::UniformBuffer, 0, true };
    input_descriptors[1] = { "font_texture", PipelineInputType::Sampler2D, 0, true };
    params.shared_input_layout = input_descriptors;

    return Pipeline::make(params).value();
}

mat4 make_transform_matrix(const UIPlacement& placement,
                           const vec2 scale,
                           const ivec2 resolution,
                           const float scaling_factor)
{
    const float aspect_ratio = static_cast<float>(resolution.x) / static_cast<float>(resolution.y);

    // Factors of 2.0f convert from [0.0, 1.0f] range into OpenGL's [-1.0f, 1.0f] range.
    const vec2 size{ 2.0f * scale.x * scaling_factor / static_cast<float>(resolution.y),
                     2.0f * scale.y * scaling_factor / static_cast<float>(resolution.y) };
    mat4 scale_matrix(1.0f);
    scale_matrix[0][0] = size.x;
    scale_matrix[1][1] = size.y;

    const vec2 anchor_offset = size * placement.anchor;

    vec2 position = 2.0f * (vec2{ placement.position.x, placement.position.y } +
                            (placement.position_pixel_offset * scaling_factor / vec2(resolution))) -
                    vec2{ 1.0f, 1.0f };

    position.x *= aspect_ratio;
    position -= anchor_offset;

    mat4 translation_matrix(1.0f);
    translation_matrix[3] = { position, 0.0f, 1.0f };

    mat4 pivot_matrix(1.0f);
    pivot_matrix[3] = vec4{ anchor_offset, 0.0f, 1.0f };

    mat4 neg_pivot_matrix(1.0f);
    neg_pivot_matrix[3] = vec4{ -anchor_offset, 0.0f, 1.0f };

    const auto rot = placement.rotation.radians();
    const float cos_rot = std::cos(rot);
    const float sin_rot = std::sin(rot);

    mat4 rot_matrix(1.0f);
    rot_matrix[0][0] = cos_rot;
    rot_matrix[1][0] = -sin_rot;
    rot_matrix[0][1] = sin_rot;
    rot_matrix[1][1] = cos_rot;

    // Apply pivot point.
    rot_matrix = pivot_matrix * rot_matrix * neg_pivot_matrix;

    mat4 aspect_matrix(1.0f);
    aspect_matrix[0][0] = 1.0f / aspect_ratio;

    return aspect_matrix * translation_matrix * rot_matrix * scale_matrix;
}

} // namespace

struct UIRenderer::Impl {
    PipelinePool pipeline_pool = make_ui_pipeline_factory();

    UniformBuffer draw_params_ubo{ sizeof(DrawParamsBlock) };

    BufferHandle quad_vbo;
    VertexArrayHandle quad_vao;

    ivec2 resolution = { 0, 0 };
    float scaling_factor = 1.0;

    Pipeline text_pipeline = make_text_pipeline();
};

UIRenderer::UIRenderer(const ivec2 resolution, const float scaling_factor)
{
    MG_GFX_DEBUG_GROUP("init UIRenderer")

    m_impl->resolution = resolution;
    m_impl->scaling_factor = scaling_factor;

    GLuint quad_vao_id = 0;
    GLuint quad_vbo_id = 0;

    glGenVertexArrays(1, &quad_vao_id);
    glBindVertexArray(quad_vao_id);

    glGenBuffers(1, &quad_vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_id);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr); // position
    glEnableVertexAttribArray(0);

    m_impl->quad_vao.set(quad_vao_id);
    m_impl->quad_vbo.set(quad_vbo_id);
}

UIRenderer::~UIRenderer()
{
    MG_GFX_DEBUG_GROUP("~UIRenderer")
    const auto quad_vbo_id = m_impl->quad_vbo.as_gl_id();
    const auto quad_vao_id = m_impl->quad_vao.as_gl_id();

    glDeleteBuffers(1, &quad_vbo_id);
    glDeleteVertexArrays(1, &quad_vao_id);
}

void UIRenderer::resolution(const ivec2 resolution)
{
    m_impl->resolution = resolution;
}
ivec2 UIRenderer::resolution() const
{
    return m_impl->resolution;
}

void UIRenderer::scaling_factor(const float scaling_factor)
{
    m_impl->scaling_factor = scaling_factor;
}
float UIRenderer::scaling_factor() const
{
    return m_impl->scaling_factor;
}

namespace {

void setup_material_pipeline(UIRenderer::Impl& data,
                             const mat4& M,
                             const Material& material,
                             Opt<BlendMode> blend_mode)
{
    DrawParamsBlock block = {};
    block.M = M;
    data.draw_params_ubo.set_data(byte_representation(block));

    std::array input_bindings = { PipelineInputBinding(k_draw_params_ubo_slot,
                                                       data.draw_params_ubo) };
    Pipeline::bind_shared_inputs(input_bindings);

    PipelineBindingContext binding_context;
    data.pipeline_pool.bind_material_pipeline(material,
                                              pipeline_settings(blend_mode),
                                              binding_context);
}

} // namespace

void UIRenderer::draw_rectangle(const UIPlacement& placement,
                                const vec2 size,
                                const Material& material,
                                Opt<BlendMode> blend_mode) noexcept
{
    MG_GFX_DEBUG_GROUP("UIRenderer::draw_rectangle");

    const mat4 M = make_transform_matrix(placement, size, m_impl->resolution, m_impl->scaling_factor);
    setup_material_pipeline(*m_impl, M, material, blend_mode);

    const auto quad_vao_id = m_impl->quad_vao.as_gl_id();

    glBindVertexArray(quad_vao_id);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

namespace {

void setup_text_pipeline(UIRenderer::Impl& data,
                         const IRenderTarget& render_target,
                         const PreparedText::GpuData& text_gpu_data,
                         const mat4& M,
                         const BlendMode blend_mode)
{
    DrawParamsBlock block = {};
    block.M = M;
    data.draw_params_ubo.set_data(byte_representation(block));

    Pipeline::Settings pipeline_settings = {};
    pipeline_settings.blending_enabled = true;
    pipeline_settings.blend_mode = blend_mode;
    pipeline_settings.depth_test_condition = DepthTestCondition::always;
    pipeline_settings.depth_write_enabled = false;
    pipeline_settings.colour_write_enabled = true;
    pipeline_settings.alpha_write_enabled = true;
    pipeline_settings.polygon_mode = PolygonMode::fill;
    pipeline_settings.culling_mode = CullingMode::back;
    pipeline_settings.target_framebuffer = render_target.handle();
    pipeline_settings.viewport_size = render_target.image_size();
    pipeline_settings.vertex_array = text_gpu_data.vertex_array;

    PipelineBindingContext binding_context;
    binding_context.bind_pipeline(data.text_pipeline, pipeline_settings);

    Pipeline::bind_shared_inputs({ PipelineInputBinding(0, data.draw_params_ubo),
                                   PipelineInputBinding(0, text_gpu_data.texture) });
}

} // namespace

void UIRenderer::draw_text(const IRenderTarget& render_target,
                           const UIPlacement& placement,
                           const PreparedText& text,
                           const float scale,
                           const BlendMode blend_mode) noexcept
{
    MG_GFX_DEBUG_GROUP("UIRenderer::draw_text");

    static constexpr size_t verts_per_char = 6;

    const mat4 M = make_transform_matrix(placement,
                                         { scale * text.width(), scale * text.height() },
                                         m_impl->resolution,
                                         m_impl->scaling_factor);

    setup_text_pipeline(*m_impl, render_target, text.gpu_data(), M, blend_mode);

    glDrawArrays(GL_TRIANGLES, 0, as<GLsizei>(verts_per_char * text.num_glyphs()));
}

void UIRenderer::drop_shaders() noexcept
{
    MG_GFX_DEBUG_GROUP("UIRenderer::drop_shaders")
    m_impl->pipeline_pool.drop_pipelines();
}

} // namespace Mg::gfx
