//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_block_scene_editor.h
 * .
 */

#pragma once

#include "mg/core/gfx/render_passes/mg_editor_render_pass.h"
#include "mg/scene/mg_block_scene.h"

namespace Mg {

namespace input {
class IInputSource;
}

namespace gfx {
class BitmapFont;
class IRenderTarget;
struct UIRenderList;
} // namespace gfx

class BlockSceneEditor : public gfx::IRenderableEditor {
public:
    explicit BlockSceneEditor(BlockScene& scene,
                              input::IInputSource& input_source,
                              std::shared_ptr<gfx::BitmapFont> font);

    void render(gfx::IRenderTarget& render_target, const gfx::RenderParams& params) override;

    void render_ui(gfx::UIRenderList& render_list) override;

    /** Run editor step. Returns whether any changes were made to the scene. */
    bool update(const glm::vec3& view_position, const glm::vec3& view_angle);

    struct Impl;

private:
    ImplPtr<Impl> m_impl;
};


} // namespace Mg
