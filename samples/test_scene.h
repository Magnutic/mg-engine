// This test scene ties many components together to create a simple scene. The more features become
// properly integrated into the engine, the smaller this sample becomes.

#pragma once

#include "mg/ecs/mg_component.h"
#include "mg/ecs/mg_entity.h"
#include "mg/utils/mg_interpolate_transform.h"
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

struct MaterialFileAssignment {
    std::variant<size_t, Mg::Identifier> submesh_index_or_name = size_t(0);
    Mg::Identifier material_fname = "";
};

struct TransformComponent : Mg::ecs::BaseComponent<1> {
    glm::mat4 previous_transform = glm::mat4(1.0f);
    glm::mat4 transform = glm::mat4(1.0f);
};

struct StaticBodyComponent : Mg::ecs::BaseComponent<2> {
    Mg::physics::StaticBodyHandle physics_body;
};

struct DynamicBodyComponent : Mg::ecs::BaseComponent<3> {
    Mg::physics::DynamicBodyHandle physics_body;
};

struct MeshComponent : Mg::ecs::BaseComponent<4> {
    Mg::gfx::MeshHandle mesh = {};

    // These probably don't make sense to have in a component. They can be shared among all users of
    // a given mesh.
    Mg::small_vector<Mg::gfx::MaterialAssignment, 10> material_assignments;
    glm::vec3 centre = glm::vec3(0.0f);
    Mg::AxisAlignedBoundingBox aabb;
    glm::mat4 mesh_transform = glm::mat4(1.0f);
};

struct AnimationComponent : Mg::ecs::BaseComponent<5> {
    Mg::Opt<uint32_t> current_clip;
    float time_in_clip = 0.0f;
    Mg::gfx::SkeletonPose pose;

    // These probably don't make sense to have in a component. They can be shared among all users of
    // a given set of animations.
    Mg::gfx::Skeleton skeleton;
    Mg::small_vector<Mg::gfx::Mesh::AnimationClip, 5> clips;
};

class Entities {
public:
    explicit Entities(const uint32_t max_num_entities) : collection{ max_num_entities }
    {
        collection.init<TransformComponent,
                        StaticBodyComponent,
                        DynamicBodyComponent,
                        MeshComponent,
                        AnimationComponent>();
    }

    void update()
    {
        for (auto [entity, dynamic_body, transform] :
             collection.get_with<DynamicBodyComponent, TransformComponent>()) {
            transform.previous_transform = transform.transform;
            transform.transform = dynamic_body.physics_body.get_transform();
        }
    }

    void animate(const float time)
    {
        for (auto [entity, animation] : collection.get_with<AnimationComponent>()) {
            if (!animation.current_clip.has_value()) {
                animation.pose = animation.skeleton.get_bind_pose();
                continue;
            }

            animation.time_in_clip = time;

            const auto clip_index = animation.current_clip.value();
            Mg::gfx::animate_skeleton(animation.clips[clip_index],
                                      animation.pose,
                                      animation.time_in_clip);
        }
    }

    void add_meshes_to_render_list(Mg::gfx::RenderCommandProducer& renderlist,
                                   const float lerp_factor)
    {
        for (auto [entity, transform, mesh, animation] :
             collection.get_with<TransformComponent,
                                 MeshComponent,
                                 Mg::ecs::Maybe<AnimationComponent>>()) {
            const auto interpolated = Mg::interpolate_transforms(transform.previous_transform,
                                                                 transform.transform,
                                                                 lerp_factor) *
                                      mesh.mesh_transform;

            if (animation) {
                const auto num_joints = Mg::narrow<uint16_t>(animation->skeleton.joints().size());

                auto palette = renderlist.allocate_skinning_matrix_palette(num_joints);

                Mg::gfx::calculate_skinning_matrices(interpolated,
                                                     animation->skeleton,
                                                     animation->pose,
                                                     palette.skinning_matrices());

                renderlist.add_skinned_mesh(mesh.mesh,
                                            interpolated,
                                            mesh.material_assignments,
                                            palette);
            }
            else {
                renderlist.add_mesh(mesh.mesh, interpolated, mesh.material_assignments);
            }
        }
    }

    Mg::ecs::EntityCollection collection{ 1024 };
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

    std::shared_ptr<Mg::gfx::MeshPool> mesh_pool =
        std::make_shared<Mg::gfx::MeshPool>(resource_cache);
    std::shared_ptr<Mg::gfx::TexturePool> texture_pool =
        std::make_shared<Mg::gfx::TexturePool>(resource_cache);
    std::shared_ptr<Mg::gfx::MaterialPool> material_pool =
        std::make_shared<Mg::gfx::MaterialPool>(resource_cache, texture_pool);

    std::unique_ptr<Mg::gfx::BitmapFont> font;

    Mg::gfx::MeshRenderer mesh_renderer{ Mg::gfx::LightGridConfig{} };
    Mg::gfx::DebugRenderer debug_renderer;
    Mg::gfx::BillboardRenderer billboard_renderer;
    std::unique_ptr<Mg::gfx::BlurRenderer> blur_renderer;
    Mg::gfx::PostProcessRenderer post_renderer;
    Mg::gfx::UIRenderer ui_renderer{ { 1024, 768 } };
    Mg::gfx::SkyboxRenderer skybox_renderer;

    Mg::gfx::RenderCommandProducer render_command_producer;
    std::vector<Mg::gfx::Billboard> billboard_render_list;

    std::unique_ptr<Mg::gfx::TextureRenderTarget> hdr_target;

    Mg::gfx::Camera camera;

    std::shared_ptr<Mg::input::ButtonTracker> sample_control_button_tracker;

    std::unique_ptr<Mg::physics::World> physics_world;
    std::unique_ptr<Mg::PlayerController> player_controller;
    std::unique_ptr<Mg::physics::CharacterController> character_controller;

    std::vector<Mg::gfx::Light> scene_lights;

    Mg::gfx::Material* bloom_material = nullptr;
    Mg::gfx::Material* blur_material = nullptr;
    Mg::gfx::Material* billboard_material = nullptr;
    Mg::gfx::Material* particle_material = nullptr;
    const Mg::gfx::Material* sky_material = nullptr;

    Mg::gfx::ParticleSystem particle_system;

    bool camera_locked = false;

    int debug_visualization = 0;

    bool animate_skinned_meshes = true;

    void init();
    void simulation_step() override;
    void render(double lerp_factor) override;
    bool should_exit() const override { return m_should_exit; }
    Mg::UpdateTimerSettings update_timer_settings() const override;

private:
    void setup_config();

    Entities entities{ 1024 };

    void load_model(Mg::Identifier mesh_file,
                    std::span<const MaterialFileAssignment> material_files,
                    Mg::ecs::Entity entity);

    Mg::ecs::Entity add_static_object(Mg::Identifier mesh_file,
                                      std::span<const MaterialFileAssignment> material_files,
                                      const glm::mat4& transform);

    Mg::ecs::Entity add_dynamic_object(Mg::Identifier mesh_file,
                                       std::span<const MaterialFileAssignment> material_files,
                                       glm::vec3 position,
                                       Mg::Rotation rotation,
                                       glm::vec3 scale,
                                       bool enable_physics);

    void make_hdr_target(Mg::VideoMode mode);

    void create_entities();
    void load_materials();
    void generate_lights();

    void on_window_focus_change(bool is_focused);

    void render_light_debug_geometry();
    void render_skeleton_debug_geometry();

    bool m_should_exit = false;
};
