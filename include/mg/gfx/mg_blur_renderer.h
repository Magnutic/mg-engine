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
#include <vector>

namespace Mg {
struct VideoMode;
class ShaderResource;
} // namespace Mg

namespace Mg::gfx {

class PostProcessRenderer;
class TextureRenderTarget;
class Texture2D;
class TexturePool;
class Material;
class MaterialPool;

class BlurRenderTarget {
public:
    MG_MAKE_DEFAULT_MOVABLE(BlurRenderTarget)
    MG_MAKE_NON_COPYABLE(BlurRenderTarget)

    explicit BlurRenderTarget(std::shared_ptr<TexturePool> texture_pool,
                              const VideoMode& video_mode);

    ~BlurRenderTarget();

    // Get target texture, which after rendering will contain final blur.
    // Lifetime is the same as this object.
    const Texture2D* target_texture() const { return m_vertical_pass_target_texture; }

private:
    friend class BlurRenderer;

    std::shared_ptr<TexturePool> m_texture_pool;

    std::vector<std::unique_ptr<TextureRenderTarget>> m_horizontal_pass_targets;
    std::vector<std::unique_ptr<TextureRenderTarget>> m_vertical_pass_targets;

    Texture2D* m_horizontal_pass_target_texture;
    Texture2D* m_vertical_pass_target_texture;
};

/** Post-process renderer that applies a blur effect. */
class BlurRenderer {
public:
    explicit BlurRenderer(std::shared_ptr<MaterialPool> material_pool,
                          const ResourceHandle<ShaderResource>& blur_shader);
    ~BlurRenderer();

    MG_MAKE_NON_COPYABLE(BlurRenderer);
    MG_MAKE_NON_MOVABLE(BlurRenderer);

    // Render blur using the render target as a source image.
    void render(PostProcessRenderer& renderer,
                const TextureRenderTarget& source,
                BlurRenderTarget& destination);

private:
    std::shared_ptr<MaterialPool> m_material_pool;
    Material* m_blur_material;
};

} // namespace Mg::gfx
