// This test scene ties many components together to create a simple scene. The more features become
// properly integrated into the engine, the smaller this sample becomes.

#pragma once

#include <mg/containers/mg_array.h>
#include <mg/containers/mg_flat_map.h>
#include <mg/core/mg_application_context.h>
#include <mg/core/mg_config.h>
#include <mg/core/mg_window.h>
#include <mg/gfx/mg_animation.h>
#include <mg/gfx/mg_billboard_renderer.h>
#include <mg/gfx/mg_bitmap_font.h>
#include <mg/gfx/mg_blur_renderer.h>
#include <mg/gfx/mg_debug_renderer.h>
#include <mg/gfx/mg_gfx_device.h>
#include <mg/gfx/mg_light.h>
#include <mg/gfx/mg_light_grid_config.h>
#include <mg/gfx/mg_material.h>
#include <mg/gfx/mg_material_pool.h>
#include <mg/gfx/mg_mesh_pool.h>
#include <mg/gfx/mg_mesh_renderer.h>
#include <mg/gfx/mg_post_process.h>
#include <mg/gfx/mg_render_command_list.h>
#include <mg/gfx/mg_render_target.h>
#include <mg/gfx/mg_skeleton.h>
#include <mg/gfx/mg_skybox_renderer.h>
#include <mg/gfx/mg_texture_pool.h>
#include <mg/gfx/mg_ui_renderer.h>
#include <mg/input/mg_input.h>
#include <mg/input/mg_keyboard.h>
#include <mg/mg_bounding_volumes.h>
#include <mg/mg_player_controller.h>
#include <mg/physics/mg_physics.h>
#include <mg/resource_cache/mg_resource_cache.h>
#include <mg/utils/mg_optional.h>

#include <variant>

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
    std::variant<size_t, Mg::Identifier> submesh_index_or_name = size_t(0);
    Mg::Identifier material_fname = "";
};

inline std::shared_ptr<Mg::ResourceCache> setup_resource_cache()
{
    return std::make_shared<Mg::ResourceCache>(std::make_unique<Mg::BasicFileLoader>("../data"));
}

class Scene : public Mg::IApplication {
public:
    Mg::ApplicationContext app;

    Scene();
    ~Scene() override;

    MG_MAKE_NON_MOVABLE(Scene);
    MG_MAKE_NON_COPYABLE(Scene);

    std::shared_ptr<Mg::ResourceCache> resource_cache = setup_resource_cache();

    Mg::gfx::MeshPool mesh_pool;
    std::shared_ptr<Mg::gfx::TexturePool> texture_pool =
        std::make_shared<Mg::gfx::TexturePool>(resource_cache);
    std::shared_ptr<Mg::gfx::MaterialPool> material_pool =
        std::make_shared<Mg::gfx::MaterialPool>(texture_pool);

    std::unique_ptr<Mg::gfx::BitmapFont> font;

    Mg::gfx::MeshRenderer mesh_renderer{ Mg::gfx::LightGridConfig{} };
    Mg::gfx::DebugRenderer debug_renderer;
    Mg::gfx::BillboardRenderer billboard_renderer;
    std::unique_ptr<Mg::gfx::BlurRenderer> blur_renderer;
    Mg::gfx::PostProcessRenderer post_renderer;
    Mg::gfx::UIRenderer ui_renderer{ { 1024, 768 } };
    Mg::gfx::SkyboxRenderer skybox_renderer;

    Mg::gfx::RenderCommandProducer render_command_producer;
    Mg::gfx::BillboardRenderList billboard_render_list;

    std::unique_ptr<Mg::gfx::TextureRenderTarget> hdr_target;

    Mg::gfx::Camera camera;
    float last_camera_z = 0.0f;
    float camera_z = 0.0f;

    std::shared_ptr<Mg::input::ButtonTracker> sample_control_button_tracker;

    std::unique_ptr<Mg::physics::World> physics_world;
    std::unique_ptr<Mg::PlayerController> player_controller;

    std::vector<Mg::gfx::Light> scene_lights;

    Mg::gfx::Material* bloom_material = nullptr;
    Mg::gfx::Material* blur_material = nullptr;
    Mg::gfx::Material* billboard_material = nullptr;
    Mg::gfx::Material* ui_material = nullptr;
    const Mg::gfx::Material* sky_material = nullptr;

    bool camera_locked = false;

    bool draw_debug = false;

    bool animate_skinned_meshes = true;

    void init();
    void simulation_step() override;
    void render(double lerp_factor) override;
    bool should_exit() const override { return m_should_exit; }
    Mg::UpdateTimerSettings update_timer_settings() const override;

private:
    void setup_config();

    bool load_mesh(Mg::Identifier file, Model& model);

    Mg::gfx::Texture2D* load_texture(Mg::Identifier file, bool sRGB);

    const Mg::gfx::Material* load_material(Mg::Identifier file,
                                           std::span<const Mg::Identifier> options);

    Model load_model(Mg::Identifier mesh_file,
                     std::span<const MaterialFileAssignment> material_files,
                     std::span<const Mg::Identifier> options);

    Model& add_scene_model(Mg::Identifier mesh_file,
                           std::span<const MaterialFileAssignment> material_files,
                           std::span<const Mg::Identifier> options);

    Model& add_dynamic_model(Mg::Identifier mesh_file,
                             std::span<const MaterialFileAssignment> material_files,
                             std::span<const Mg::Identifier> options,
                             glm::vec3 position,
                             Mg::Rotation rotation,
                             glm::vec3 scale,
                             bool enable_physics);

    void make_hdr_target(Mg::VideoMode mode);

    void load_models();
    void load_materials();
    void generate_lights();

    void on_window_focus_change(bool is_focused);
    void on_resource_reload(const Mg::FileChangedEvent& event);

    void render_light_debug_geometry();
    void render_skeleton_debug_geometry();

    using SceneModels = Mg::FlatMap<Mg::Identifier, Model, Mg::Identifier::HashCompare>;

    using DynamicModels = Mg::FlatMap<Mg::Identifier, Model, Mg::Identifier::HashCompare>;

    SceneModels scene_models;
    DynamicModels dynamic_models;

    bool m_should_exit = false;
};
