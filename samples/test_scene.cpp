#include "test_scene.h"

#include <mg/core/mg_config.h>
#include <mg/core/mg_log.h>
#include <mg/gfx/mg_material_repository.h>
#include <mg/gfx/mg_mesh_repository.h>
#include <mg/gfx/mg_texture_repository.h>
#include <mg/resources/mg_mesh_resource.h>
#include <mg/resources/mg_shader_resource.h>
#include <mg/resources/mg_texture_resource.h>

#include <fmt/core.h>

using namespace Mg;
using namespace Mg::gfx;
using namespace Mg::input;

// Global scene pointer. Since this is just a small sample, I feel okay with using a global.
Scene* g_scene;

inline void setup_config()
{
    auto& cfg = g_scene->root.config();
    cfg.set_default_value("mouse_sensitivity_x", 0.002f);
    cfg.set_default_value("mouse_sensitivity_y", 0.002f);
}

inline Mg::gfx::MeshHandle load_mesh(Identifier file)
{
    auto access = g_scene->resource_cache.access_resource<MeshResource>(file);
    return g_scene->root.gfx_device().mesh_repository().create(*access);
}

inline Mg::gfx::TextureHandle load_texture(std::string_view file)
{
    auto file_name = Identifier::from_runtime_string(fmt::format("textures/{}.dds", file));
    auto access    = g_scene->resource_cache.access_resource<TextureResource>(file_name);
    return g_scene->root.gfx_device().texture_repository().create(*access);
}

inline Mg::gfx::Material* load_material(Identifier file, std::initializer_list<Identifier> options)
{
    auto handle = g_scene->resource_cache.resource_handle<ShaderResource>(
        "shaders/default.mgshader");
    Material* m = g_scene->root.gfx_device().material_repository().create(file, handle);

    for (auto o : options) { m->set_option(o, true); }

    m->set_sampler("sampler_diffuse", load_texture(fmt::format("{}_da", file.c_str())));
    m->set_sampler("sampler_normal", load_texture(fmt::format("{}_n", file.c_str())));
    m->set_sampler("sampler_specular", load_texture(fmt::format("{}_s", file.c_str())));

    return m;
}

inline Model load_model(Identifier                        mesh_file,
                        span<const MaterialAssignment>    material_files,
                        std::initializer_list<Identifier> options)
{
    Model model;

    model.mesh = load_mesh(mesh_file);

    for (auto&& [submesh_index, material_fname] : material_files) {
        model.material_bindings.push_back(
            MaterialBinding{ submesh_index, load_material(material_fname, options) });
    }

    return model;
}

inline InputMap make_input_map(Window& w)
{
    const auto& kb    = w.keyboard;
    const auto& mouse = w.mouse;

    InputMap input;

    // clang-format off
    input.bind("forward",          kb.key(Keyboard::Key::W));
    input.bind("backward",         kb.key(Keyboard::Key::S));
    input.bind("left",             kb.key(Keyboard::Key::A));
    input.bind("right",            kb.key(Keyboard::Key::D));
    input.bind("up",               kb.key(Keyboard::Key::Space));
    input.bind("down",             kb.key(Keyboard::Key::LeftControl));
    input.bind("fullscreen",       kb.key(Keyboard::Key::F4));
    input.bind("exit",             kb.key(Keyboard::Key::Esc));
    input.bind("toggle_debug_vis", kb.key(Keyboard::Key::F));

    input.bind("look_x", mouse.axis(Mouse::Axis::delta_x));
    input.bind("look_y", mouse.axis(Mouse::Axis::delta_y));
    // clang-format on

    return input;
}

Mg::gfx::TextureRenderTarget make_hdr_target(VideoMode mode)
{
    RenderTargetParams params{};
    params.filter_mode      = TextureFilterMode::Linear;
    params.width            = mode.width;
    params.height           = mode.height;
    params.render_target_id = "HDR.colour";
    params.texture_format   = RenderTargetParams::Format::RGBA16F;

    TextureRepository& tex_repo = GfxDevice::get().texture_repository();

    TextureHandle colour_target = tex_repo.create_render_target(params);

    params.render_target_id = "HDR.depth";
    params.texture_format   = RenderTargetParams::Format::Depth24;

    TextureHandle depth_target = tex_repo.create_render_target(params);

    return TextureRenderTarget::with_colour_and_depth_targets(colour_target, depth_target);
}

