//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/editor/mg_block_scene_editor.h"

#include "mg/core/gfx/mg_pipeline.h"
#include "mg/core/gfx/mg_render_target.h"
#include "mg/core/gfx/mg_ui_renderer.h"
#include "mg/core/gfx/render_passes/mg_ui_pass.h"
#include "mg/core/input/mg_input.h"

#include "block_scene_editor/mg_edit_mode_render_common.h"
#include "block_scene_editor/mg_edit_mode_add.h"
#include "block_scene_editor/mg_edit_mode_edit.h"

namespace Mg {

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

    if (button_states.get("mode_add").was_pressed) {
        m_impl->current_edit_mode = Impl::Mode::Add;
    }
    else if (button_states.get("mode_edit").was_pressed) {
        m_impl->current_edit_mode = Impl::Mode::Edit;
    }

    auto& edit_mode = *m_impl->edit_modes[m_impl->current_edit_mode];
    return edit_mode.update(m_impl->scene,
                            IEditMode::UpdateArgs{
                                .view_position = view_position,
                                .view_angle = view_angle,
                                .button_states = button_states,
                                .mouse_scroll_delta = mouse_scroll_delta,
                            });
}

} // namespace Mg
