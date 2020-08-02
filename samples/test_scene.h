// This test scene ties many components together to create a simple scene. The more features become
// properly integrated into the engine, the smaller this sample becomes.

#pragma once

#include "mg/audio/mg_sound_buffer_handle.h"
#include "mg/audio/mg_sound_source.h"
#include <mg/core/mg_config.h>
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

#include <chrono>

using Clock = std::chrono::high_resolution_clock;

constexpr double k_time_step = 1.0 / 60.0;
constexpr double k_accumulator_max_steps = 10;
constexpr size_t k_num_lights = 128;
constexpr float k_light_radius = 3.0f;

constexpr auto camera_max_vel = 0.2f;
constexpr auto camera_acc = 0.01f;
constexpr auto camera_friction = 0.005f;

class ApplicationContext {
public:
    ApplicationContext()
        : m_config("mg_engine.cfg")
        , m_window(Mg::Window::make(Mg::WindowSettings{}, "Mg Engine Test Scene"))
        , m_gfx_device(*m_window)
        , m_start_time(Clock::now())
    {}

    Mg::Window& window() { return *m_window; }
    const Mg::Window& window() const { return *m_window; }

    Mg::Config& config() { return m_config; }
    const Mg::Config& config() const { return m_config; }

    Mg::gfx::GfxDevice& gfx_device() { return m_gfx_device; }
    const Mg::gfx::GfxDevice& gfx_device() const { return m_gfx_device; }

    double time_since_init() noexcept
    {
        using namespace std::chrono;
        using seconds_double = duration<double, seconds::period>;
        return seconds_double(high_resolution_clock::now() - m_start_time).count();
    }

private:
    Mg::Config m_config;
    std::unique_ptr<Mg::Window> m_window;
    Mg::gfx::GfxDevice m_gfx_device;
    std::chrono::time_point<Clock> m_start_time;
};

struct Model {
    Mg::Transform transform;
    Mg::gfx::MeshHandle mesh;
    Mg::small_vector<Mg::gfx::MaterialBinding, 10> material_bindings;
};

struct MaterialAssignment {
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

class Scene {
public:
    ApplicationContext app;

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

    std::unique_ptr<Mg::gfx::TextureRenderTarget> hdr_target;
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

    Mg::audio::SoundSource sound_source;

    double time = 0.0;
    bool exit = false;
    bool draw_debug = false;

    void init();
    void main_loop();

private:
    void time_step();
    void render_scene(double lerp_factor);

    void setup_config();

    Mg::gfx::MeshHandle load_mesh(Mg::Identifier file);

    Mg::gfx::Texture2D* load_texture(Mg::Identifier file);

    Mg::gfx::Material* load_material(Mg::Identifier file, Mg::span<const Mg::Identifier> options);

    Model load_model(Mg::Identifier mesh_file,
                     Mg::span<const MaterialAssignment> material_files,
                     Mg::span<const Mg::Identifier> options);

    void make_input_map();

    void make_blur_targets(Mg::VideoMode video_mode);
    void make_hdr_target(Mg::VideoMode mode);

    void load_models();
    void load_materials();
    void generate_lights();

    void on_window_focus_change(bool is_focused);
    void on_resource_reload(const Mg::FileChangedEvent& event);

    void render_light_debug_geometry();
    void render_bloom();
};