void init()
{
    setup_config();

    auto& window = g_scene->root.window();

    // Configure window
    {
        window.set_focus_callback([](bool is_focused) {
            if (is_focused) { g_scene->resource_cache.refresh(); }
        });

        WindowSettings window_settings = read_display_settings(g_scene->root.config());
        window.set_title("Mg Engine Example Application");
        window.apply_settings(window_settings);
        window.set_cursor_lock_mode(CursorLockMode::LOCKED);
    }

    g_scene->resource_cache.set_resource_reload_callback([](const FileChangedEvent& event) {
        switch (event.resource.type_id().hash()) {
        case Identifier("TextureResource").hash():
            g_scene->root.gfx_device().texture_repository().update(
                static_cast<TextureResource&>(event.resource));
            break;
        case Identifier("ShaderResource").hash(): g_scene->mesh_renderer.drop_shaders(); break;
        default:
            g_log.write_verbose(fmt::format("Resource '{}' was updated, but ignored.",
                                            event.resource.resource_id().str_view()));
        }
    });

    g_scene->hdr_target = make_hdr_target(window.settings().video_mode);

    g_scene->root.gfx_device().set_clear_colour(0.0125f, 0.01275f, 0.025f);

    g_scene->camera.set_aspect_ratio(window.aspect_ratio());
    g_scene->camera.field_of_view         = { FieldOfView::DEGREES, 80.0f };
    g_scene->current_state.cam_position.z = 1.0f;
    g_scene->prev_state                   = g_scene->current_state;

    g_scene->input_map = make_input_map(window);

    // Load models
    {
        MaterialAssignment scene_mats[] = { { 0, "buildings/GreenBrick" },
                                            { 1, "buildings/W31_1" },
                                            { 2, "buildings/BigWhiteBricks" },
                                            { 3, "buildings/GreenBrick" } };

        MaterialAssignment hest_mats[]    = { { 0, "actors/HestDraugr" } };
        MaterialAssignment narmask_mats[] = { { 0, "actors/narmask" } };

        g_scene->scene_models.emplace_back(
            load_model("meshes/misc/test_scene_2.mgm", scene_mats, { "PARALLAX" }));

        {
            Model& hest_model = g_scene->scene_models.emplace_back(
                load_model("meshes/misc/hestdraugr.mgm", hest_mats, { "RIM_LIGHT" }));

            hest_model.transform.position.x = 3.0f;
        }

        {
            Model& narmask_model = g_scene->scene_models.emplace_back(
                load_model("meshes/misc/narmask.mgm", narmask_mats, { "RIM_LIGHT" }));

            narmask_model.transform.position.x -= 2.0f;
            narmask_model.transform.rotation.yaw(glm::half_pi<float>());
        }
    }

    // Create post-process material
    {
        auto handle = g_scene->resource_cache.resource_handle<ShaderResource>(
            "shaders/post_process_test.mgshader");
        g_scene->post_material = g_scene->root.gfx_device().material_repository().create(
            "PostProcessMaterial", handle);
    }

    g_scene->light_billboard_texture = load_texture("light_t");

    main_loop();
}

void time_step()
{
    auto& input      = g_scene->input_map;
    auto& window     = g_scene->root.window();
    auto& camera     = g_scene->camera;
    auto& prev_state = g_scene->prev_state;
    auto& state      = g_scene->current_state;
    auto& config     = g_scene->root.config();

    window.poll_input_events();
    input.refresh();

    if (input.was_pressed("exit")) { g_scene->exit = true; }

    prev_state = state;

    // Mouselook
    float mouse_delta_x = input.state("look_x");
    float mouse_delta_y = input.state("look_y");
    mouse_delta_x *= config.as<float>("mouse_sensitivity_x");
    mouse_delta_y *= config.as<float>("mouse_sensitivity_y");

    if (!window.is_cursor_locked_to_window()) {
        mouse_delta_x = 0.0f;
        mouse_delta_y = 0.0f;
    }

    float cam_pitch = state.cam_rotation.pitch() - mouse_delta_y;
    float cam_yaw   = state.cam_rotation.yaw() - mouse_delta_x;

    state.cam_rotation = Rotation{
        { glm::clamp(cam_pitch, -glm::half_pi<float>() + 0.0001f, glm::half_pi<float>() - 0.0001f),
          0.0f,
          cam_yaw }
    };

    // Camera movement
    glm::vec3 vec_forward = camera.rotation.forward();
    glm::vec3 vec_right   = camera.rotation.right();
    glm::vec3 vec_up      = camera.rotation.up();

    auto max_vel  = 0.2f;
    auto acc      = 0.01f;
    auto friction = 0.005f;

    auto forward_acc = acc * (input.is_held("forward") - input.is_held("backward"));
    auto right_acc   = acc * (input.is_held("right") - input.is_held("left"));
    auto up_acc      = acc * (input.is_held("up") - input.is_held("down"));

    if (glm::distance(state.cam_velocity, {}) > friction) {
        state.cam_velocity -= glm::normalize(state.cam_velocity) * friction;
    }
    else {
        state.cam_velocity = {};
    }

    state.cam_velocity += vec_forward * forward_acc;
    state.cam_velocity += vec_right * right_acc;
    state.cam_velocity += vec_up * up_acc;

    if (auto vel = glm::distance(state.cam_velocity, {}); vel > max_vel) {
        state.cam_velocity = glm::normalize(state.cam_velocity) * max_vel;
    }

    state.cam_position += state.cam_velocity;

    // Fullscreen switching
    if (input.was_pressed("fullscreen")) {
        WindowSettings s = window.settings();
        s.fullscreen     = !s.fullscreen;
        s.video_mode     = {}; // reset resolution etc. to defaults
        window.apply_settings(s);
        camera.set_aspect_ratio(window.aspect_ratio());
        g_scene->hdr_target = make_hdr_target(window.settings().video_mode);
        window.release_cursor();
    }

    if (input.was_pressed("toggle_debug_vis")) { g_scene->draw_debug = !g_scene->draw_debug; }
}

