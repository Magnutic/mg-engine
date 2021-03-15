//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_ui_renderer.h"

#include "../mg_shader.h"

#include "mg/containers/mg_array.h"
#include "mg/gfx/mg_blend_modes.h"
#include "mg/gfx/mg_font_handler.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_pipeline_repository.h"
#include "mg/gfx/mg_uniform_buffer.h"

#include <glm/mat2x2.hpp>
#include <glm/mat4x4.hpp>

#include "mg_gl_debug.h"
#include <glad/glad.h>

namespace Mg::gfx {

namespace {

const std::array<float, 8> quad_vertices = { { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f } };

// Binding slots for UniformBufferObjects.
constexpr uint32_t k_draw_params_ubo_slot = 0;
constexpr uint32_t k_material_params_ubo_slot = 1;

struct DrawParamsBlock {
    glm::mat4 M;
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

PipelineRepository make_ui_pipeline_factory()
{
    PipelineRepository::Config config = {};

    config.shared_input_layout = Array<PipelineInputDescriptor>::make(1);
    config.shared_input_layout[0] = { "DrawParamsBlock",
                                      PipelineInputType::UniformBuffer,
                                      k_draw_params_ubo_slot };

    config.preamble_shader_code = { VertexShaderCode{ ui_vertex_shader },
                                    {},
                                    FragmentShaderCode{ ui_fragment_shader_preamble } };

    config.on_error_shader_code = { {}, {}, FragmentShaderCode{ ui_fragment_shader_fallback } };

    config.material_params_ubo_slot = k_material_params_ubo_slot;

    return PipelineRepository(std::move(config));
}

Pipeline make_text_pipeline()
{
    Opt<VertexShaderHandle> vs = compile_vertex_shader(text_shader_vs);
    Opt<FragmentShaderHandle> fs = compile_fragment_shader(text_shader_fs);

    Pipeline::Params params;
    params.vertex_shader = vs.value();
    params.fragment_shader = fs.value();
    std::array<PipelineInputDescriptor, 2> inputs;
    inputs[0] = { "DrawParamsBlock", PipelineInputType::UniformBuffer, 0 };
    inputs[1] = { "font_texture", PipelineInputType::Sampler2D, 0 };
    params.shared_input_layout = inputs;

    return Pipeline::make(params).value();
}

glm::mat4 transform_params_to_matrix(const glm::vec2 scale,
                                     const UIRenderer::TransformParams& transform_params,
                                     const glm::ivec2 resolution,
                                     const float scaling_factor)
{
    const float aspect_ratio = static_cast<float>(resolution.x) / static_cast<float>(resolution.y);

    // Factors of 2.0f convert from [0.0, 1.0f] range into OpenGL's [-1.0f, 1.0f] range.
    const glm::vec2 size{ 2.0f * scale.x * scaling_factor / static_cast<float>(resolution.y),
                          2.0f * scale.y * scaling_factor / static_cast<float>(resolution.y) };
    glm::mat4 scale_matrix(1.0f);
    scale_matrix[0][0] = size.x;
    scale_matrix[1][1] = size.y;

    const glm::vec2 anchor_offset = size * transform_params.anchor;

    glm::vec2 position =
        2.0f * (glm::vec2{ transform_params.position.x, transform_params.position.y } +
                (transform_params.position_pixel_offset / glm::vec2(resolution))) -
        glm::vec2{ 1.0f, 1.0f };

    position.x *= aspect_ratio;
    position -= anchor_offset;

    glm::mat4 translation_matrix(1.0f);
    translation_matrix[3] = { position, 0.0f, 1.0f };

    glm::mat4 pivot_matrix(1.0f);
    pivot_matrix[3] = glm::vec4{ anchor_offset, 0.0f, 1.0f };

    glm::mat4 neg_pivot_matrix(1.0f);
    neg_pivot_matrix[3] = glm::vec4{ -anchor_offset, 0.0f, 1.0f };

    const auto rot = transform_params.rotation.radians();
    const float cos_rot = std::cos(rot);
    const float sin_rot = std::sin(rot);

    glm::mat4 rot_matrix(1.0f);
    rot_matrix[0][0] = cos_rot;
    rot_matrix[1][0] = -sin_rot;
    rot_matrix[0][1] = sin_rot;
    rot_matrix[1][1] = cos_rot;

    // Apply pivot point.
    rot_matrix = pivot_matrix * rot_matrix * neg_pivot_matrix;

    glm::mat4 aspect_matrix(1.0f);
    aspect_matrix[0][0] = 1.0f / aspect_ratio;

    return aspect_matrix * translation_matrix * rot_matrix * scale_matrix;
}

} // namespace

namespace detail {
struct UIRendererData {
    PipelineRepository pipeline_repository = make_ui_pipeline_factory();

    FontHandler font_handler;

    UniformBuffer draw_params_ubo{ sizeof(DrawParamsBlock) };

    BufferHandle quad_vbo;
    VertexArrayHandle quad_vao;

