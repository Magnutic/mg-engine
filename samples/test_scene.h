// This test scene ties many components together to create a simple scene. The more features become
// properly integrated into the engine, the smaller this sample becomes.

#pragma once

#include <mg/core/mg_root.h>
#include <mg/core/mg_window.h>
#include <mg/gfx/mg_billboard_renderer.h>
#include <mg/gfx/mg_debug_renderer.h>
#include <mg/gfx/mg_gfx_device.h>
#include <mg/gfx/mg_light.h>
#include <mg/gfx/mg_material.h>
#include <mg/gfx/mg_material_repository.h>
#include <mg/gfx/mg_mesh_renderer.h>
#include <mg/gfx/mg_mesh_repository.h>
#include <mg/gfx/mg_post_process.h>
#include <mg/gfx/mg_render_command_list.h>
#include <mg/gfx/mg_render_target.h>
#include <mg/gfx/mg_texture_repository.h>
#include <mg/input/mg_input.h>
#include <mg/input/mg_keyboard.h>
#include <mg/resource_cache/mg_resource_cache.h>
#include <mg/utils/mg_optional.h>

constexpr double k_time_step = 1.0 / 60.0;
constexpr double k_accumulator_max_steps = 10;
constexpr size_t k_num_lights = 128;
constexpr float k_light_radius = 3.0f;

constexpr auto camera_max_vel = 0.2f;
constexpr auto camera_acc = 0.01f;
constexpr auto camera_friction = 0.005f;

struct Model {
    Mg::Transform transform;
    Mg::gfx::MeshHandle mesh;
    Mg::small_vector<Mg::gfx::MaterialBinding, 10> material_bindings;
};

struct MaterialAssignment {
    size_t submesh_index = 0;
    Mg::Identifier material_fname;
};

inline Mg::ResourceCache setup_resource_cache()
{
    return Mg::ResourceCache{ std::make_unique<Mg::BasicFileLoader>("../data") };
}

struct BlurTargets {
    std::vector<Mg::gfx::TextureRenderTarget> hor_pass_targets;
    std::vector<Mg::gfx::TextureRenderTarget> vert_pass_targets;

    Mg::gfx::TextureHandle hor_pass_target_texture;
    Mg::gfx::TextureHandle vert_pass_target_texture;
};

struct Scene {
    Mg::Root root;
    Mg::ResourceCache resource_cache = setup_resource_cache();

    Mg::gfx::MeshRepository mesh_repository;
    Mg::gfx::TextureRepository texture_repository;
    Mg::gfx::MaterialRepository material_repository;

    Mg::gfx::MeshRenderer mesh_renderer;
    Mg::gfx::DebugRenderer debug_renderer;
    Mg::gfx::BillboardRenderer billboard_renderer;
    Mg::gfx::PostProcessRenderer post_renderer;

    Mg::gfx::RenderCommandProducer render_command_producer;
    Mg::gfx::BillboardRenderList billboard_render_list;

    Mg::Opt<Mg::gfx::TextureRenderTarget> hdr_target; // Optional to defer initialisation
    BlurTargets blur_targets;

    Mg::gfx::Camera camera;

    Mg::input::InputMap input_map;

    struct State {
        glm::vec3 cam_position{};
        Mg::Rotation cam_rotation{};
        glm::vec3 cam_velocity{};
    };

    State prev_state{};
    State current_state{};

    std::vector<Model> scene_models;
    std::vector<Mg::gfx::Light> scene_lights;

    Mg::gfx::Material* blur_material;
    Mg::gfx::Material* bloom_material;
    Mg::gfx::Material* billboard_material;

    double time = 0.0;
    bool exit = false;
    bool draw_debug = false;
};
