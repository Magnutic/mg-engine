//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_scene.h
 */

#pragma once

#include "mg/core/mg_window.h"
#include "mg/gfx/mg_billboard_renderer.h"
#include "mg/gfx/mg_bitmap_font.h"
#include "mg/gfx/mg_blur_renderer.h"
#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_light.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_mesh_renderer.h"
#include "mg/gfx/mg_post_process.h"
#include "mg/gfx/mg_render_command_list.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/gfx/mg_skybox_renderer.h"
#include "mg/gfx/mg_ui_renderer.h"
#include <mg/core/mg_application_context.h>
#include <mg/core/mg_file_loader.h>
#include <mg/gfx/mg_material_pool.h>
#include <mg/gfx/mg_mesh_pool.h>
#include <mg/gfx/mg_texture_pool.h>
#include <mg/resource_cache/mg_resource_cache.h>

namespace Mg {

class SceneResources {
public:
    explicit SceneResources(std::shared_ptr<ResourceCache> resource_cache)
        : m_resource_cache{ std::move(resource_cache) }
        , m_mesh_pool{ std::make_shared<gfx::MeshPool>(m_resource_cache) }
        , m_texture_pool{ std::make_shared<gfx::TexturePool>(m_resource_cache) }
        , m_material_pool{ std::make_shared<gfx::MaterialPool>(m_resource_cache, m_texture_pool) }
    {}

protected:
    std::shared_ptr<ResourceCache> m_resource_cache;
    std::shared_ptr<gfx::MeshPool> m_mesh_pool;
    std::shared_ptr<gfx::TexturePool> m_texture_pool;
    std::shared_ptr<gfx::MaterialPool> m_material_pool;
};

namespace gfx {

struct RenderParams {
    const ICamera& camera;
    float time_since_init;
};

class IRenderPass {
public:
    MG_INTERFACE_BOILERPLATE(IRenderPass);
    virtual void render(const RenderParams&) = 0;
};

class SceneRenderer : Observer<WindowSettings> {
public:
    using RenderPasses = std::vector<std::unique_ptr<IRenderPass>>;

    explicit SceneRenderer(ResourceCache& resource_cache, Window& window)
        : m_window_settings{ window.settings() }
    {
        // Drop render passes when shader resources have changed, so changes take immediate effect.
        m_shader_file_changed_tracker = resource_cache.make_file_change_tracker(
            "ShaderResource",
            [](void* data, const FileChangedEvent&) {
                auto& self = *static_cast<SceneRenderer*>(data);
                self.m_render_passes.clear();
            },
            this);

        // Also drop when window has changed, to make new settings take effect.
        window.observe_settings(*this);
    }

    void render(const RenderParams& params)
    {
        if (m_render_passes.empty()) {
            m_render_passes = create_passes(m_window_settings);
        }

        for (auto& pass : m_render_passes) {
            pass->render(params);
        }
    }

protected:
    virtual RenderPasses create_passes(const WindowSettings& settings) = 0;

    void on_notify(const WindowSettings& settings) final
    {
        m_window_settings = settings;
        m_render_passes.clear();
    }

    std::vector<std::unique_ptr<IRenderPass>> m_render_passes;

private:
    std::shared_ptr<Mg::FileChangedTracker> m_shader_file_changed_tracker;
    WindowSettings m_window_settings;
};

class ClearPass : public IRenderPass {
public:
    explicit ClearPass(std::shared_ptr<IRenderTarget> target,
                       const glm::vec4 colour,
                       const bool do_clear_colour,
                       const bool do_clear_depth)
        : m_target{ std::move(target) }
        , m_colour{ colour }
        , m_do_clear_colour{ do_clear_colour }
        , m_do_clear_depth{ do_clear_depth }
    {}

    void render(const RenderParams& /*unused*/) override;

private:
    std::shared_ptr<IRenderTarget> m_target;
    glm::vec4 m_colour;
    bool m_do_clear_colour;
    bool m_do_clear_depth;
};

class BlurPass : public IRenderPass {
public:
    explicit BlurPass(std::shared_ptr<BlurRenderTarget> target,
                      std::shared_ptr<const TextureRenderTarget> source,
                      Material* blur_material)
        : m_blur_renderer{ blur_material }
        , m_target{ std::move(target) }
        , m_source{ std::move(source) }
    {}

    void render(const RenderParams& /*unused*/) override
    {
        m_blur_renderer.render(m_post_process_renderer, *m_source, *m_target);
    }

private:
    PostProcessRenderer m_post_process_renderer;
    BlurRenderer m_blur_renderer;
    std::shared_ptr<BlurRenderTarget> m_target;
    std::shared_ptr<const TextureRenderTarget> m_source;
};

struct SceneLights {
    std::vector<Light> point_lights;
};

class MeshPass : public IRenderPass {
public:
    explicit MeshPass(std::shared_ptr<IRenderTarget> target,
                      std::shared_ptr<SceneLights> scene_lights,
                      std::shared_ptr<RenderCommandProducer> render_command_producer,
                      SortingMode sorting_mode = SortingMode::near_to_far)
        : m_renderer{ LightGridConfig{} }
        , m_target{ std::move(target) }
        , m_scene_lights{ std::move(scene_lights) }
        , m_render_command_producer{ std::move(render_command_producer) }
        , m_sorting_mode{ sorting_mode }
    {}

