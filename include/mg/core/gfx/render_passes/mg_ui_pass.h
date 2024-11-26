//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_ui_pass.h
 */

#pragma once

#include "mg/core/gfx/mg_bitmap_font.h"
#include "mg/core/gfx/mg_ui_renderer.h"
#include "mg/core/gfx/render_passes/mg_irender_pass.h"

#include <memory>

namespace Mg::gfx {

struct TextRenderCommand {
    std::shared_ptr<BitmapFont> font;
    TypeSetting typesetting = {};
    UIPlacement placement = {};
    std::string text;
};

struct UIRenderList {
    std::vector<TextRenderCommand> text_render_commands;
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
        for (const auto& text_render_command : m_render_list->text_render_commands) {
            auto prepared_text =
                text_render_command.font->prepare_text(text_render_command.text,
                                                       text_render_command.typesetting);
            m_renderer.draw_text(*m_target, text_render_command.placement, prepared_text);
        }
    }

private:
    std::shared_ptr<IRenderTarget> m_target;
    std::shared_ptr<UIRenderList> m_render_list;
    UIRenderer m_renderer;
};


} // namespace Mg::gfx