    glm::ivec2 resolution = { 0, 0 };
    float scaling_factor = 1.0;

    Pipeline text_pipeline = make_text_pipeline();
};
} // namespace detail

using detail::UIRendererData;

UIRenderer::UIRenderer(const glm::ivec2 resolution, const float scaling_factor)
{
    MG_GFX_DEBUG_GROUP("init UIRenderer")

    impl().resolution = resolution;
    impl().scaling_factor = scaling_factor;

    GLuint quad_vao_id = 0;
    GLuint quad_vbo_id = 0;

    glGenVertexArrays(1, &quad_vao_id);
    glBindVertexArray(quad_vao_id);

    glGenBuffers(1, &quad_vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_id);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr); // position
    glEnableVertexAttribArray(0);

    impl().quad_vao.set(quad_vao_id);
    impl().quad_vbo.set(quad_vbo_id);
}

UIRenderer::~UIRenderer()
{
    MG_GFX_DEBUG_GROUP("~UIRenderer")
    const auto quad_vbo_id = narrow<GLuint>(impl().quad_vbo.get());
    const auto quad_vao_id = narrow<GLuint>(impl().quad_vao.get());

    glDeleteBuffers(1, &quad_vbo_id);
    glDeleteVertexArrays(1, &quad_vao_id);
}

void UIRenderer::resolution(const glm::ivec2 resolution)
{
    impl().resolution = resolution;
}
glm::ivec2 UIRenderer::resolution() const
{
    return impl().resolution;
}

void UIRenderer::scaling_factor(const float scaling_factor)
{
    impl().scaling_factor = scaling_factor;
}
float UIRenderer::scaling_factor() const
{
    return impl().scaling_factor;
}

FontHandler& UIRenderer::font_handler()
{
    return impl().font_handler;
}
const FontHandler& UIRenderer::font_handler() const
{
    return impl().font_handler;
}

namespace {
void setup_material_pipeline(UIRendererData& data,
                             const glm::mat4& M,
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
    data.pipeline_repository.bind_material_pipeline(material,
                                                    pipeline_settings(blend_mode),
                                                    binding_context);
}
} // namespace

void UIRenderer::draw_rectangle(const glm::vec2 size,
                                const TransformParams& params,
                                const Material& material,
                                Opt<BlendMode> blend_mode) noexcept
{
    MG_GFX_DEBUG_GROUP("UIRenderer::draw_rectangle");

    const glm::mat4 M =
        transform_params_to_matrix(size, params, impl().resolution, impl().scaling_factor);
    setup_material_pipeline(impl(), M, material, blend_mode);

    const auto quad_vao_id = narrow<GLuint>(impl().quad_vao.get());

    glBindVertexArray(quad_vao_id);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

namespace {
Pipeline::Settings text_pipeline_settings(const BlendMode blend_mode)
{
    Pipeline::Settings settings = {};
    settings.blending_enabled = true;
    settings.blend_mode = blend_mode;
    settings.depth_test_condition = DepthTestCondition::always;
    settings.depth_write_enabled = false;
    settings.colour_write_enabled = true;
    settings.alpha_write_enabled = true;
    settings.polygon_mode = PolygonMode::fill;
    settings.culling_mode = CullingMode::back;
    return settings;
}

void setup_text_pipeline(UIRendererData& data,
                         const TextureHandle texture,
                         const glm::mat4& M,
                         const BlendMode blend_mode)
{
    DrawParamsBlock block = {};
    block.M = M;
    data.draw_params_ubo.set_data(byte_representation(block));

    std::array input_bindings = { PipelineInputBinding(0, data.draw_params_ubo),
                                  PipelineInputBinding(0, texture) };

    PipelineBindingContext binding_context;
    binding_context.bind_pipeline(data.text_pipeline, text_pipeline_settings(blend_mode));
    Pipeline::bind_shared_inputs(input_bindings);
}
} // namespace

void UIRenderer::draw_text(const PreparedText& text,
                           const TransformParams& transform_params,
                           const float scale,
                           const BlendMode blend_mode) noexcept
{
    MG_GFX_DEBUG_GROUP("UIRenderer::draw_text");

    static constexpr size_t verts_per_char = 6;

    const glm::mat4 M = transform_params_to_matrix({ scale * text.width(), scale * text.height() },
                                                   transform_params,
                                                   impl().resolution,
                                                   impl().scaling_factor);

    setup_text_pipeline(impl(), text.gpu_data().texture, M, blend_mode);

    glBindVertexArray(narrow<GLuint>(text.gpu_data().vertex_array.get()));
    glDrawArrays(GL_TRIANGLES, 0, narrow<GLsizei>(verts_per_char * text.num_glyphs()));
}

void UIRenderer::drop_shaders() noexcept
{
    MG_GFX_DEBUG_GROUP("UIRenderer::drop_shaders")
    impl().pipeline_repository.drop_pipelines();
}

} // namespace Mg::gfx
