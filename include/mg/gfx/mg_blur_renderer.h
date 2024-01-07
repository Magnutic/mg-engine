//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_blur_renderer.h
 * Post-process renderer that applies a blur effect.
 */

#pragma once

#include "mg/resource_cache/mg_resource_handle.h"
#include "mg/utils/mg_impl_ptr.h"
#include "mg/utils/mg_macros.h"

#include <memory>

namespace Mg {
struct VideoMode;
}

namespace Mg::gfx {

class PostProcessRenderer;
class TextureRenderTarget;
class Texture2D;
class TexturePool;
class Material;

class BlurRenderer {
public:
    explicit BlurRenderer(std::shared_ptr<TexturePool> texture_pool, const VideoMode& video_mode);

    ~BlurRenderer();

    MG_MAKE_NON_COPYABLE(BlurRenderer);
    MG_MAKE_NON_MOVABLE(BlurRenderer);

    // Render blur using the render target as a source image.
    void render(PostProcessRenderer& renderer,
                const TextureRenderTarget& source,
                Material& blur_material);

    // Get target texture, which after rendering will contain final blur.
    Texture2D* target_texture() const;

    struct Impl;

private:
    ImplPtr<Impl> m_impl;
};

} // namespace Mg::gfx
