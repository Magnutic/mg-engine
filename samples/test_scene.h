#pragma once

#include <mg/containers/mg_small_vector.h>
#include <mg/core/mg_application_context.h>
#include <mg/core/mg_config.h>
#include <mg/core/mg_identifier.h>
#include <mg/core/mg_window_settings.h>
#include <mg/ecs/mg_entity.h>
#include <mg/gfx/mg_bitmap_font.h>
#include <mg/gfx/mg_material.h>
#include <mg/gfx/mg_mesh.h>
#include <mg/gfx/mg_render_command_list.h>
#include <mg/gfx/mg_render_target.h>
#include <mg/gfx/mg_simple_scene_renderer.h>
#include <mg/input/mg_input.h>
#include <mg/mg_player_controller.h>
#include <mg/physics/mg_physics.h>
#include <mg/resource_cache/mg_resource_cache.h>
#include <mg/scene/mg_common_scene_components.h>
#include <mg/scene/mg_scene.h>
#include <mg/utils/mg_interpolate_transform.h>
#include <mg/utils/mg_optional.h>

class Scene : public Mg::IApplication, Mg::SceneResources {
public:
    Scene(Mg::Config& config, Mg::Window& window);
    ~Scene() override;

    MG_MAKE_NON_MOVABLE(Scene);
    MG_MAKE_NON_COPYABLE(Scene);

    Mg::Config& config;
    Mg::Window& window;
    Mg::gfx::Camera camera;

    std::shared_ptr<Mg::input::ButtonTracker> sample_control_button_tracker;

    std::unique_ptr<Mg::physics::World> physics_world;
    std::unique_ptr<Mg::PlayerController> player_controller;
    std::unique_ptr<Mg::EditorController> editor_controller;
    Mg::IPlayerController* current_controller = nullptr;
    std::unique_ptr<Mg::physics::CharacterController> character_controller;

    std::shared_ptr<Mg::gfx::SimpleSceneRendererData> renderer_data;
    std::unique_ptr<Mg::gfx::SceneRenderer> renderer;

    int debug_visualization = 0;

    void simulation_step(Mg::ApplicationTimeInfo time_info) override;
    void render(double lerp_factor, Mg::ApplicationTimeInfo time_info) override;
    bool should_exit() const override { return m_should_exit; }
    Mg::UpdateTimerConfig update_timer_config() const override;

private:
    void setup_config();

    Mg::ecs::EntityCollection entities{ 1024 };

    const Mg::gfx::Material* default_material =
        m_material_pool->get_or_load("materials/default.hjson");

    std::unique_ptr<Mg::gfx::BitmapFont> make_font() const;

    std::shared_ptr<Mg::gfx::SimpleSceneRendererData> make_renderer_data();

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
