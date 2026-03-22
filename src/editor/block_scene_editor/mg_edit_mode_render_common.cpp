//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2026, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_edit_mode_render_common.h"

#include "../../core/gfx/mg_gl_debug.h"
#include "../../core/gfx/mg_shader.h"
#include "../../core/gfx/mg_opengl_loader_glad.h"

#include "mg/core/gfx/mg_camera.h"
#include "mg/core/gfx/mg_render_target.h"
#include "glm/gtx/transform.hpp"

#include <numeric>

namespace Mg {

namespace {

constexpr auto vs_code = R"(
#version 440 core
layout(location = 0) in vec3 vert_position;

layout(std140) uniform DrawParamsBlock {
    uniform vec4 colour;
    uniform mat4 MVP;
};

void main()
{
    vec4 pos = MVP * vec4(vert_position, 1.0);
    pos.z -= 0.0001; // Little bit of z bias to give nice outlines and avoid z-fighting
    gl_Position = pos;
}
)";

constexpr auto fs_code = R"(
#version 440 core

layout(std140) uniform DrawParamsBlock {
    uniform vec4 colour;
    uniform mat4 MVP;
};

layout(location = 0) out vec4 frag_colour;

void main()
{
    frag_colour = colour;
}
)";

class GlLineWidthSetter {
public:
    [[nodiscard]] explicit GlLineWidthSetter(float line_width)
    {
        glGetFloatv(GL_LINE_WIDTH, &m_old_line_width);
        glLineWidth(line_width);
    }

    ~GlLineWidthSetter() { glLineWidth(m_old_line_width); }

    MG_MAKE_NON_COPYABLE(GlLineWidthSetter);
    MG_MAKE_NON_MOVABLE(GlLineWidthSetter);

private:
    float m_old_line_width{};
};

} // namespace

gfx::Pipeline EditModeRenderCommon::make_pipeline()
{
    Opt<gfx::VertexShaderHandle::Owner> vs = gfx::compile_vertex_shader(vs_code);
    Opt<gfx::FragmentShaderHandle::Owner> fs = gfx::compile_fragment_shader(fs_code);

    gfx::Pipeline::Params params;
    params.vertex_shader = vs.value().handle;
    params.fragment_shader = fs.value().handle;
    std::array<gfx::PipelineInputDescriptor, 2> input_descriptors;
    input_descriptors[0] = {
        "DrawParamsBlock", gfx::PipelineInputDescriptor::Type::UniformBuffer, 0, true
    };
    params.shared_input_layout = input_descriptors;

    return gfx::Pipeline::make(params).value();
}

EditModeRenderCommon::MeshBuffers EditModeRenderCommon::make_cube_mesh()
{
    GLuint vao_id = 0;
    GLuint vbo_id = 0;
    GLuint ibo_id = 0;

    glGenVertexArrays(1, &vao_id);
    glGenBuffers(1, &vbo_id);
    glGenBuffers(1, &ibo_id);

    glBindVertexArray(vao_id);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);

    std::array<glm::vec3, 8> positions{ {
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
        { 1.0f, 0.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 1.0f, 1.0f },
    } };

    glBufferData(GL_ARRAY_BUFFER,
                 as<GLsizeiptr>(positions.size() * sizeof(positions[0])),
                 positions.data(),
                 GL_DYNAMIC_DRAW);

    std::array<uint16_t, 24> indices{ {
        // clang-format off
        0, 1, 1, 2, 2, 3, 3, 0,  // Horizontal lines around bottom
        0, 4, 1, 5, 2, 6, 3, 7,  // Vertical lines
        4, 5, 5, 6, 6, 7, 7, 4,  // Horizontal lines around top
        // clang-format on
    } };
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 as<GLsizeiptr>(indices.size() * sizeof(indices[0])),
                 indices.data(),
                 GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    MG_CHECK_GL_ERROR();

    return {
        gfx::VertexArrayHandle::Owner{ vao_id },
        gfx::BufferHandle::Owner{ vbo_id },
        gfx::BufferHandle::Owner{ ibo_id },
        int(indices.size()),
    };
}

