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
    virtual void drop_shaders() = 0;
};

class SceneRenderer {
public:
    explicit SceneRenderer(ResourceCache& resource_cache)
    {
        // Drop shaders when shader resources have changed. This will force recompilation, so that
        // any changes will take immediate effect.
        m_shader_file_changed_tracker = resource_cache.make_file_change_tracker(
            "ShaderResource",
            [](void* data, const FileChangedEvent&) {
                auto& self = *static_cast<SceneRenderer*>(data);
                for (auto& pass : self.render_passes) {
                    pass->drop_shaders();
                }
            },
            this);
    }

    void render(const RenderParams& params)
    {
        for (auto& pass : render_passes) {
            pass->render(params);
        }
    }

    std::vector<std::unique_ptr<IRenderPass>> render_passes;

private:
    std::shared_ptr<Mg::FileChangedTracker> m_shader_file_changed_tracker;
};

class SceneRenderTargets : Observer<WindowSettings> {
public:
    MG_MAKE_NON_COPYABLE(SceneRenderTargets);
    MG_MAKE_DEFAULT_MOVABLE(SceneRenderTargets);

    explicit SceneRenderTargets(Window& window, const std::shared_ptr<TexturePool>& texture_pool)
        : m_texture_pool(texture_pool)
    {
        window.observe_settings(*this);
        on_notify(window.settings());
    }

    ~SceneRenderTargets() override
    {
        m_texture_pool->destroy(hdr_target->colour_target());
        m_texture_pool->destroy(hdr_target->depth_target());
    }

    void on_notify(const WindowSettings& settings) override
    {
        blur_target = std::make_unique<BlurRenderTarget>(m_texture_pool, settings.video_mode);
        // Dispose of old render target textures.
        if (hdr_target) {
            m_texture_pool->destroy(hdr_target->colour_target());
            m_texture_pool->destroy(hdr_target->depth_target());
        }
        hdr_target = make_hdr_target(settings.video_mode);
    }

    std::shared_ptr<BlurRenderTarget> blur_target;
    std::shared_ptr<TextureRenderTarget> hdr_target;

private:
    std::shared_ptr<TexturePool> m_texture_pool;

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

#if 0
class ClearPass : public IRenderPass {
public:
    explicit ClearPass(std::shared_ptr<IRenderTarget> target,
                       glm::vec4 colour,
                       bool clear_colour,
                       bool clear_depth)
        : m_target{ std::move(target) }
        , m_colour{ colour }
        , m_clear_colour{ clear_colour }
        , m_clear_depth{ clear_depth }
    {}

    void render(const RenderParams& /*unused*/) override
    {
        glClearColor(red, green, blue, alpha);
        glBindFramebuffer(GL_FRAMEBUFFER, target.handle().as_gl_id());
        glViewport(0, 0, target.image_size().width, target.image_size().height);

        if (!m_target.is_window_render_target()) {
            const GLuint buffer = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &buffer);
        }

        // Change depth/colour write state if needed.
        std::array<GLboolean, 4> prev_colour_write = {};
        glGetBooleanv(GL_COLOR_WRITEMASK, prev_colour_write.data());
        const bool should_set_colour_write =
            m_clear_colour &&
            std::ranges::any_of(prev_colour_write, [](GLboolean b) { return b == GL_FALSE; });

        GLboolean prev_depth_write = GL_FALSE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &prev_depth_write);
        const bool should_set_depth_write = m_clear_depth && prev_depth_write == GL_FALSE;

        if (should_set_colour_write) {
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
        if (should_set_depth_write) {
            glDepthMask(GL_TRUE);
        }

        glClear((m_clear_colour ? GL_COLOR_BUFFER_BIT : 0u) |
                (m_clear_depth ? GL_DEPTH_BUFFER_BIT : 0u));

        // Revert depth/colour write state if it was changed.
        if (should_set_colour_write) {
            glColorMask(prev_colour_write[0],
                        prev_colour_write[1],
                        prev_colour_write[2],
                        prev_colour_write[3]);
        }
        if (should_set_depth_write) {
            glDepthMask(prev_depth_write);
        }
    }

    void drop_shaders() override
    { /* no shaders */
    }

private:
    std::shared_ptr<IRenderTarget> m_target;
    glm::vec4 m_colour;
    bool m_clear_colour;
    bool m_clear_depth;
};
#endif

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

    void drop_shaders() override { m_post_process_renderer.drop_shaders(); }

