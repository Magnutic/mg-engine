//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_blur_pass.h
 */

#pragma once

#include "mg/core/mg_window_settings.h"
#include "mg/gfx/mg_post_process.h"
#include "mg/gfx/render_passes/mg_irender_pass.h"
#include "mg/utils/mg_macros.h"

#include <memory>
#include <vector>

namespace Mg::gfx {

class PostProcessRenderer;
class TextureRenderTarget;
class Texture2D;
class TexturePool;
class Material;

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
    friend class BlurPass;

    std::shared_ptr<TexturePool> m_texture_pool;

    std::vector<std::unique_ptr<TextureRenderTarget>> m_horizontal_pass_targets;
    std::vector<std::unique_ptr<TextureRenderTarget>> m_vertical_pass_targets;

    Texture2D* m_horizontal_pass_target_texture;
    Texture2D* m_vertical_pass_target_texture;
};

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
