//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/scene/mg_block_scene.h"

#include <cmath>
#include <map>
#include <numeric>
#include <type_traits>

#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_bitmap_font.h"
#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_pipeline.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/gfx/mg_ui_renderer.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/input/mg_input.h"
#include "mg/input/mg_input_source.h"
#include "mg/mg_unicode.h"
#include "mg/resources/mg_font_resource.h"
#include "mg/utils/mg_math_utils.h"

#include "../gfx/mg_shader.h"
#include "../gfx/opengl/mg_gl_debug.h"
#include "../gfx/opengl/mg_glad.h"

#define MG_DEBUG_RENDERING 1
#if MG_DEBUG_RENDERING
#    include "mg/gfx/mg_debug_renderer.h"
#endif

namespace Mg {

using glm::vec2, glm::vec3, glm::vec4, glm::mat4;

namespace {

const char* vs_code = R"(
    #version 440 core
    layout(location = 0) in vec3 vert_position;

    layout(std140) uniform DrawParamsBlock {
        uniform vec4 colour;
        uniform mat4 MVP;
    };

    void main()
    {
        vec4 pos = MVP * vec4(vert_position, 1.0);
        pos.z -= 0.001; // Little bit of z bias to give nice outlines and avoid z-fighting
        gl_Position = pos;
    }
)";

const char* fs_code = R"(
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

// RAII owner for OpenGL mesh buffers
struct MeshBuffers {
    gfx::VertexArrayHandle::Owner vao;
    gfx::BufferHandle::Owner vbo;
    gfx::BufferHandle::Owner ibo;

    int num_indices;
};

MeshBuffers make_cube_mesh()
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

    std::array<vec3, 8> positions{ {
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

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), nullptr);
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

MeshBuffers make_grid_mesh()
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

    std::vector<vec3> positions;
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

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), nullptr);
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

// Block of shader uniforms.
struct DrawParamsBlock {
    glm::vec4 colour;
    glm::mat4 MVP;
};

gfx::Pipeline make_pipeline()
{
    Opt<gfx::VertexShaderHandle::Owner> vs = gfx::compile_vertex_shader(vs_code);
    Opt<gfx::FragmentShaderHandle::Owner> fs = gfx::compile_fragment_shader(fs_code);

    gfx::Pipeline::Params params;
    params.vertex_shader = vs.value().handle;
    params.fragment_shader = fs.value().handle;
    std::array<gfx::PipelineInputDescriptor, 2> input_descriptors;
    input_descriptors[0] = { "DrawParamsBlock", gfx::PipelineInputType::UniformBuffer, 0, true };
    params.shared_input_layout = input_descriptors;

    return gfx::Pipeline::make(params).value();
}

bool was_pressed(const input::ButtonState& s)
{
    return s.was_pressed;
}
bool is_held(const input::ButtonState& s)
{
    return s.is_held;
}

struct EditModeUpdateArgs {
    glm::vec3 view_position;
    glm::vec3 view_angle;
    const input::ButtonTracker::ButtonStates& button_states;
    glm::vec2 mouse_scroll_delta;
};

class IEditMode {
public:
    MG_INTERFACE_BOILERPLATE(IEditMode);

    virtual void render(const gfx::Camera& cam,
                        const BlockScene& scene,
                        const gfx::IRenderTarget& render_target) = 0;
    virtual bool update(BlockScene& scene, const EditModeUpdateArgs& data) = 0;
};

