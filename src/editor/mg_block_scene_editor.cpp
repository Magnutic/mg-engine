//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/editor/mg_block_scene_editor.h"

#include "mg/core/gfx/mg_camera.h"
#include "mg/core/gfx/mg_pipeline.h"
#include "mg/core/gfx/mg_render_target.h"
#include "mg/core/gfx/mg_ui_renderer.h"
#include "mg/core/gfx/mg_uniform_buffer.h"
#include "mg/core/gfx/render_passes/mg_ui_pass.h"
#include "mg/core/input/mg_input.h"
#include "mg/utils/mg_math_utils.h"

#include "../core/gfx/mg_gl_debug.h"
#include "../core/gfx/mg_shader.h"
#include "../core/gfx/mg_opengl_loader_glad.h"

#include <numeric>

namespace Mg {

namespace {

constexpr float k_vertical_step = 0.25f;

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
        pos.z -= 0.0001; // Little bit of z bias to give nice outlines and avoid z-fighting
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
    input_descriptors[0] = {
        "DrawParamsBlock", gfx::PipelineInputDescriptor::Type::UniformBuffer, 0, true
    };
    params.shared_input_layout = input_descriptors;

    return gfx::Pipeline::make(params).value();
}

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

struct SelectedBlock {
    int x;
    int y;
    float z_min;
    float z_max;
};

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

class EditModeRenderCommon {
public:
    void draw_box_outline(const gfx::ICamera& cam,
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
                               glm::vec3{ float(coord_x) * block_size,
                                          float(coord_y) * block_size,
                                          z_min },
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

    void draw_grid_around_point(const gfx::ICamera& cam,
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

private:
    gfx::Pipeline::Settings configure_pipeline(const gfx::ICamera& cam,
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

    MeshBuffers m_cube_mesh{ make_cube_mesh() };
    MeshBuffers m_grid_mesh{ make_grid_mesh() };
    gfx::UniformBuffer m_draw_params_ubo{ sizeof(DrawParamsBlock) };
    gfx::Pipeline m_pipeline = make_pipeline();
};

class IEditMode {
public:
    MG_INTERFACE_BOILERPLATE(IEditMode);

    virtual void render(const gfx::ICamera& cam,
                        const BlockScene& scene,
                        gfx::IRenderTarget& render_target,
                        EditModeRenderCommon& render_common) = 0;

    virtual bool update(BlockScene& scene, const EditModeUpdateArgs& data) = 0;

    virtual std::string name() const = 0;
};

template<std::invocable<int /*grid_x*/,
                        int /*grid_y*/,
                        float /*z_on_cell_entry*/,
                        float /*z_on_cell_exit*/> F>
void try_blocks_on_grid(const glm::vec3 start,
                        const glm::vec3 view_angle,
                        F&& function,
                        const float block_size,
                        const float max_distance)
{
    const auto v = view_angle;

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
        if (function(cell_x, cell_y, z_on_cell_entry, z_on_cell_exit)) {
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
    void render(const gfx::ICamera& cam,
                const BlockScene& scene,
                gfx::IRenderTarget& render_target,
                EditModeRenderCommon& render_common) override
    {
        const auto colour = can_insert ? glm::vec4(0.25f, 1.0f, 0.25f, 1.0f)
                                       : glm::vec4(1.0f, 0.25f, 0.25f, 1.0f);

        if (m_coord.has_value()) {
            render_common.draw_grid_around_point(cam,
                                                 render_target,
                                                 *m_coord,
                                                 m_new_block_z_min,
                                                 scene.block_size());

            render_common.draw_box_outline(cam,
                                           render_target,
                                           m_coord->x,
                                           m_coord->y,
                                           m_new_block_z_min,
                                           m_new_block_z_max,
                                           scene.block_size(),
                                           colour);
        }
    }

    bool update(BlockScene& scene, const EditModeUpdateArgs& data) override
    {
        const bool modify = data.button_states.try_get("modify").map_or(is_held, false);

        const auto block_size = scene.block_size();

        if (std::abs(data.view_angle.z) < FLT_EPSILON) {
            return false;
        }

        if (data.mouse_scroll_delta.y >= 1.0f) {
            m_new_block_z_max += k_vertical_step;
            if (!modify && m_new_block_z_min + k_vertical_step < m_new_block_z_max) {
                m_new_block_z_min += k_vertical_step;
            }
        }
        else if (data.mouse_scroll_delta.y <= -1.0f) {
            if (!modify) {
                m_new_block_z_min -= k_vertical_step;
            }
            if (m_new_block_z_min + k_vertical_step < m_new_block_z_max) {
                m_new_block_z_max -= k_vertical_step;
            }
        }

        m_coord = nullopt;
        m_cursor_target = get_target_position(data);
        if (m_cursor_target) {
            m_coord = { narrow_cast<int>(std::floor(m_cursor_target->x / block_size)),
                        narrow_cast<int>(std::floor(m_cursor_target->y / block_size)) };
        }

        auto existing_block_overlaps = [&] {
            std::vector<Block> blocks;
            scene.get_blocks_at(m_coord->x, m_coord->y, blocks);
            return std::ranges::any_of(blocks, [&](const auto& b) {
                return b.z_min < m_new_block_z_max && b.z_max > m_new_block_z_min;
            });
        };

        can_insert = m_coord.has_value() && !existing_block_overlaps();

        if (can_insert && data.button_states.try_get("interact").map_or(was_pressed, false)) {
            return scene.try_insert(m_coord->x,
                                    m_coord->y,
                                    { .z_min = m_new_block_z_min, .z_max = m_new_block_z_max });
        }

        return false;
    }

    std::string name() const override { return "Add"; }

    Opt<glm::vec3> get_target_position(const EditModeUpdateArgs& data) const
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
    float m_new_block_z_min = 0.0f;
    float m_new_block_z_max = 1.0f;
    Opt<glm::vec3> m_cursor_target;
    Opt<glm::ivec2> m_coord;
    bool can_insert = false;
};

class EditModeEdit : public IEditMode {
public:
    void render(const gfx::ICamera& cam,
                const BlockScene& scene,
                gfx::IRenderTarget& render_target,
                EditModeRenderCommon& render_common) override
    {
        const auto colour = glm::vec4(0.25f, 0.25f, 1.0f, 1.0f);
        if (m_selected_block.has_value()) {
            render_common.draw_box_outline(cam,
                                           render_target,
                                           m_selected_block->x,
                                           m_selected_block->y,
                                           m_selected_block->z_min,
                                           m_selected_block->z_max,
                                           scene.block_size(),
                                           colour);
        }
    }

    bool update(BlockScene& scene, const EditModeUpdateArgs& data) override
    {
        m_selected_block = nullopt;
        std::vector<Block> blocks;

        auto vertically_overlaps_block = [](const float z_min, const float z_max) {
            return [z_min, z_max](const Block& b) { return b.z_max >= z_min && b.z_min <= z_max; };
        };

        auto vertical_distance_to_block = [](const float z) {
            return [z](const Block& b) {
                const auto is_within = z >= b.z_min && z <= b.z_max;
                return is_within ? 0.0f : min(abs(b.z_min - z), abs(b.z_max - z));
            };
        };

        try_blocks_on_grid(
            data.view_position,
            data.view_angle,
            [&](int x, int y, float z_entry, float z_exit) {
                blocks.clear();
                scene.get_blocks_at(x, y, blocks);
                std::erase_if(blocks,
                              std::not_fn(vertically_overlaps_block(min(z_entry, z_exit),
                                                                    max(z_entry, z_exit))));
                if (blocks.empty()) {
                    return false;
                }

                // If several blocks in the cell overlap the vertical range of the ray within the
                // cell, pick the one that's closest to the ray's entrypoint into the cell.
                if (blocks.size() > 1) {
                    std::ranges::partial_sort(blocks,
                                              std::next(blocks.begin()),
                                              std::less{},
                                              vertical_distance_to_block(z_entry));
                }

                m_selected_block = SelectedBlock{ x, y, blocks[0].z_min, blocks[0].z_max };
                return true;
            },
            scene.block_size(),
            500.0f);

        if (!m_selected_block) {
            return false;
        }

        const auto [selected_block_level, selected_block_data] = [&] {
            blocks.clear();
            scene.get_blocks_at(m_selected_block->x, m_selected_block->y, blocks);
            for (size_t i = 0; i < blocks.size(); ++i) {
                if (blocks[i].z_min == m_selected_block->z_min) {
                    return std::pair{ i, blocks[i] };
                }
            }

            MG_ASSERT(false && "unreachable");
        }();

        const bool modify = data.button_states.try_get("modify").map_or(is_held, false);

        auto new_block_data = selected_block_data;

        if (data.mouse_scroll_delta.y >= 1.0f) {
            new_block_data.z_max += k_vertical_step;
            if (!modify && new_block_data.z_min + k_vertical_step < new_block_data.z_max) {
                new_block_data.z_min += k_vertical_step;
            }
        }
        else if (data.mouse_scroll_delta.y <= -1.0f) {
            if (!modify) {
                new_block_data.z_min -= k_vertical_step;
            }
            if (new_block_data.z_min + k_vertical_step < new_block_data.z_max) {
                new_block_data.z_max -= k_vertical_step;
            }
        }

        if (new_block_data != selected_block_data) {
            scene.try_delete(m_selected_block->x, m_selected_block->y, selected_block_level);
            const bool success =
                scene.try_insert(m_selected_block->x, m_selected_block->y, new_block_data);

            if (!success) {
                // Restore original block.
                scene.try_insert(m_selected_block->x, m_selected_block->y, selected_block_data);
            }

            return success;
        }

        if (data.button_states.try_get("interact_2").map_or(was_pressed, false)) {
            Opt<BlockTextures&> textures = scene.block_textures_at(m_selected_block->x,
                                                                   m_selected_block->y,
                                                                   selected_block_level);
            textures.map([](auto& t) {
                t[BlockFace::top] = (t[BlockFace::top] + 1) % k_block_scene_max_num_textures;
            });
            return textures.has_value();
        }

        if (data.button_states.try_get("delete").map_or(was_pressed, false)) {
            return scene.try_delete(m_selected_block->x, m_selected_block->y, selected_block_level);
        }

        return false;
    }

    std::string name() const override { return "Edit"; }

    Opt<SelectedBlock> m_selected_block;
};


} // namespace

struct BlockSceneEditor::Impl {
    enum class Mode { Add, Edit, Texture };

    Impl(BlockScene& scene_,
         input::IInputSource& input_source,
         std::shared_ptr<gfx::BitmapFont> font_)
        : scene{ scene_ }
        , button_tracker{ input_source }
        , scroll_tracker{ input_source }
        , font{ std::move(font_) }
    {
        edit_modes[Mode::Add] = std::make_unique<EditModeAdd>();
        edit_modes[Mode::Edit] = std::make_unique<EditModeEdit>();
        button_tracker.bind("mode_add", input::Key::Num1);
        button_tracker.bind("mode_edit", input::Key::Num2);
        button_tracker.bind("interact", input::MouseButton::left);
        button_tracker.bind("interact_2", input::MouseButton::right);
        button_tracker.bind("delete", input::Key::Del);
        button_tracker.bind("modify", input::Key::LeftShift);
    }

    BlockScene& scene;
    BlockSceneMesh mesh;

    input::ButtonTracker button_tracker;
    input::ScrollTracker scroll_tracker;

    std::shared_ptr<gfx::BitmapFont> font;

    EditModeRenderCommon render_common;

    std::map<Mode, std::unique_ptr<IEditMode>> edit_modes;
    Mode current_edit_mode{ Mode::Add };
};

BlockSceneEditor::BlockSceneEditor(BlockScene& scene,
                                   input::IInputSource& input_source,
                                   std::shared_ptr<gfx::BitmapFont> font)
    : m_impl{ scene, input_source, std::move(font) }
{}

void BlockSceneEditor::render(gfx::IRenderTarget& render_target, const gfx::RenderParams& params)
{
    auto& edit_mode = *m_impl->edit_modes[m_impl->current_edit_mode];
    edit_mode.render(params.camera, m_impl->scene, render_target, m_impl->render_common);
}

void BlockSceneEditor::render_ui(gfx::UIRenderList& render_list)
{
    auto& label_cmd = render_list.text_render_commands.emplace_back();
    label_cmd.text = "Block Scene Editor";
    label_cmd.font = m_impl->font;
    label_cmd.placement = gfx::UIPlacement{
        .position{ 0.5f, 0.0f },
        .position_pixel_offset{ 0.0f, 10.0f },
        .rotation{},
        .anchor{ 0.5f, 0.0f },
    };

    auto& mode_cmd = render_list.text_render_commands.emplace_back();
    mode_cmd.text = "Edit mode: " + m_impl->edit_modes[m_impl->current_edit_mode]->name();
    mode_cmd.font = m_impl->font;
    mode_cmd.placement = gfx::UIPlacement{
        .position{ gfx::UIPlacement::top_right },
        .position_pixel_offset{ -10.0f, -10.0f },
        .rotation{},
        .anchor{ gfx::UIPlacement::top_right },
    };

    auto& cross_cmd = render_list.text_render_commands.emplace_back();
    cross_cmd.text = "+";
    cross_cmd.font = m_impl->font;
    cross_cmd.placement = gfx::UIPlacement{
        .position{ gfx::UIPlacement::centre },
        .position_pixel_offset{ 0.0f, 0.0f },
        .rotation{},
        .anchor{ gfx::UIPlacement::centre },
    };
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
