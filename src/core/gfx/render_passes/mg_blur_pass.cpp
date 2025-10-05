//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/gfx/render_passes/mg_blur_pass.h"

#include "mg/core/gfx/mg_gfx_debug_group.h"
#include "mg/core/gfx/mg_material.h"
#include "mg/core/gfx/mg_render_target.h"
#include "mg/core/gfx/mg_texture2d.h"
#include "mg/utils/mg_math_utils.h"

namespace Mg::gfx {

void BlurPass::render(const RenderParams& /*unused*/)
{
    MG_GFX_DEBUG_GROUP_BY_FUNCTION

    constexpr size_t k_num_blur_iterations = 2;
    const size_t num_blur_targets = m_target->m_horizontal_pass_targets.size();

    TextureRenderTarget::BlitSettings blit_settings = {};
    blit_settings.colour = true;
    blit_settings.depth = false;
    blit_settings.stencil = false;
    blit_settings.filter = TextureRenderTarget::BlitFilter::linear;

    // Init first pass by blitting to ping-pong buffers.
    TextureRenderTarget::blit(*m_source, *m_target->m_vertical_pass_targets[0], blit_settings);

    PostProcessRenderer::Context post_render_context = m_post_process_renderer.make_context();

    for (size_t mip_i = 0; mip_i < num_blur_targets; ++mip_i) {
        TextureRenderTarget& horizontal_target = *m_target->m_horizontal_pass_targets[mip_i];
        TextureRenderTarget& vertical_target = *m_target->m_vertical_pass_targets[mip_i];

        // Source mip-level will be [0, 0, 1, 2, 3, ...]
        m_blur_material->set_parameter("source_mip_level", max(0, static_cast<int>(mip_i) - 1));
        m_blur_material->set_parameter("destination_mip_level", static_cast<int>(mip_i));

        // Render gaussian blur in separate horizontal and vertical passes.
        for (size_t u = 0; u < k_num_blur_iterations; ++u) {
            // Horizontal pass
            m_blur_material->set_option("HORIZONTAL", true);
            m_post_process_renderer.post_process(post_render_context,
                                                 *m_blur_material,
                                                 horizontal_target,
                                                 vertical_target.colour_target()->handle());

            m_blur_material->set_parameter("source_mip_level", static_cast<int>(mip_i));

            // Vertical pass
            m_blur_material->set_option("HORIZONTAL", false);
            m_post_process_renderer.post_process(post_render_context,
                                                 *m_blur_material,
                                                 vertical_target,
                                                 horizontal_target.colour_target()->handle());
        }
    }
}

} // namespace Mg::gfx