    void render(const RenderParams& params) override
    {
        const auto& commands = m_render_command_producer->finalize(params.camera, m_sorting_mode);
        m_renderer.render(params.camera,
                          commands,
                          m_scene_lights->point_lights,
                          *m_target,
                          { params.time_since_init });
    }

private:
    MeshRenderer m_renderer;
    std::shared_ptr<IRenderTarget> m_target;
    std::shared_ptr<SceneLights> m_scene_lights;
    std::shared_ptr<RenderCommandProducer> m_render_command_producer;
    SortingMode m_sorting_mode;
};

class SkyPass : public IRenderPass {
public:
    explicit SkyPass(std::shared_ptr<IRenderTarget> target, const Material* material)
        : m_target{ std::move(target) }, m_material{ material }
    {}

    void render(const RenderParams& params) override
    {
        m_renderer.render(*m_target, params.camera, *m_material);
    }

private:
    std::shared_ptr<IRenderTarget> m_target;
    const Material* m_material;
    SkyboxRenderer m_renderer;
};

struct BillboardRenderList {
    using RenderList = std::pair<const Material*, std::vector<Billboard>>;
    std::vector<RenderList> render_lists;
};

class BillboardPass : public IRenderPass {
public:
    explicit BillboardPass(std::shared_ptr<IRenderTarget> target,
                           std::shared_ptr<BillboardRenderList> render_list)
        : m_target{ std::move(target) }, m_render_list{ std::move(render_list) }
    {}

    void render(const RenderParams& params) override
    {
        for (auto& [material, billboards] : m_render_list->render_lists) {
            m_renderer.render(*m_target, params.camera, billboards, *material);
        }
    }

private:
    std::shared_ptr<IRenderTarget> m_target;
    std::shared_ptr<BillboardRenderList> m_render_list;
    BillboardRenderer m_renderer;
};

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

struct UIRenderList {
    // Temporary; should be a command list (of some type yet to be defined as of writing).
    std::unique_ptr<BitmapFont> font;
    std::string text_to_draw;
};

class UIPass : public IRenderPass {
public:
    explicit UIPass(std::shared_ptr<IRenderTarget> target,
                    std::shared_ptr<UIRenderList> render_list,
                    VideoMode video_mode)
        : m_target{ std::move(target) }
        , m_render_list{ std::move(render_list) }
        , m_renderer{ video_mode } // Note: we cannot use m_target->image_size, because it may take
                                   // a while for the render target to update after the window has
                                   // been resized.
    {}

    void render(const RenderParams& /*params*/) override
    {
        // Temporarily just drawing text
        UIPlacement placement = {};
        placement.position = UIPlacement::top_left + glm::vec2(0.01f, -0.01f);
        placement.anchor = UIPlacement::top_left;

        TypeSetting typesetting = {};
        typesetting.line_spacing_factor = 1.25f;
        typesetting.max_width_pixels = m_renderer.resolution().width;

        auto text = m_render_list->font->prepare_text(m_render_list->text_to_draw, typesetting);
        m_renderer.draw_text(*m_target, placement, text);
    }

private:
    std::shared_ptr<IRenderTarget> m_target;
    std::shared_ptr<UIRenderList> m_render_list;
    UIRenderer m_renderer;
};

class SceneRenderTargets {
public:
    MG_MAKE_NON_COPYABLE(SceneRenderTargets);
    MG_MAKE_DEFAULT_MOVABLE(SceneRenderTargets);

    explicit SceneRenderTargets(const WindowSettings& settings,
                                const std::shared_ptr<TexturePool>& texture_pool)
        : m_texture_pool{ texture_pool }
        , m_blur_target{ std::make_unique<BlurRenderTarget>(m_texture_pool, settings.video_mode) }
        , m_hdr_target{ make_hdr_target(settings.video_mode) }
    {}

    ~SceneRenderTargets()
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

struct BasicSceneRendererConfig {
    const Material* sky_material;
    Material* blur_material;
    Material* bloom_material;
};

struct BasicSceneRendererData {
    std::shared_ptr<SceneLights> scene_lights;
    std::shared_ptr<RenderCommandProducer> mesh_render_command_producer;
    std::shared_ptr<BillboardRenderList> billboard_render_list;
    std::shared_ptr<UIRenderList> ui_render_list;
};

class BasicSceneRenderer : public SceneRenderer {
public:
    BasicSceneRenderer(ResourceCache& resource_cache,
                       const std::shared_ptr<TexturePool>& texture_pool,
                       Window& window,
                       BasicSceneRendererConfig config,
                       std::shared_ptr<BasicSceneRendererData> data)
        : SceneRenderer{ resource_cache, window }
        , m_config{ config }
        , m_data{ std::move(data) }
        , m_window_render_target{ window.render_target }
        , m_texture_pool{ texture_pool }
    {}

private:
    RenderPasses create_passes(const WindowSettings& settings) override
    {
        m_render_targets = std::make_unique<SceneRenderTargets>(settings, m_texture_pool);

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

    BasicSceneRendererConfig m_config;
    std::shared_ptr<BasicSceneRendererData> m_data;
    std::shared_ptr<WindowRenderTarget> m_window_render_target;
    std::unique_ptr<SceneRenderTargets> m_render_targets;
    std::shared_ptr<TexturePool> m_texture_pool;
};

} // namespace gfx

} // namespace Mg