template<std::invocable<int, int, float, float> F>
void try_blocks_on_grid(const glm::vec3 start,
                        const Rotation rotation,
                        F&& function,
                        const float block_size,
                        const float max_distance)
{
    const auto v = glm::normalize(rotation.forward());

    const int step_x = v.x > 0.0f ? 1 : -1;
    const int step_y = v.y > 0.0f ? 1 : -1;

    const auto t_delta_x = std::abs(v.x) > FLT_EPSILON ? std::abs(1.0f / v.x) : FLT_MAX;
    const auto t_delta_y = std::abs(v.y) > FLT_EPSILON ? std::abs(1.0f / v.y) : FLT_MAX;

    const auto start_in_grid_units = start / block_size;

    auto cell_x = int(std::floor(start_in_grid_units.x));
    auto cell_y = int(std::floor(start_in_grid_units.y));

    auto t_max_x = v.x < 0.0f ? (start_in_grid_units.x - float(cell_x)) * t_delta_x
                              : (1.0f - start_in_grid_units.x + float(cell_x)) * t_delta_x;
    auto t_max_y = v.y < 0.0f ? (start_in_grid_units.y - float(cell_y)) * t_delta_y
                              : (1.0f - start_in_grid_units.y + float(cell_y)) * t_delta_y;

    auto cell_entry_t = 0.0f;
    for (;;) {
        if (cell_entry_t * block_size > max_distance) {
            return;
        }

        const auto cell_exit_t = std::min(t_max_x, t_max_y);
        const auto z_on_cell_entry = start.z + (cell_entry_t * block_size * v).z;
        const auto z_on_cell_exit = start.z + (cell_exit_t * block_size * v).z;
        if (function(cell_x,
                     cell_y,
                     std::min(z_on_cell_entry, z_on_cell_exit),
                     std::max(z_on_cell_entry, z_on_cell_exit))) {
            return;
        }

        if (t_max_x < t_max_y) {
            cell_entry_t = t_max_x;
            t_max_x += t_delta_x;
            cell_x += step_x;
        }
        else {
            cell_entry_t = t_max_y;
            t_max_y += t_delta_y;
            cell_y += step_y;
        }
    }
}

class EditModeAdd : public IEditMode {
public:
    EditModeAdd() : m_cube_mesh{ make_cube_mesh() }, m_grid_mesh{ make_grid_mesh() } {}

    void render(const gfx::Camera& cam,
                const BlockScene& scene,
                const gfx::IRenderTarget& render_target) override
    {
        const auto block_size = scene.block_size();

        auto configure_pipeline = [&](glm::vec3 position,
                                      glm::vec3 scale,
                                      glm::vec4 colour,
                                      gfx::VertexArrayHandle vao) -> gfx::Pipeline::Settings {
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
        };

        gfx::PipelineBindingContext binding_context;

        float old_line_width = 0.0f;
        glGetFloatv(GL_LINE_WIDTH, &old_line_width);

        auto draw_box_outline = [&](int coord_x, int coord_y, float z_min, float z_max) {
            auto box_outline_settings = configure_pipeline(
                // position
                vec3{ float(coord_x) * block_size, float(coord_y) * block_size, z_min },
                // scale
                glm::vec3{ block_size, block_size, z_max - z_min },
                // colour
                glm::vec4(0.25f, 1.0f, 0.25f, 1.0f),
                m_cube_mesh.vao.handle);

            binding_context.bind_pipeline(m_pipeline, box_outline_settings);

            gfx::Pipeline::bind_shared_inputs(
                std::array{ gfx::PipelineInputBinding(0, m_draw_params_ubo) });

            glLineWidth(4.0);

            glDrawElements(GL_LINES, m_cube_mesh.num_indices, GL_UNSIGNED_SHORT, nullptr);
        };

        std::vector<Block> blocks;
        try_blocks_on_grid(
            cam.get_position(),
            cam.rotation,
            [&](int x, int y, float z1, float z2) {
                blocks.clear();
                scene.get_blocks_at(x, y, blocks);
                for (auto& block : blocks) {
                    if (block.z_max >= z1 && block.z_min <= z2) {
                        draw_box_outline(x, y, block.z_min, block.z_max);
                        return true;
                    }
                }
                return false;
            },
            block_size,
            500.0f);

        if (m_coord.has_value()) {
            // Draw grid
            auto grid_settings = configure_pipeline(
                // position
                vec3{ float(m_coord->x) * block_size,
                      float(m_coord->y) * block_size,
                      m_new_block_z_min },
                // scale
                glm::vec3{ block_size, block_size, m_new_block_z_max - m_new_block_z_min },
                // colour
                glm::vec4(0.5f, 0.5f, 0.5f, 1.0f),
                m_grid_mesh.vao.handle);
            binding_context.bind_pipeline(m_pipeline, grid_settings);

            gfx::Pipeline::bind_shared_inputs(
                std::array{ gfx::PipelineInputBinding(0, m_draw_params_ubo) });

            glLineWidth(2.0);
            glDrawElements(GL_LINES, m_grid_mesh.num_indices, GL_UNSIGNED_SHORT, nullptr);
        }

        glLineWidth(old_line_width);

        glBindVertexArray(0);
    }

