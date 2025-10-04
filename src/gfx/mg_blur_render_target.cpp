//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_blur_render_target.h"

#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/gfx/mg_texture_pool.h"

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

} // namespace Mg::gfx