private:
    PostProcessRenderer m_post_process_renderer;
    BlurRenderer m_blur_renderer;
    std::shared_ptr<BlurRenderTarget> m_target;
    std::shared_ptr<const TextureRenderTarget> m_source;
};

struct SceneLights {
    std::vector<gfx::Light> point_lights;
};

class MeshPass : public IRenderPass {
public:
    explicit MeshPass(std::shared_ptr<IRenderTarget> target,
                      std::shared_ptr<SceneLights> scene_lights,
                      std::shared_ptr<gfx::RenderCommandProducer> render_command_producer,
                      gfx::SortingMode sorting_mode = gfx::SortingMode::near_to_far)
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

    void drop_shaders() override { m_renderer.drop_shaders(); }

private:
    MeshRenderer m_renderer;
    std::shared_ptr<IRenderTarget> m_target;
    std::shared_ptr<SceneLights> m_scene_lights;
    std::shared_ptr<gfx::RenderCommandProducer> m_render_command_producer;
    gfx::SortingMode m_sorting_mode;
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

    void drop_shaders() override { m_renderer.drop_shaders(); }

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

    void drop_shaders() override { m_renderer.drop_shaders(); }

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

    void drop_shaders() override { m_post_process_renderer.drop_shaders(); }

private:
    std::shared_ptr<IRenderTarget> m_target;
    std::shared_ptr<const BlurRenderTarget> m_blur_source;
    std::shared_ptr<const TextureRenderTarget> m_hdr_colour_source;
    PostProcessRenderer m_post_process_renderer;
    Material* m_bloom_material;
};

struct UIRenderList {
    // Temporary; should be a command list (of some type yet to be defined as of writing).
    std::unique_ptr<Mg::gfx::BitmapFont> font;
    std::string text_to_draw;
};

class UIPass : public IRenderPass, Observer<WindowSettings> {
public:
    explicit UIPass(std::shared_ptr<IRenderTarget> target,
                    std::shared_ptr<UIRenderList> render_list,
                    Window& window)
        : m_target{ std::move(target) }
        , m_render_list{ std::move(render_list) }
        , m_renderer{ window.settings().video_mode }
    {
        window.observe_settings(*this);
    }

    void on_notify(const WindowSettings& settings) override
    {
        m_renderer.resolution(settings.video_mode);
    }

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

    void drop_shaders() override { m_renderer.drop_shaders(); }


private:
    std::shared_ptr<IRenderTarget> m_target;
    std::shared_ptr<UIRenderList> m_render_list;
    UIRenderer m_renderer;
};

struct BasicSceneRendererData {
    std::shared_ptr<SceneRenderTargets> render_targets;
    std::shared_ptr<SceneLights> scene_lights;
    std::shared_ptr<RenderCommandProducer> mesh_render_command_producer;
    std::shared_ptr<BillboardRenderList> billboard_render_list;
    std::shared_ptr<UIRenderList> ui_render_list;

    const Material* sky_material;
    Material* blur_material;
    Material* bloom_material;
};

inline std::unique_ptr<SceneRenderer> setup_basic_scene_renderer(Window& window,
                                                                 ResourceCache& resource_cache,
                                                                 const BasicSceneRendererData& data)
{
    auto result = std::make_unique<SceneRenderer>(resource_cache);

    result->render_passes.push_back(
        std::make_unique<SkyPass>(data.render_targets->hdr_target, data.sky_material));

    result->render_passes.push_back(std::make_unique<MeshPass>(data.render_targets->hdr_target,
                                                               data.scene_lights,
                                                               data.mesh_render_command_producer));

    result->render_passes.push_back(std::make_unique<BillboardPass>(data.render_targets->hdr_target,
                                                                    data.billboard_render_list));

    result->render_passes.push_back(std::make_unique<BlurPass>(data.render_targets->blur_target,
                                                               data.render_targets->hdr_target,
                                                               data.blur_material));

    result->render_passes.push_back(
        std::make_unique<TonemapAndBloomPass>(window.render_target,
                                              data.render_targets->blur_target,
                                              data.render_targets->hdr_target,
                                              data.bloom_material));

    result->render_passes.push_back(
        std::make_unique<UIPass>(window.render_target, data.ui_render_list, window));

    return result;
}

} // namespace gfx

} // namespace Mg
