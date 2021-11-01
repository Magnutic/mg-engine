// This test scene ties many components together to create a simple scene. The more features become
// properly integrated into the engine, the smaller this sample becomes.

#pragma once

#include "mg/gfx/mg_light_grid_config.h"
#include <mg/containers/mg_array.h>
#include <mg/containers/mg_flat_map.h>
#include <mg/core/mg_application_context.h>
#include <mg/core/mg_config.h>
#include <mg/core/mg_window.h>
#include <mg/gfx/mg_animation.h>
#include <mg/gfx/mg_billboard_renderer.h>
#include <mg/gfx/mg_bitmap_font.h>
#include <mg/gfx/mg_debug_renderer.h>
#include <mg/gfx/mg_gfx_device.h>
#include <mg/gfx/mg_light.h>
#include <mg/gfx/mg_material.h>
#include <mg/gfx/mg_material_pool.h>
#include <mg/gfx/mg_mesh_pool.h>
#include <mg/gfx/mg_mesh_renderer.h>
#include <mg/gfx/mg_post_process.h>
#include <mg/gfx/mg_render_command_list.h>
#include <mg/gfx/mg_render_target.h>
#include <mg/gfx/mg_skeleton.h>
#include <mg/gfx/mg_texture_pool.h>
#include <mg/gfx/mg_ui_renderer.h>
#include <mg/input/mg_input.h>
#include <mg/input/mg_keyboard.h>
#include <mg/mg_bounding_volumes.h>
#include <mg/physics/mg_character_controller.h>
#include <mg/physics/mg_physics.h>
#include <mg/resource_cache/mg_resource_cache.h>
#include <mg/utils/mg_optional.h>

struct Model {
    Model();
    ~Model();
    MG_MAKE_DEFAULT_MOVABLE(Model);
    MG_MAKE_NON_COPYABLE(Model);

    void update();

    using AnimationClips = Mg::small_vector<Mg::gfx::Mesh::AnimationClip, 5>;

    glm::mat4 transform = glm::mat4(1.0f);
    glm::mat4 vis_transform = glm::mat4(1.0f);
    Mg::gfx::MeshHandle mesh = {};
    Mg::small_vector<Mg::gfx::MaterialAssignment, 10> material_assignments;
    Mg::Opt<Mg::gfx::Skeleton> skeleton;
    Mg::Opt<Mg::gfx::SkeletonPose> pose;
    AnimationClips clips;
    Mg::Identifier id;
    glm::vec3 centre = glm::vec3(0.0f);
    Mg::AxisAlignedBoundingBox aabb;

    Mg::Opt<Mg::physics::PhysicsBodyHandle> physics_body;
};

struct MaterialFileAssignment {
    size_t submesh_index = 0;
    Mg::Identifier material_fname = "";
};

inline Mg::ResourceCache setup_resource_cache()
{
    return Mg::ResourceCache{ std::make_unique<Mg::BasicFileLoader>("../data") };
}

using RenderPassTargets = std::vector<std::unique_ptr<Mg::gfx::TextureRenderTarget>>;

struct BlurTargets {
    RenderPassTargets hor_pass_targets;
    RenderPassTargets vert_pass_targets;

    Mg::gfx::Texture2D* hor_pass_target_texture;
    Mg::gfx::Texture2D* vert_pass_target_texture;
};

class Actor {
public:
    constexpr static float radius = 0.5f;
    constexpr static float height = 1.8f;
    constexpr static float step_height = 0.6f;

    explicit Actor(Mg::physics::World& physics_world, const glm::vec3& position, const float mass)
        : character_controller("Actor", physics_world, {})
    {
        character_controller.mass = mass;
        character_controller.set_position(position);
    }

    void update(glm::vec3 acceleration, float jump_impulse);

    glm::vec3 position(const float interpolate = 1.0f) const
    {
        return character_controller.get_position(interpolate);
    }

    float current_height() const { return character_controller.current_height(); }

