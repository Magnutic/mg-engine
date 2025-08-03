//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_billboard_pass.h
 */

#pragma once

#include "mg/gfx/mg_billboard_renderer.h"
#include "mg/gfx/render_passes/mg_irender_pass.h"

#include <memory>

namespace Mg::gfx {

struct BillboardRenderList {
    using RenderList = std::pair<const Material*, std::vector<Billboard>>;
    std::vector<RenderList> render_lists;
};

class BillboardPass : public IRenderPass {
public:
    explicit BillboardPass(std::shared_ptr<IRenderTarget> target,
                           std::shared_ptr<BillboardRenderList> render_list)
        : m_target{ std::move(target) }, m_render_list{ std::move(render_list) }
    {}

    void render(const RenderParams& params) override
    {
        for (auto& [material, billboards] : m_render_list->render_lists) {
            m_renderer.render(*m_target, params.camera, billboards, *material);
        }
    }

private:
    std::shared_ptr<IRenderTarget> m_target;
    std::shared_ptr<BillboardRenderList> m_render_list;
    BillboardRenderer m_renderer;
};

} // namespace Mg::gfx
