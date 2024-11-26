//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_editor_render_pass.h
 * .
 */

#pragma once

#include "mg/core/gfx/mg_render_target.h"
#include "mg/core/gfx/render_passes/mg_irender_pass.h"
#include "mg/core/gfx/render_passes/mg_ui_pass.h"
#include "mg/utils/mg_macros.h"

#include <memory>
#include <vector>

namespace Mg::gfx {

struct UIRenderList;

class IRenderableEditor {
public:
    MG_INTERFACE_BOILERPLATE(IRenderableEditor);
    virtual void render(IRenderTarget& render_target, const RenderParams& params) = 0;
    virtual void render_ui(gfx::UIRenderList& render_list) = 0;
};

struct EditorsToRender {
    std::vector<std::weak_ptr<IRenderableEditor>> editors;
};

class Editor3DPass : public IRenderPass {
public:
    explicit Editor3DPass(std::shared_ptr<IRenderTarget> target,
                          std::shared_ptr<EditorsToRender> editors_to_render)
        : m_target{ std::move(target) }, m_editors_to_render{ std::move(editors_to_render) }
    {}

    void render(const RenderParams& params) override
    {
        for (auto& to_render : m_editors_to_render->editors) {
            to_render.lock()->render(*m_target, params);
        }
    }

private:
    std::shared_ptr<IRenderTarget> m_target;
    std::shared_ptr<EditorsToRender> m_editors_to_render;
};

class EditorUIPass : public IRenderPass {
public:
    explicit EditorUIPass(std::shared_ptr<IRenderTarget> target,
                          std::shared_ptr<EditorsToRender> editors_to_render,
                          VideoMode video_mode)
        : m_render_list{ std::make_shared<UIRenderList>() }
        , m_editors_to_render{ std::move(editors_to_render) }
        , m_ui_pass{ std::move(target), m_render_list, video_mode }
    {}

    void render(const RenderParams& params) override
    {
        m_render_list->text_render_commands.clear();
        for (auto& to_render : m_editors_to_render->editors) {
            to_render.lock()->render_ui(*m_render_list);
        }
        m_ui_pass.render(params);
    }

private:
    std::shared_ptr<UIRenderList> m_render_list;
    std::shared_ptr<EditorsToRender> m_editors_to_render;
    UIPass m_ui_pass;
};

} // namespace Mg::gfx