    Mg::physics::CharacterController character_controller;
    float max_horizontal_speed = 10.0f;
    float friction = 0.6f;
};

class Scene : public Mg::IApplication {
public:
    Mg::ApplicationContext app;

    Scene();
    ~Scene() override;

    MG_MAKE_NON_MOVABLE(Scene);
    MG_MAKE_NON_COPYABLE(Scene);

    Mg::ResourceCache resource_cache = setup_resource_cache();

    Mg::gfx::MeshPool mesh_pool;
    Mg::gfx::TexturePool texture_pool;
    Mg::gfx::MaterialPool material_pool;

    std::unique_ptr<Mg::gfx::BitmapFont> font;

    Mg::gfx::MeshRenderer mesh_renderer{ Mg::gfx::LightGridConfig{} };
    Mg::gfx::DebugRenderer debug_renderer;
    Mg::gfx::BillboardRenderer billboard_renderer;
    Mg::gfx::PostProcessRenderer post_renderer;
    Mg::gfx::UIRenderer ui_renderer{ { 1024, 768 } };

    Mg::gfx::RenderCommandProducer render_command_producer;
    Mg::gfx::BillboardRenderList billboard_render_list;

    std::unique_ptr<Mg::gfx::TextureRenderTarget> hdr_target;
    BlurTargets blur_targets;

    Mg::gfx::Camera camera;
    float last_camera_z = 0.0f;
    float camera_z = 0.0f;

    Mg::input::InputMap input_map;

    std::unique_ptr<Mg::physics::World> physics_world;
    std::unique_ptr<Actor> actor;

    std::vector<Mg::gfx::Light> scene_lights;

    Mg::gfx::Material* blur_material = nullptr;
    Mg::gfx::Material* bloom_material = nullptr;
    Mg::gfx::Material* billboard_material = nullptr;
    Mg::gfx::Material* ui_material = nullptr;

    bool camera_locked = false;

    bool draw_debug = false;

    void init();
    void simulation_step() override;
    void render(double lerp_factor) override;
    bool should_exit() const override { return m_should_exit; }
    Mg::UpdateTimerSettings update_timer_settings() const override;

private:
    Mg::input::Mouse::CursorPosition last_cursor_position;

    void setup_config();

    bool load_mesh(Mg::Identifier file, Model& model);

    Mg::gfx::Texture2D* load_texture(Mg::Identifier file);

    Mg::gfx::Material* load_material(Mg::Identifier file, Mg::span<const Mg::Identifier> options);

    Model load_model(Mg::Identifier mesh_file,
                     Mg::span<const MaterialFileAssignment> material_files,
                     Mg::span<const Mg::Identifier> options);

    Model& add_scene_model(Mg::Identifier mesh_file,
                           Mg::span<const MaterialFileAssignment> material_files,
                           Mg::span<const Mg::Identifier> options);

    Model& add_dynamic_model(Mg::Identifier mesh_file,
                             Mg::span<const MaterialFileAssignment> material_files,
                             Mg::span<const Mg::Identifier> options,
                             glm::vec3 position,
                             Mg::Rotation rotation,
                             glm::vec3 scale,
                             bool enable_physics);

    void make_input_map();

    void make_blur_targets(Mg::VideoMode video_mode);
    void make_hdr_target(Mg::VideoMode mode);

    void load_models();
    void load_materials();
    void generate_lights();

    void on_window_focus_change(bool is_focused);
    void on_resource_reload(const Mg::FileChangedEvent& event);

    void render_light_debug_geometry();
    void render_skeleton_debug_geometry();
    void render_bloom();

    using SceneModels = Mg::FlatMap<Mg::Identifier, Model, Mg::Identifier::HashCompare>;

    using DynamicModels = Mg::FlatMap<Mg::Identifier, Model, Mg::Identifier::HashCompare>;

    SceneModels scene_models;
    DynamicModels dynamic_models;

    bool m_should_exit = false;
};
