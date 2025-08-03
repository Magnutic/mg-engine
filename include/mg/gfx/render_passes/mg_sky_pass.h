//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_sky_pass.h
 */

#pragma once

#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/gfx/mg_skybox_renderer.h"
#include "mg/gfx/render_passes/mg_irender_pass.h"

#include <memory>

namespace Mg::gfx {

class SkyPass : public IRenderPass {
public:
    explicit SkyPass(std::shared_ptr<IRenderTarget> target, const Material* material)
        : m_target{ std::move(target) }, m_material{ material }
    {}

    void render(const RenderParams& params) override
    {
        m_renderer.render(*m_target, params.camera, *m_material);
    }

private:
    std::shared_ptr<IRenderTarget> m_target;
    const Material* m_material;
    SkyboxRenderer m_renderer;
};

} // namespace Mg::gfx
