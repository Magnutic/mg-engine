//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_blur_pass.h
 */

#pragma once

#include "mg/core/gfx/mg_blur_render_target.h"
#include "mg/core/gfx/mg_post_process.h"
#include "mg/core/gfx/render_passes/mg_irender_pass.h"

#include <memory>

namespace Mg::gfx {

class PostProcessRenderer;
class Material;

class BlurPass : public IRenderPass {
public:
    explicit BlurPass(std::shared_ptr<BlurRenderTarget> target,
                      std::shared_ptr<const TextureRenderTarget> source,
                      Material* blur_material)
        : m_blur_material{ blur_material }
        , m_target{ std::move(target) }
        , m_source{ std::move(source) }
    {}

    void render(const RenderParams& /*unused*/) override;

private:
    PostProcessRenderer m_post_process_renderer;
    Material* m_blur_material;
    std::shared_ptr<BlurRenderTarget> m_target;
    std::shared_ptr<const TextureRenderTarget> m_source;
};

} // namespace Mg::gfx
