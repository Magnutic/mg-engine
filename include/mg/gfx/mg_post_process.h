//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_post_process.h
 * Post-processing functionality.
 */

#pragma once

#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

namespace Mg::gfx {

class Material;

struct PostProcessRendererData;

class PostProcessRenderer : PImplMixin<PostProcessRendererData> {
public:
    explicit PostProcessRenderer();
    ~PostProcessRenderer();

    MG_MAKE_NON_COPYABLE(PostProcessRenderer);
    MG_MAKE_DEFAULT_MOVABLE(PostProcessRenderer);

    class RenderGuard;

    RenderGuard get_guard() noexcept;

    /** Post-process using only colour texture as input. */
    void post_process(const RenderGuard& guard,
                      const Material& material,
                      TextureHandle sampler_colour) noexcept;

    /** Post-process using colour and depth textures. */
    void post_process(const RenderGuard& guard,
                      const Material& material,
                      TextureHandle sampler_colour,
                      TextureHandle sampler_depth,
                      float z_near,
                      float z_far) noexcept;

    void drop_shaders() noexcept;
};

class PostProcessRenderer::RenderGuard {
    friend class PostProcessRenderer;

public:
    ~RenderGuard();

    MG_MAKE_NON_COPYABLE(RenderGuard);
    MG_MAKE_NON_MOVABLE(RenderGuard);

private:
    RenderGuard(PostProcessRendererData& data);

    PostProcessRendererData* m_data = nullptr;
};

} // namespace Mg::gfx
