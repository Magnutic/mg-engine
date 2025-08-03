//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_blur_pass.h
 */

#pragma once

#include "mg/gfx/mg_blur_renderer.h"
#include "mg/gfx/mg_post_process.h"
#include "mg/gfx/render_passes/mg_irender_pass.h"

#include <memory>

namespace Mg::gfx {

class BlurPass : public IRenderPass {
public:
    explicit BlurPass(std::shared_ptr<BlurRenderTarget> target,
                      std::shared_ptr<const TextureRenderTarget> source,
                      Material* blur_material)
        : m_blur_renderer{ blur_material }
        , m_target{ std::move(target) }
        , m_source{ std::move(source) }
    {}

    void render(const RenderParams& /*unused*/) override
    {
        m_blur_renderer.render(m_post_process_renderer, *m_source, *m_target);
    }

private:
    PostProcessRenderer m_post_process_renderer;
    BlurRenderer m_blur_renderer;
    std::shared_ptr<BlurRenderTarget> m_target;
    std::shared_ptr<const TextureRenderTarget> m_source;
};

} // namespace Mg::gfx
