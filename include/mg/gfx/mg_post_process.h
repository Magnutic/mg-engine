//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_post_process.h
 * Post-processing functionality.
 */

#pragma once

#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/utils/mg_impl_ptr.h"
#include "mg/utils/mg_macros.h"

namespace Mg::gfx {

class IRenderTarget;
class Material;

class PostProcessRenderer {
public:
    explicit PostProcessRenderer();
    ~PostProcessRenderer();
    MG_MAKE_NON_COPYABLE(PostProcessRenderer);
    MG_MAKE_DEFAULT_MOVABLE(PostProcessRenderer);

    class Context;

    /** Create a post-processing context. This initializes some state within the renderer so that it
     * can be re-used between multiple sequential invocations to `post_process`. It should not live
     * longer than necessary; in particular, it is not allowed to keep the Context while rendering
     * using some other renderer.
     */
    Context make_context() noexcept;

    /** Post-process using only colour texture as input. */
    void post_process(const Context& context,
                      const Material& material,
                      const IRenderTarget& render_target,
                      TextureHandle sampler_colour) noexcept;

    /** Post-process using colour and depth textures. */
    void post_process(const Context& context,
                      const Material& material,
                      const IRenderTarget& render_target,
                      TextureHandle sampler_colour,
                      TextureHandle sampler_depth,
                      float z_near,
                      float z_far) noexcept;

    void drop_shaders() noexcept;

    struct Impl;

private:
    ImplPtr<Impl> m_impl;
};

/** RAII class that allows a post-process rendering context to be re-used between multiple
 * invocations to PostProcessRenderer::post_process.
 */
class PostProcessRenderer::Context {
    friend class PostProcessRenderer;

public:
    ~Context();

    MG_MAKE_NON_COPYABLE(Context);
    MG_MAKE_NON_MOVABLE(Context);

private:
    Context(PostProcessRenderer::Impl& data);

    PostProcessRenderer::Impl* m_data = nullptr;
};

} // namespace Mg::gfx