inline Scene::State lerp(const Scene::State& fst, const Scene::State& snd, double x)
{
    Scene::State output;
    output.cam_position = glm::mix(fst.cam_position, snd.cam_position, x);
    output.cam_rotation = Rotation::mix(fst.cam_rotation, snd.cam_rotation, float(x));

    return output;
}

inline void add_to_render_list(const Model& model, RenderCommandList& renderlist)
{
    renderlist.add_mesh(model.mesh, model.transform, model.material_bindings);
}

void render_scene(double lerp_factor)
{
    auto& gfx = g_scene->root.gfx_device();

    Scene::State render_state = lerp(g_scene->prev_state, g_scene->current_state, lerp_factor);
    g_scene->camera.position  = render_state.cam_position;
    g_scene->camera.rotation  = render_state.cam_rotation;

    // Draw meshes
    RenderCommandList& render_list = g_scene->render_list;
    render_list.clear();
    for (auto&& model : g_scene->scene_models) { add_to_render_list(model, render_list); }

    std::vector<Light> lights;

    // Create a lot of random lights
    // I know rand / srand is not much good but this is just a little sample...
    std::srand(222);
    g_scene->billboard_render_list.clear();
    for (size_t i = 0; i < k_num_lights; ++i) {
        auto pos = glm::vec3(rand(), rand(), 0.0f);
        pos /= RAND_MAX;
        pos -= glm::vec3(0.5f, 0.5f, 0.0f);
        pos *= 15.0f;
        pos.z += 1.125f;

        glm::vec4 colour(rand(), rand(), rand(), RAND_MAX);
        colour /= RAND_MAX;

        float offset = float(rand()) / RAND_MAX * 7.0f;
        float s      = float(sin(g_scene->time * 0.5 + offset));

        pos.z += s;

        {
            Billboard& billboard = g_scene->billboard_render_list.add();
            billboard.pos        = pos;
            billboard.colour     = colour;
            billboard.radius     = 0.1f;
        }

        lights.push_back(make_point_light(pos, colour * 100.0f, k_light_radius));
    }

    render_list.frustum_cull_draw_list(g_scene->camera);
    render_list.sort_draw_list(g_scene->camera, SortFunc::NEAR_TO_FAR);

    g_scene->hdr_target->bind();
    gfx.clear();

    auto time = float(g_scene->root.time_since_init());
    g_scene->mesh_renderer.render(g_scene->camera, render_list, lights, { time, -6.0 });

    g_scene->billboard_renderer.render(g_scene->camera,
                                       g_scene->billboard_render_list,
                                       g_scene->light_billboard_texture,
                                       BillboardSetting::A_TEST);

    g_scene->root.window().render_target.bind();
    gfx.clear();

    g_scene->post_renderer.post_process(*g_scene->post_material,
                                        g_scene->hdr_target->colour_target(),
                                        g_scene->hdr_target->depth_target(),
                                        g_scene->camera.depth_range().near(),
                                        g_scene->camera.depth_range().far());

    if (g_scene->draw_debug) {
        gfx.set_depth_test(DepthFunc::NONE);

        for (const Light& light : lights) {
            if (light.vector.w == 0.0) { continue; }
            DebugRenderer::EllipsoidDrawParams params;
            params.centre     = glm::vec3(light.vector);
            params.colour     = glm::vec4(normalize(light.colour), 0.5f);
            params.dimensions = glm::vec3(std::sqrt(light.range_sqr));
            params.wireframe  = true;
            g_scene->debug_renderer.draw_ellipsoid(g_scene->camera, params);
        }

        gfx.set_depth_test(DepthFunc::LESS);
    }

    g_scene->root.window().refresh();
}

void main_loop()
{
    double accumulator    = 0.0;
    double last_fps_write = g_scene->time;
    size_t n_frames       = 0;

    while (!g_scene->exit) {
        double last_time = g_scene->time;
        g_scene->time    = g_scene->root.time_since_init();

        ++n_frames;

        if (g_scene->time - last_fps_write > 1.0) {
            g_log.write_verbose(fmt::format("{} FPS", n_frames));
            last_fps_write = g_scene->time;
            n_frames       = 0;
        }

        accumulator += g_scene->time - last_time;
        accumulator = std::min(accumulator, k_accumulator_max_steps * k_time_step);

        while (accumulator >= k_time_step) {
            time_step();
            accumulator -= k_time_step;
        }

        render_scene(accumulator / k_time_step);
    }
}

int main(int /*argc*/, char* /*argv*/ [])
{
    Scene scene;
    g_scene = &scene;
    init();
}

