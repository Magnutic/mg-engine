
//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_debug_pass.h
 */

#pragma once

#include "mg/gfx/mg_debug_renderer.h"
#include "mg/gfx/render_passes/mg_irender_pass.h"

#include <memory>

namespace Mg::gfx {

class DebugPass : public IRenderPass {
public:
    explicit DebugPass(std::shared_ptr<IRenderTarget> target) : m_target{ std::move(target) } {}

    void render(const RenderParams& params) override
    {
        get_debug_render_queue().dispatch(*m_target, m_renderer, params.camera.view_proj_matrix());
    }

private:
    std::shared_ptr<IRenderTarget> m_target;
    DebugRenderer m_renderer;
};

} // namespace Mg::gfx
