//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_blur_renderer.h"

#include "mg/core/mg_window_settings.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_material_pool.h"
#include "mg/gfx/mg_post_process.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/gfx/mg_texture2d.h"
#include "mg/gfx/mg_texture_pool.h"
#include "mg/utils/mg_math_utils.h"

namespace Mg::gfx {

BlurRenderTarget::BlurRenderTarget(std::shared_ptr<TexturePool> texture_pool,
                                   const VideoMode& video_mode)
    : m_texture_pool{ std::move(texture_pool) }
{
    MG_GFX_DEBUG_GROUP("BlurRenderTarget::BlurRenderTarget")

    static constexpr int32_t k_num_mip_levels = 4;

    RenderTargetParams params{};
    params.filter_mode = TextureFilterMode::Linear;
    params.width = video_mode.width >> 2;
    params.height = video_mode.height >> 2;
    params.num_mip_levels = k_num_mip_levels;
    params.texture_format = RenderTargetParams::Format::RGBA16F;

    m_horizontal_pass_target_texture = m_texture_pool->create_render_target(params);
    m_vertical_pass_target_texture = m_texture_pool->create_render_target(params);

    for (int32_t mip_level = 0; mip_level < k_num_mip_levels; ++mip_level) {
        m_horizontal_pass_targets.emplace_back(
            TextureRenderTarget::with_colour_target(m_horizontal_pass_target_texture,
                                                    TextureRenderTarget::DepthType::None,
                                                    mip_level));

        m_vertical_pass_targets.emplace_back(
            TextureRenderTarget::with_colour_target(m_vertical_pass_target_texture,
                                                    TextureRenderTarget::DepthType::None,
                                                    mip_level));
    }
}

BlurRenderTarget::~BlurRenderTarget()
{
    m_texture_pool->destroy(m_horizontal_pass_target_texture);
    m_texture_pool->destroy(m_vertical_pass_target_texture);
}

BlurRenderer::BlurRenderer(std::shared_ptr<MaterialPool> material_pool,
                           const ResourceHandle<ShaderResource>& blur_shader)
    : m_material_pool{ std::move(material_pool) }
    , m_blur_material{ m_material_pool->create_anonymous(blur_shader) }
{}

BlurRenderer::~BlurRenderer()
{
    m_material_pool->destroy(m_blur_material);
}

void BlurRenderer::render(PostProcessRenderer& renderer,
                          const TextureRenderTarget& source,
                          BlurRenderTarget& destination)
{
    MG_GFX_DEBUG_GROUP("BlurRenderer::render")

    constexpr size_t k_num_blur_iterations = 2;
    const size_t num_blur_targets = destination.m_horizontal_pass_targets.size();

    TextureRenderTarget::BlitSettings blit_settings = {};
    blit_settings.colour = true;
    blit_settings.depth = false;
    blit_settings.stencil = false;
    blit_settings.filter = TextureRenderTarget::BlitFilter::linear;

    // Init first pass by blitting to ping-pong buffers.
    TextureRenderTarget::blit(source, *destination.m_vertical_pass_targets[0], blit_settings);

    PostProcessRenderer::Context post_render_context = renderer.make_context();

    for (size_t mip_i = 0; mip_i < num_blur_targets; ++mip_i) {
        TextureRenderTarget& horizontal_target = *destination.m_horizontal_pass_targets[mip_i];
        TextureRenderTarget& vertical_target = *destination.m_vertical_pass_targets[mip_i];

        // Source mip-level will be [0, 0, 1, 2, 3, ...]
        m_blur_material->set_parameter("source_mip_level", max(0, static_cast<int>(mip_i) - 1));
        m_blur_material->set_parameter("destination_mip_level", static_cast<int>(mip_i));

        // Render gaussian blur in separate horizontal and vertical passes.
        for (size_t u = 0; u < k_num_blur_iterations; ++u) {
            // Horizontal pass
            m_blur_material->set_option("HORIZONTAL", true);
            renderer.post_process(post_render_context,
                                  *m_blur_material,
                                  horizontal_target,
                                  vertical_target.colour_target()->handle());

            m_blur_material->set_parameter("source_mip_level", static_cast<int>(mip_i));

            // Vertical pass
            m_blur_material->set_option("HORIZONTAL", false);
            renderer.post_process(post_render_context,
                                  *m_blur_material,
                                  vertical_target,
                                  horizontal_target.colour_target()->handle());
        }
    }
}

} // namespace Mg::gfx
