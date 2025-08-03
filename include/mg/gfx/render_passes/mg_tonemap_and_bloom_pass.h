//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_tonemap_and_bloom_pass.h
 */

#pragma once

#include "mg/gfx/mg_blur_renderer.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_post_process.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/gfx/mg_texture2d.h"
#include "mg/gfx/render_passes/mg_irender_pass.h"

#include <memory>

namespace Mg::gfx {

class TonemapAndBloomPass : public IRenderPass {
public:
    explicit TonemapAndBloomPass(std::shared_ptr<IRenderTarget> target,
                                 std::shared_ptr<const BlurRenderTarget> blur_source,
                                 std::shared_ptr<const TextureRenderTarget> hdr_colour_source,
                                 Material* bloom_material)
        : m_target{ std::move(target) }
        , m_blur_source{ std::move(blur_source) }
        , m_hdr_colour_source{ std::move(hdr_colour_source) }
        , m_bloom_material{ bloom_material }
    {}

    void render(const RenderParams& /*params*/) override
    {
        m_bloom_material->set_sampler("sampler_bloom", m_blur_source->target_texture());
        m_post_process_renderer.post_process(m_post_process_renderer.make_context(),
                                             *m_bloom_material,
                                             *m_target,
                                             m_hdr_colour_source->colour_target()->handle());
    }

private:
    std::shared_ptr<IRenderTarget> m_target;
    std::shared_ptr<const BlurRenderTarget> m_blur_source;
    std::shared_ptr<const TextureRenderTarget> m_hdr_colour_source;
    PostProcessRenderer m_post_process_renderer;
    Material* m_bloom_material;
};


} // namespace Mg::gfx
