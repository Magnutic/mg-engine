//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_ui_renderer.h"

#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_pipeline_repository.h"
#include "mg/gfx/mg_uniform_buffer.h"

#include <glm/mat2x2.hpp>
#include <glm/mat4x4.hpp>

#include <plf_colony.h>

#include "mg_gl_debug.h"
#include <glad/glad.h>

#include <iostream>

namespace Mg::gfx {

namespace {

const std::array<float, 12> quad_vertices = {
    { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f }
};

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

//--------------------------------------------------------------------------------------------------

namespace {
Pipeline::Settings pipeline_settings(Opt<BlendMode> bm)
{
    Pipeline::Settings settings;
    settings.depth_test_condition = DepthTestCondition::always;
    settings.blending_enabled = true;
    settings.depth_write_enabled = false;

    if (bm) {
        settings.blend_mode = bm.value();
        settings.blending_enabled = true;
    }
    else {
        settings.blending_enabled = false;
    }

    return settings;
}
} // namespace

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

} // namespace

struct UIRendererData {
    PipelineRepository pipeline_repository = make_ui_pipeline_factory();
    UniformBuffer draw_params_ubo{ sizeof(DrawParamsBlock) };
    BufferHandle vbo;
    VertexArrayHandle vao;
    float aspect_ratio = 1.0f;
};

namespace {

glm::mat4x4 transform_params_to_matrix(const UIRenderer::TransformParams& transform_params,
                                       const float aspect_ratio)
{
    // Factors of 2.0f convert from [0.0, 1.0f] range into OpenGL's [-1.0f, 1.0f] range.
    const glm::vec2 size{ 2.0f * transform_params.size.x, 2.0f * transform_params.size.y };
    glm::mat4 scale_matrix(1.0f);
    scale_matrix[0][0] = size.x;
    scale_matrix[1][1] = size.y;

    const glm::vec2 anchor_offset = size * transform_params.anchor;

    glm::vec2 position = glm::vec2{ 2.0f * transform_params.position.x,
                                    2.0f * transform_params.position.y } -
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

    glm::mat4x4 aspect_matrix(1.0f);
    aspect_matrix[0][0] = 1.0f / aspect_ratio;

    return aspect_matrix * translation_matrix * rot_matrix * scale_matrix;
}

void setup_render_pipeline(UIRendererData& data,
                           const UIRenderer::TransformParams& params,
                           const Material& material,
                           Opt<BlendMode> blend_mode)
{
    DrawParamsBlock block = {};
    block.M = transform_params_to_matrix(params, data.aspect_ratio);
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

UIRenderer::UIRenderer()
{
    MG_GFX_DEBUG_GROUP("init UIRenderer")

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

    impl().vao.set(vao_id);
    impl().vbo.set(vbo_id);
}

UIRenderer::~UIRenderer()
{
    MG_GFX_DEBUG_GROUP("~UIRenderer")
    const auto vbo_id = narrow<GLuint>(impl().vbo.get());
    const auto vao_id = narrow<GLuint>(impl().vao.get());

    glDeleteBuffers(1, &vbo_id);
    glDeleteVertexArrays(1, &vao_id);
}

void UIRenderer::aspect_ratio(const float aspect_ratio)
{
    impl().aspect_ratio = aspect_ratio;
}
float UIRenderer::aspect_ratio() const
{
    return impl().aspect_ratio;
}

void UIRenderer::draw_rectangle(const TransformParams& params,
                                const Material* material,
                                Opt<BlendMode> blend_mode) noexcept
{
    MG_GFX_DEBUG_GROUP("UIRenderer::draw_rectangle");
    MG_ASSERT(material);

    setup_render_pipeline(impl(), params, *material, blend_mode);

    const auto vao_id = narrow<GLuint>(impl().vao.get());

    glBindVertexArray(vao_id);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void UIRenderer::drop_shaders() noexcept
{
    MG_GFX_DEBUG_GROUP("UIRenderer::drop_shaders")
    impl().pipeline_repository.drop_pipelines();
}

} // namespace Mg::gfx
