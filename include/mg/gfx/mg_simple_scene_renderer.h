//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_simple_scene_renderer.h
 */

#pragma once

#include "mg/gfx/mg_scene_renderer.h"
#include "mg/gfx/mg_texture_pool.h"
#include "mg/gfx/render_passes/mg_billboard_pass.h"
#include "mg/gfx/render_passes/mg_blur_pass.h"
#include "mg/gfx/render_passes/mg_clear_pass.h"
#include "mg/gfx/render_passes/mg_mesh_pass.h"
#include "mg/gfx/render_passes/mg_sky_pass.h"
#include "mg/gfx/render_passes/mg_tonemap_and_bloom_pass.h"
#include "mg/gfx/render_passes/mg_ui_pass.h"

namespace Mg::gfx {

class SimpleSceneRenderTargets {
public:
    MG_MAKE_NON_COPYABLE(SimpleSceneRenderTargets);
    MG_MAKE_DEFAULT_MOVABLE(SimpleSceneRenderTargets);

    explicit SimpleSceneRenderTargets(const WindowSettings& settings,
                                      const std::shared_ptr<TexturePool>& texture_pool)
        : m_texture_pool{ texture_pool }
        , m_blur_target{ std::make_unique<BlurRenderTarget>(m_texture_pool, settings.video_mode) }
        , m_hdr_target{ make_hdr_target(settings.video_mode) }
    {}

    ~SimpleSceneRenderTargets()
    {
        m_texture_pool->destroy(m_hdr_target->colour_target());
        m_texture_pool->destroy(m_hdr_target->depth_target());
    }

    const std::shared_ptr<BlurRenderTarget>& blur_target() const { return m_blur_target; }
    const std::shared_ptr<TextureRenderTarget>& hdr_target() const { return m_hdr_target; }

private:
    std::shared_ptr<TexturePool> m_texture_pool;
    std::shared_ptr<BlurRenderTarget> m_blur_target;
    std::shared_ptr<TextureRenderTarget> m_hdr_target;

    std::unique_ptr<TextureRenderTarget> make_hdr_target(VideoMode mode) const
    {
        RenderTargetParams params{};
        params.filter_mode = TextureFilterMode::Linear;
        params.width = mode.width;
        params.height = mode.height;

        params.texture_format = RenderTargetParams::Format::RGBA16F;
        Texture2D* colour_target = m_texture_pool->create_render_target(params);

        params.texture_format = RenderTargetParams::Format::Depth24;
        Texture2D* depth_target = m_texture_pool->create_render_target(params);

        return TextureRenderTarget::with_colour_and_depth_targets(colour_target, depth_target);
    }
};

struct SimpleSceneRendererConfig {
    const Material* sky_material;
    Material* blur_material;
    Material* bloom_material;
};

struct SimpleSceneRendererData {
    std::shared_ptr<SceneLights> scene_lights;
    std::shared_ptr<RenderCommandProducer> mesh_render_command_producer;
    std::shared_ptr<BillboardRenderList> billboard_render_list;
    std::shared_ptr<UIRenderList> ui_render_list;
};

class SimpleSceneRenderer : public SceneRenderer {
public:
    SimpleSceneRenderer(ResourceCache& resource_cache,
                        const std::shared_ptr<TexturePool>& texture_pool,
                        Window& window,
                        SimpleSceneRendererConfig config,
                        std::shared_ptr<SimpleSceneRendererData> data)
        : SceneRenderer{ resource_cache, window }
        , m_config{ config }
        , m_data{ std::move(data) }
        , m_window_render_target{ window.render_target }
        , m_texture_pool{ texture_pool }
    {}

private:
    RenderPasses create_passes(const WindowSettings& settings) override
    {
        m_render_targets = std::make_unique<SimpleSceneRenderTargets>(settings, m_texture_pool);

        RenderPasses passes;

        passes.push_back(std::make_unique<ClearPass>(m_render_targets->hdr_target(),
                                                     glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f },
                                                     true,
                                                     true));

        passes.push_back(std::make_unique<ClearPass>(m_window_render_target,
                                                     glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f },
                                                     true,
                                                     true));

        passes.push_back(
            std::make_unique<SkyPass>(m_render_targets->hdr_target(), m_config.sky_material));

        passes.push_back(std::make_unique<MeshPass>(m_render_targets->hdr_target(),
                                                    m_data->scene_lights,
                                                    m_data->mesh_render_command_producer));

        passes.push_back(std::make_unique<BillboardPass>(m_render_targets->hdr_target(),
                                                         m_data->billboard_render_list));

        passes.push_back(std::make_unique<BlurPass>(m_render_targets->blur_target(),
                                                    m_render_targets->hdr_target(),
                                                    m_config.blur_material));

        passes.push_back(std::make_unique<TonemapAndBloomPass>(m_window_render_target,
                                                               m_render_targets->blur_target(),
                                                               m_render_targets->hdr_target(),
                                                               m_config.bloom_material));

        passes.push_back(std::make_unique<UIPass>(m_window_render_target,
                                                  m_data->ui_render_list,
                                                  settings.video_mode));

        return passes;
    }

    SimpleSceneRendererConfig m_config;
    std::shared_ptr<SimpleSceneRendererData> m_data;
    std::shared_ptr<WindowRenderTarget> m_window_render_target;
    std::unique_ptr<SimpleSceneRenderTargets> m_render_targets;
    std::shared_ptr<TexturePool> m_texture_pool;
};

} // namespace Mg::gfx