    bool update(BlockScene& scene, const EditModeUpdateArgs& data) override
    {
        const bool modify = data.button_states.try_get("modify").map_or(is_held, false);

        const auto block_size = scene.block_size();

        if (std::abs(data.view_angle.z) < FLT_EPSILON) {
            return false;
        }

        if (data.mouse_scroll_delta.y >= 1.0f) {
            m_new_block_z_max += 1.0f;
            if (!modify && m_new_block_z_min + 1.0f < m_new_block_z_max) {
                m_new_block_z_min += 1.0f;
            }
        }
        if (data.mouse_scroll_delta.y <= -1.0f) {
            if (!modify) {
                m_new_block_z_min -= 1.0f;
            }
            if (m_new_block_z_min + 1.0 < m_new_block_z_max) {
                m_new_block_z_max -= 1.0f;
            }
        }

        m_coord = nullopt;
        m_cursor_target = get_target_position(data);
        if (m_cursor_target) {
            m_coord = { narrow_cast<int>(std::floor(m_cursor_target->x / block_size)),
                        narrow_cast<int>(std::floor(m_cursor_target->y / block_size)) };
        }

        if (m_coord && data.button_states.try_get("interact").map_or(was_pressed, false)) {
            return scene.try_insert(m_coord->x,
                                    m_coord->y,
                                    {
                                        .z_min = m_new_block_z_min,
                                        .z_max = m_new_block_z_max,
                                    });
        }

        return false;
    }

    Opt<vec3> get_target_position(const EditModeUpdateArgs& data) const
    {
        if (data.view_angle.z == 0.0f) {
            return nullopt;
        }

        const float distance = (m_new_block_z_min - data.view_position.z) / data.view_angle.z;
        if (distance <= 0.0f) {
            return nullopt;
        }

        return data.view_position + data.view_angle * distance;
    }

private:
    MeshBuffers m_cube_mesh;
    MeshBuffers m_grid_mesh;
    gfx::UniformBuffer m_draw_params_ubo{ sizeof(DrawParamsBlock) };
    gfx::Pipeline m_pipeline = make_pipeline();

    float m_new_block_z_min = 0.0f;
    float m_new_block_z_max = 1.0f;
    Opt<glm::vec3> m_cursor_target;
    Opt<glm::ivec2> m_coord;
};

class EditModeEdit : public IEditMode {
public:
    void render(const gfx::Camera& cam,
                const BlockScene& scene,
                const gfx::IRenderTarget& render_target) override
    {}

    bool update(BlockScene& scene, const EditModeUpdateArgs& data) override { return false; }
};


} // namespace

