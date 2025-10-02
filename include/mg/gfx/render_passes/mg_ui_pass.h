//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_ui_pass.h
 */

#pragma once

#include "mg/gfx/mg_bitmap_font.h"
#include "mg/gfx/mg_ui_renderer.h"
#include "mg/gfx/render_passes/mg_irender_pass.h"

#include <memory>

namespace Mg::gfx {

struct UIRenderList {
    // Temporary; should be a command list (of some type yet to be defined as of writing).
    std::unique_ptr<BitmapFont> font;
    std::string text_to_draw;
};

class UIPass : public IRenderPass {
public:
    explicit UIPass(std::shared_ptr<IRenderTarget> target,
                    std::shared_ptr<UIRenderList> render_list,
                    VideoMode video_mode)
        : m_target{ std::move(target) }
        , m_render_list{ std::move(render_list) }
        , m_renderer{ video_mode } // Note: we cannot use m_target->image_size, because it may take
                                   // a while for the render target to update after the window has
                                   // been resized.
    {}

    void render(const RenderParams& /*params*/) override
    {
        // Temporarily just drawing text
        UIPlacement placement = {};
        placement.position = UIPlacement::top_left + glm::vec2(0.01f, -0.01f);
        placement.anchor = UIPlacement::top_left;

        TypeSetting typesetting = {};
        typesetting.line_spacing_factor = 1.25f;
        typesetting.max_width_pixels = m_renderer.resolution().width;

        auto text = m_render_list->font->prepare_text(m_render_list->text_to_draw, typesetting);
        m_renderer.draw_text(*m_target, placement, text);
    }

private:
    std::shared_ptr<IRenderTarget> m_target;
    std::shared_ptr<UIRenderList> m_render_list;
    UIRenderer m_renderer;
};


} // namespace Mg::gfx
