#pragma once

#include <mg/containers/mg_small_vector.h>
#include <mg/core/mg_application_context.h>
#include <mg/core/mg_identifier.h>
#include <mg/core/mg_window_settings.h>
#include <mg/ecs/mg_entity.h>
#include <mg/gfx/mg_bitmap_font.h>
#include <mg/gfx/mg_material.h>
#include <mg/gfx/mg_material_pool.h>
#include <mg/gfx/mg_mesh.h>
#include <mg/gfx/mg_mesh_pool.h>
#include <mg/gfx/mg_render_command_list.h>
#include <mg/gfx/mg_render_target.h>
#include <mg/gfx/mg_texture_pool.h>
#include <mg/input/mg_input.h>
#include <mg/mg_player_controller.h>
#include <mg/physics/mg_physics.h>
#include <mg/resource_cache/mg_resource_cache.h>
#include <mg/scene/mg_common_scene_components.h>
#include <mg/utils/mg_interpolate_transform.h>
#include <mg/utils/mg_optional.h>

#include "shared/renderers.h"

inline std::shared_ptr<Mg::ResourceCache> setup_resource_cache()
{
    return std::make_shared<Mg::ResourceCache>(
        std::make_unique<Mg::BasicFileLoader>("../samples/data"));
}

class Scene : public Mg::IApplication {
public:
    Mg::ApplicationContext app;

    Scene();
    ~Scene() override;

    MG_MAKE_NON_MOVABLE(Scene);
    MG_MAKE_NON_COPYABLE(Scene);

    std::shared_ptr<Mg::ResourceCache> resource_cache = setup_resource_cache();

    std::shared_ptr<Mg::gfx::MeshPool> mesh_pool =
        std::make_shared<Mg::gfx::MeshPool>(resource_cache);
    std::shared_ptr<Mg::gfx::TexturePool> texture_pool =
        std::make_shared<Mg::gfx::TexturePool>(resource_cache);
    std::shared_ptr<Mg::gfx::MaterialPool> material_pool =
        std::make_shared<Mg::gfx::MaterialPool>(resource_cache, texture_pool);

    std::unique_ptr<Mg::gfx::BitmapFont> font;

    Mg::ResourceHandle<Mg::ShaderResource> blur_shader_handle =
        resource_cache->resource_handle<Mg::ShaderResource>("shaders/post_process_blur.hjson");

    Renderers renderers{ app.window(), resource_cache, material_pool, blur_shader_handle };

    Mg::gfx::RenderCommandProducer render_command_producer;
    std::vector<Mg::gfx::Billboard> billboard_render_list;

    RenderTargets render_targets{ app.window(), texture_pool };

    Mg::gfx::Camera camera;

    std::shared_ptr<Mg::input::ButtonTracker> sample_control_button_tracker;

    std::unique_ptr<Mg::physics::World> physics_world;
    std::unique_ptr<Mg::PlayerController> player_controller;
    std::unique_ptr<Mg::EditorController> editor_controller;
    Mg::IPlayerController* current_controller = nullptr;
    std::unique_ptr<Mg::physics::CharacterController> character_controller;

    std::vector<Mg::gfx::Light> scene_lights;

    Mg::gfx::Material* bloom_material =
        material_pool->copy("bloom", material_pool->get_or_load("materials/bloom.hjson"));

    const Mg::gfx::Material* billboard_material =
        material_pool->get_or_load("materials/billboard.hjson");
    const Mg::gfx::Material* particle_material =
        material_pool->get_or_load("materials/particle.hjson");
    const Mg::gfx::Material* sky_material = material_pool->get_or_load("materials/skybox.hjson");

    const Mg::gfx::Material* default_material =
        material_pool->get_or_load("materials/default.hjson");

    Mg::gfx::ParticleSystem particle_system;

    int debug_visualization = 0;

    void init();
    void simulation_step() override;
    void render(double lerp_factor) override;
    bool should_exit() const override { return m_should_exit; }
    Mg::UpdateTimerSettings update_timer_settings() const override;

private:
    void setup_config();

    Mg::ecs::EntityCollection entities{ 1024 };

    void
    load_model(Mg::Identifier mesh_file,
               std::initializer_list<std::pair<Mg::Identifier, Mg::Identifier>> material_bindings,
               Mg::ecs::Entity entity);

    Mg::ecs::Entity add_static_object(
        Mg::Identifier mesh_file,
        std::initializer_list<std::pair<Mg::Identifier, Mg::Identifier>> material_bindings,
        const glm::mat4& transform);

    Mg::ecs::Entity add_dynamic_object(
        Mg::Identifier mesh_file,
        std::initializer_list<std::pair<Mg::Identifier, Mg::Identifier>> material_bindings,
        glm::vec3 position,
        Mg::Rotation rotation,
        glm::vec3 scale,
        bool enable_physics);

    void create_entities();
    void generate_lights();

    void on_window_focus_change(bool is_focused);

    void render_light_debug_geometry();
    void render_skeleton_debug_geometry();

    bool m_should_exit = false;
};