EditModeRenderCommon::MeshBuffers EditModeRenderCommon::make_grid_mesh()
{
    GLuint vao_id = 0;
    GLuint vbo_id = 0;
    GLuint ibo_id = 0;

    glGenVertexArrays(1, &vao_id);
    glGenBuffers(1, &vbo_id);
    glGenBuffers(1, &ibo_id);

    glBindVertexArray(vao_id);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);

    std::vector<glm::vec3> positions;
    constexpr auto grid_radius = 3;
    for (int i = -grid_radius; i <= grid_radius + 1; ++i) {
        positions.emplace_back(i, -grid_radius, 0.0f);
        positions.emplace_back(i, grid_radius + 1, 0.0f);
        positions.emplace_back(-grid_radius, i, 0.0f);
        positions.emplace_back(grid_radius + 1, i, 0.0f);
    }

    glBufferData(GL_ARRAY_BUFFER,
                 as<GLsizeiptr>(positions.size() * sizeof(positions[0])),
                 positions.data(),
                 GL_DYNAMIC_DRAW);

    std::vector<uint16_t> indices;
    indices.resize(positions.size());
    std::iota(indices.begin(), indices.end(), 0u);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 as<GLsizeiptr>(indices.size() * sizeof(indices[0])),
                 indices.data(),
                 GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    MG_CHECK_GL_ERROR();

    return {
        gfx::VertexArrayHandle::Owner{ vao_id },
        gfx::BufferHandle::Owner{ vbo_id },
        gfx::BufferHandle::Owner{ ibo_id },
        int(indices.size()),
    };
}

void EditModeRenderCommon::draw_box_outline(const gfx::ICamera& cam,
                                            gfx::IRenderTarget& render_target,
                                            int coord_x,
                                            int coord_y,
                                            float z_min,
                                            float z_max,
                                            float block_size,
                                            glm::vec4 colour)
{
    gfx::PipelineBindingContext binding_context;

    auto box_outline_settings =
        configure_pipeline(cam,
                           render_target,
                           // position
                           glm::vec3{
                               float(coord_x) * block_size, float(coord_y) * block_size, z_min },
                           // scale
                           glm::vec3{ block_size, block_size, z_max - z_min },
                           // colour
                           colour,
                           m_cube_mesh.vao.handle);

    binding_context.bind_pipeline(m_pipeline, box_outline_settings);

    gfx::Pipeline::bind_shared_inputs(
        std::array{ gfx::PipelineInputBinding(0, m_draw_params_ubo) });

    GlLineWidthSetter lws{ 4.0 };
    glDrawElements(GL_LINES, m_cube_mesh.num_indices, GL_UNSIGNED_SHORT, nullptr);
}

void EditModeRenderCommon::draw_grid_around_point(const gfx::ICamera& cam,
                                                  gfx::IRenderTarget& render_target,
                                                  glm::ivec2 point,
                                                  float elevation,
                                                  float block_size)
{
    gfx::PipelineBindingContext binding_context;

    auto grid_settings = configure_pipeline(cam,
                                            render_target,
                                            // position
                                            glm::vec3{ float(point.x) * block_size,
                                                       float(point.y) * block_size,
                                                       elevation },
                                            // scale
                                            glm::vec3{ block_size, block_size, 1.0f },
                                            // colour
                                            glm::vec4(1.0f, 1.0f, 1.0f, 0.3f),
                                            m_grid_mesh.vao.handle);

    binding_context.bind_pipeline(m_pipeline, grid_settings);

    gfx::Pipeline::bind_shared_inputs(
        std::array{ gfx::PipelineInputBinding(0, m_draw_params_ubo) });

    GlLineWidthSetter lws{ 1.0 };
    glDrawElements(GL_LINES, m_grid_mesh.num_indices, GL_UNSIGNED_SHORT, nullptr);
}

gfx::Pipeline::Settings EditModeRenderCommon::configure_pipeline(const gfx::ICamera& cam,
                                                                 gfx::IRenderTarget& render_target,
                                                                 glm::vec3 position,
                                                                 glm::vec3 scale,
                                                                 glm::vec4 colour,
                                                                 gfx::VertexArrayHandle vao)
{
    DrawParamsBlock block = {};
    block.colour = colour;
    const auto translation_matrix = glm::translate(position);
    const auto scale_matrix = glm::scale(scale);
    block.MVP = cam.view_proj_matrix() * translation_matrix * scale_matrix;
    m_draw_params_ubo.set_data(byte_representation(block));

    gfx::Pipeline::Settings pipeline_settings = {};
    pipeline_settings.blending_enabled = true;
    pipeline_settings.blend_mode = gfx::blend_mode_constants::bm_alpha;
    pipeline_settings.depth_test_condition = gfx::DepthTestCondition::less;
    pipeline_settings.depth_write_enabled = true;
    pipeline_settings.colour_write_enabled = true;
    pipeline_settings.alpha_write_enabled = true;
    pipeline_settings.polygon_mode = gfx::PolygonMode::line;
    pipeline_settings.culling_mode = gfx::CullingMode::none;
    pipeline_settings.target_framebuffer = render_target.handle();
    pipeline_settings.viewport_size = render_target.image_size();
    pipeline_settings.vertex_array = vao;

    return pipeline_settings;
}

} // namespace Mg
