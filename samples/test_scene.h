// This test scene ties many components together to create a simple scene. As more features become
// properly integrated into the engine, the smaller this sample becomes.

#pragma once

#include <mg/core/mg_resource_cache.h>
#include <mg/core/mg_root.h>
#include <mg/core/mg_window.h>
#include <mg/gfx/mg_billboard_renderer.h>
#include <mg/gfx/mg_debug_renderer.h>
#include <mg/gfx/mg_gfx_device.h>
#include <mg/gfx/mg_light.h>
#include <mg/gfx/mg_lit_mesh_renderer.h>
#include <mg/gfx/mg_material.h>
#include <mg/gfx/mg_post_process.h>
#include <mg/gfx/mg_render_command_list.h>
#include <mg/gfx/mg_render_target.h>
#include <mg/input/mg_input.h>
#include <mg/input/mg_keyboard.h>

static const double k_time_step             = 1.0 / 60.0;
static const double k_accumulator_max_steps = 10;
static const size_t k_num_lights            = 256;
static const float  k_light_radius          = 1.5f;

struct Model {
    Mg::Transform                                  transform;
    Mg::gfx::MeshHandle                            mesh;
    Mg::small_vector<Mg::gfx::MaterialBinding, 10> material_bindings;
};

struct MaterialAssignment {
    size_t         submesh_index = 0;
    Mg::Identifier material_fname;
};

inline Mg::ResourceCache setup_resource_cache()
{
    return Mg::ResourceCache{ std::make_unique<Mg::BasicFileLoader>("../data") };
}

struct Scene {
    Mg::Root          root;
    Mg::ResourceCache resource_cache = setup_resource_cache();

    Mg::gfx::MeshRenderer        mesh_renderer;
    Mg::gfx::DebugRenderer       debug_renderer;
    Mg::gfx::BillboardRenderer   billboard_renderer;
    Mg::gfx::PostProcessRenderer post_renderer;

    Mg::gfx::RenderCommandList   render_list;
    Mg::gfx::BillboardRenderList billboard_render_list;

    std::optional<Mg::gfx::TextureRenderTarget> hdr_target; // Optional to defer initialisation

    Mg::gfx::Camera camera;

    Mg::input::InputMap input_map;

    struct State {
        glm::vec3    cam_position;
        Mg::Rotation cam_rotation;
        glm::vec3    cam_velocity;
    };

    State prev_state;
    State current_state;

    std::vector<Model> scene_models;

    Mg::gfx::Material*     post_material;
    Mg::gfx::TextureHandle light_billboard_texture;

    double time       = 0.0;
    bool   exit       = false;
    bool   draw_debug = false;
};

void init();

void render_scene(double lerp_factor);

void time_step();

void main_loop();