BlockSceneMesh BlockScene::make_mesh() const
{
    BlockSceneMesh result;

    auto add_block_to_mesh = [&](const Block block, int x, int y) {
        if (block.z_min >= block.z_max) {
            return;
        }

        auto start_index = as<uint32_t>(result.m_vertices.size());
        const auto x1 = float(x) * m_block_size;
        const auto y1 = float(y) * m_block_size;
        const auto x2 = x1 + m_block_size;
        const auto y2 = y1 + m_block_size;

        // Bottom
        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x1, y1, block.z_min);
            v.tex_coord = glm::vec2(0.0f, 0.0f);
            v.normal = glm::vec3(0.0f, 0.0f, -1.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x2, y1, block.z_min);
            v.tex_coord = glm::vec2(1.0f, 0.0f);
            v.normal = glm::vec3(0.0f, 0.0f, -1.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x1, y2, block.z_min);
            v.tex_coord = glm::vec2(0.0f, 1.0f);
            v.normal = glm::vec3(0.0f, 0.0f, -1.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x2, y2, block.z_min);
            v.tex_coord = glm::vec2(1.0f, 1.0f);
            v.normal = glm::vec3(0.0f, 0.0f, -1.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
        }

        result.m_indices.push_back(start_index);
        result.m_indices.push_back(start_index + 2);
        result.m_indices.push_back(start_index + 1);
        result.m_indices.push_back(start_index + 2);
        result.m_indices.push_back(start_index + 3);
        result.m_indices.push_back(start_index + 1);

        start_index += 4;

        // Top
        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x1, y1, block.z_max);
            v.tex_coord = glm::vec2(0.0f, 0.0f);
            v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x2, y1, block.z_max);
            v.tex_coord = glm::vec2(1.0f, 0.0f);
            v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x1, y2, block.z_max);
            v.tex_coord = glm::vec2(0.0f, 1.0f);
            v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x2, y2, block.z_max);
            v.tex_coord = glm::vec2(1.0f, 1.0f);
            v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
        }

        result.m_indices.push_back(start_index);
        result.m_indices.push_back(start_index + 1);
        result.m_indices.push_back(start_index + 2);
        result.m_indices.push_back(start_index + 2);
        result.m_indices.push_back(start_index + 1);
        result.m_indices.push_back(start_index + 3);

        start_index += 4;

        const float uv_top = (block.z_max - block.z_min) / m_block_size;

        // -X
        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x1, y1, block.z_min);
            v.tex_coord = glm::vec2(0.0f, 0.0f);
            v.normal = glm::vec3(-1.0f, 0.0f, 0.0f);
            v.tangent = glm::vec3(0.0f, 1.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x1, y2, block.z_min);
            v.tex_coord = glm::vec2(1.0f, 0.0f);
            v.normal = glm::vec3(-1.0f, 0.0f, 0.0f);
            v.tangent = glm::vec3(0.0f, 1.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x1, y1, block.z_max);
            v.tex_coord = glm::vec2(0.0f, uv_top);
            v.normal = glm::vec3(-1.0f, 0.0f, 0.0f);
            v.tangent = glm::vec3(0.0f, 1.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x1, y2, block.z_max);
            v.tex_coord = glm::vec2(1.0f, uv_top);
            v.normal = glm::vec3(-1.0f, 0.0f, 0.0f);
            v.tangent = glm::vec3(0.0f, 1.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        result.m_indices.push_back(start_index);
        result.m_indices.push_back(start_index + 2);
        result.m_indices.push_back(start_index + 1);
        result.m_indices.push_back(start_index + 2);
        result.m_indices.push_back(start_index + 3);
        result.m_indices.push_back(start_index + 1);

        start_index += 4;

        // +X
        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x2, y1, block.z_min);
            v.tex_coord = glm::vec2(0.0f, 0.0f);
            v.normal = glm::vec3(1.0f, 0.0f, 0.0f);
            v.tangent = glm::vec3(0.0f, 1.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x2, y2, block.z_min);
            v.tex_coord = glm::vec2(1.0f, 0.0f);
            v.normal = glm::vec3(1.0f, 0.0f, 0.0f);
            v.tangent = glm::vec3(0.0f, 1.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x2, y1, block.z_max);
            v.tex_coord = glm::vec2(0.0f, uv_top);
            v.normal = glm::vec3(1.0f, 0.0f, 0.0f);
            v.tangent = glm::vec3(0.0f, 1.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x2, y2, block.z_max);
            v.tex_coord = glm::vec2(1.0f, uv_top);
            v.normal = glm::vec3(1.0f, 0.0f, 0.0f);
            v.tangent = glm::vec3(0.0f, 1.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        result.m_indices.push_back(start_index);
        result.m_indices.push_back(start_index + 1);
        result.m_indices.push_back(start_index + 2);
        result.m_indices.push_back(start_index + 2);
        result.m_indices.push_back(start_index + 1);
        result.m_indices.push_back(start_index + 3);

        start_index += 4;

        // -Y
        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x1, y1, block.z_min);
            v.tex_coord = glm::vec2(0.0f, 0.0f);
            v.normal = glm::vec3(0.0f, -1.0f, 0.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x2, y1, block.z_min);
            v.tex_coord = glm::vec2(1.0f, 0.0f);
            v.normal = glm::vec3(0.0f, -1.0f, 0.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x1, y1, block.z_max);
            v.tex_coord = glm::vec2(0.0f, uv_top);
            v.normal = glm::vec3(0.0f, -1.0f, 0.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x2, y1, block.z_max);
            v.tex_coord = glm::vec2(1.0f, uv_top);
            v.normal = glm::vec3(0.0f, -1.0f, 0.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        result.m_indices.push_back(start_index);
        result.m_indices.push_back(start_index + 1);
        result.m_indices.push_back(start_index + 2);
        result.m_indices.push_back(start_index + 2);
        result.m_indices.push_back(start_index + 1);
        result.m_indices.push_back(start_index + 3);

        start_index += 4;

        // +Y
        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x1, y2, block.z_min);
            v.tex_coord = glm::vec2(0.0f, 0.0f);
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x2, y2, block.z_min);
            v.tex_coord = glm::vec2(1.0f, 0.0f);
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x1, y2, block.z_max);
            v.tex_coord = glm::vec2(0.0f, uv_top);
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        {
            auto& v = result.m_vertices.emplace_back();
            v.position = glm::vec3(x2, y2, block.z_max);
            v.tex_coord = glm::vec2(1.0f, uv_top);
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        result.m_indices.push_back(start_index);
        result.m_indices.push_back(start_index + 2);
        result.m_indices.push_back(start_index + 1);
        result.m_indices.push_back(start_index + 2);
        result.m_indices.push_back(start_index + 3);
        result.m_indices.push_back(start_index + 1);

        start_index += 4;
    };

    for (const auto& [cluster_coord, cluster] : m_clusters) {
        const auto cluster_x_min = cluster_coord.x * int(k_cluster_size);
        const auto cluster_y_min = cluster_coord.y * int(k_cluster_size);

        for (const auto& level : cluster.block_levels) {
            for (int y = 0; y < int(k_cluster_size); ++y) {
                for (int x = 0; x < int(k_cluster_size); ++x) {
                    const Block block = level[index_for_block(x, y)];
                    add_block_to_mesh(block, cluster_x_min + x, cluster_y_min + y);
                }
            }
        }
    }

    result.m_submeshes.push_back(gfx::mesh_data::Submesh{
        .index_range{
            0u,
            as<uint32_t>(result.m_indices.size()),
        },
        .name = "BlockScene",
        .material_binding_id = "SceneMaterial0",
    });

    return result;
}

