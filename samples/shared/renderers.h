#include <mg/core/mg_window.h>
#include <mg/core/mg_window_settings.h>
#include <mg/gfx/mg_billboard_renderer.h>
#include <mg/gfx/mg_blur_renderer.h>
#include <mg/gfx/mg_debug_renderer.h>
#include <mg/gfx/mg_material.h>
#include <mg/gfx/mg_material_pool.h>
#include <mg/gfx/mg_mesh_renderer.h>
#include <mg/gfx/mg_post_process.h>
#include <mg/gfx/mg_skybox_renderer.h>
#include <mg/gfx/mg_texture_pool.h>
#include <mg/gfx/mg_ui_renderer.h>
#include <mg/resource_cache/mg_resource_cache.h>

class Renderers : Mg::Observer<Mg::WindowSettings> {
public:
    explicit Renderers(Mg::Window& window,
                       const std::shared_ptr<Mg::ResourceCache>& resource_cache,
                       const std::shared_ptr<Mg::gfx::MaterialPool>& material_pool,
                       Mg::ResourceHandle<Mg::ShaderResource> blur_shader_handle)
        : mesh_renderer{ Mg::gfx::LightGridConfig{} }
        , blur_renderer{ material_pool, blur_shader_handle }
        , ui_renderer{ window.settings().video_mode }
        , m_resource_cache{ resource_cache }
    {
        window.observe_settings(*this);

        // Drop shaders when shader resources have changed. This will force recompilation, so that
        // any changes will take immediate effect.
        m_resource_cache->set_resource_reload_callback(
            "ShaderResource",
            [](void* data, const Mg::FileChangedEvent&) {
                auto& self = *static_cast<Renderers*>(data);
                self.mesh_renderer.drop_shaders();
                self.billboard_renderer.drop_shaders();
                self.post_renderer.drop_shaders();
                self.ui_renderer.drop_shaders();
                self.skybox_renderer.drop_shaders();
            },
            this);
    }

    ~Renderers() override { m_resource_cache->remove_resource_reload_callback("ShaderResource"); }

    // Can not move or copy, since that would invalidate the resource-reload-callback user pointer.
    MG_MAKE_NON_MOVABLE(Renderers)
    MG_MAKE_NON_COPYABLE(Renderers)

    void on_notify(const Mg::WindowSettings& settings) override
    {
        ui_renderer.resolution(settings.video_mode);
    }

    Mg::gfx::MeshRenderer mesh_renderer;
    Mg::gfx::DebugRenderer debug_renderer;
    Mg::gfx::BillboardRenderer billboard_renderer;
    Mg::gfx::BlurRenderer blur_renderer;
    Mg::gfx::PostProcessRenderer post_renderer;
    Mg::gfx::UIRenderer ui_renderer;
    Mg::gfx::SkyboxRenderer skybox_renderer;

private:
    std::shared_ptr<Mg::ResourceCache> m_resource_cache;
};

class RenderTargets : Mg::Observer<Mg::WindowSettings> {
public:
    explicit RenderTargets(Mg::Window& window,
                           const std::shared_ptr<Mg::gfx::TexturePool>& texture_pool)
        : m_texture_pool(texture_pool)
    {
        window.observe_settings(*this);
        on_notify(window.settings());
    }

    void on_notify(const Mg::WindowSettings& settings) override
    {
        blur_target = std::make_unique<Mg::gfx::BlurRenderTarget>(m_texture_pool,
                                                                  settings.video_mode);
        // Dispose of old render target textures.
        if (hdr_target) {
            m_texture_pool->destroy(hdr_target->colour_target());
            m_texture_pool->destroy(hdr_target->depth_target());
        }
        hdr_target = make_hdr_target(settings.video_mode);
    }

    std::unique_ptr<Mg::gfx::BlurRenderTarget> blur_target;
    std::unique_ptr<Mg::gfx::TextureRenderTarget> hdr_target;

private:
    std::shared_ptr<Mg::gfx::TexturePool> m_texture_pool;

    std::unique_ptr<Mg::gfx::TextureRenderTarget> make_hdr_target(Mg::VideoMode mode) const
    {
        Mg::gfx::RenderTargetParams params{};
        params.filter_mode = Mg::gfx::TextureFilterMode::Linear;
        params.width = mode.width;
        params.height = mode.height;
        params.texture_format = Mg::gfx::RenderTargetParams::Format::RGBA16F;

        Mg::gfx::Texture2D* colour_target = m_texture_pool->create_render_target(params);

        params.texture_format = Mg::gfx::RenderTargetParams::Format::Depth24;

        Mg::gfx::Texture2D* depth_target = m_texture_pool->create_render_target(params);

        return Mg::gfx::TextureRenderTarget::with_colour_and_depth_targets(colour_target,
                                                                           depth_target);
    }
};