struct BlockSceneEditor::Impl {
    enum class Mode { Add, Edit, Texture };

    Impl(BlockScene& scene_,
         input::IInputSource& input_source,
         ResourceHandle<FontResource> font_resource)
        : scene{ scene_ }, button_tracker{ input_source }, scroll_tracker{ input_source }
    {
        auto unicode_ranges = { get_unicode_range(UnicodeBlock::Basic_Latin) };
        font = std::make_unique<gfx::BitmapFont>(font_resource, 24, unicode_ranges);
        edit_modes[Mode::Add] = std::make_unique<EditModeAdd>();
        edit_modes[Mode::Edit] = std::make_unique<EditModeEdit>();
        button_tracker.bind("mode_add", input::Key::Num1);
        button_tracker.bind("mode_edit", input::Key::Num2);
        button_tracker.bind("interact", input::MouseButton::left);
        button_tracker.bind("modify", input::Key::LeftShift);
    }

    BlockScene& scene;
    BlockSceneMesh mesh;

    input::ButtonTracker button_tracker;
    input::ScrollTracker scroll_tracker;

    std::unique_ptr<gfx::BitmapFont> font;

    std::map<Mode, std::unique_ptr<IEditMode>> edit_modes;
    Mode current_edit_mode{ Mode::Add };
};

BlockSceneEditor::BlockSceneEditor(BlockScene& scene,
                                   input::IInputSource& input_source,
                                   ResourceHandle<FontResource> font_resource)
    : m_impl{ scene, input_source, font_resource }
{}

void BlockSceneEditor::render(const gfx::Camera& cam, const gfx::IRenderTarget& render_target)
{
    auto& edit_mode = *m_impl->edit_modes[m_impl->current_edit_mode];
    edit_mode.render(cam, m_impl->scene, render_target);
}

void BlockSceneEditor::render_ui(const gfx::IRenderTarget& render_target,
                                 gfx::UIRenderer& ui_renderer)
{
    auto text = m_impl->font->prepare_text("Block Scene Editor", gfx::TypeSetting{});
    ui_renderer.draw_text(render_target,
                          gfx::UIPlacement{ { 0.5f, 0.0f }, { 0.0f, 20.0f }, {}, { 0.5f, 0.5f } },
                          text);
    auto cross = m_impl->font->prepare_text("+", gfx::TypeSetting{});
    ui_renderer.draw_text(render_target,
                          gfx::UIPlacement{ { 0.5f, 0.5f }, { 0.0f, 0.0f }, {}, { 0.5f, 0.5f } },
                          cross);
}

bool BlockSceneEditor::update(const glm::vec3& view_position, const glm::vec3& view_angle)
{
    const auto button_states = m_impl->button_tracker.get_button_events();
    const auto mouse_scroll_delta = m_impl->scroll_tracker.scroll_delta();

    if (button_states.try_get("mode_add").map_or(was_pressed, false)) {
        m_impl->current_edit_mode = Impl::Mode::Add;
    }
    else if (button_states.try_get("mode_edit").map_or(was_pressed, false)) {
        m_impl->current_edit_mode = Impl::Mode::Edit;
    }

    auto& edit_mode = *m_impl->edit_modes[m_impl->current_edit_mode];
    return edit_mode.update(m_impl->scene,
                            EditModeUpdateArgs{
                                .view_position = view_position,
                                .view_angle = view_angle,
                                .button_states = button_states,
                                .mouse_scroll_delta = mouse_scroll_delta,
                            });
}

} // namespace Mg
