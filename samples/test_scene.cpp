#include "test_scene.h"
#include "mg/gfx/mg_blend_modes.h"
#include "mg/gfx/mg_render_target.h"

#include <mg/audio/mg_audio_context.h>
#include <mg/core/mg_config.h>
#include <mg/core/mg_identifier.h>
#include <mg/core/mg_log.h>
#include <mg/gfx/mg_gfx_debug_group.h>
#include <mg/gfx/mg_mesh_renderer.h>
#include <mg/gfx/mg_texture2d.h>
#include <mg/resource_cache/internal/mg_resource_entry_base.h>
#include <mg/resource_cache/mg_resource_access_guard.h>
#include <mg/resources/mg_mesh_resource.h>
#include <mg/resources/mg_shader_resource.h>
#include <mg/resources/mg_sound_resource.h>
#include <mg/resources/mg_texture_resource.h>
#include <mg/utils/mg_angle.h>
#include <mg/utils/mg_math_utils.h>

#include <fmt/core.h>

namespace {

std::array<float, 3> camera_acceleration(Mg::input::InputMap& input_map)
{
    auto forward_acc = camera_acc * (input_map.is_held("forward") - input_map.is_held("backward"));
    auto right_acc = camera_acc * (input_map.is_held("right") - input_map.is_held("left"));
    auto up_acc = camera_acc * (input_map.is_held("up") - input_map.is_held("down"));

    return { forward_acc, right_acc, up_acc };
}

void add_to_render_list(const Model& model, Mg::gfx::RenderCommandProducer& renderlist)
{
    renderlist.add_mesh(model.mesh, model.transform, model.material_bindings);
}

Scene::State lerp(const Scene::State& fst, const Scene::State& snd, double x)
{
    Scene::State output;
    output.cam_position = glm::mix(fst.cam_position, snd.cam_position, x);
    output.cam_rotation = Mg::Rotation::mix(fst.cam_rotation, snd.cam_rotation, float(x));

    return output;
}

} // namespace

void Scene::init()
{
    MG_GFX_DEBUG_GROUP("Scene::init")

    using namespace Mg::literals;

    setup_config();

    // Configure window
    {
        app.window().set_focus_callback(
            [this](bool is_focused) { on_window_focus_change(is_focused); });

        Mg::WindowSettings window_settings = read_display_settings(app.config());
        app.window().set_title("Mg Engine Example Application");
        app.window().apply_settings(window_settings);
        app.window().set_cursor_lock_mode(Mg::CursorLockMode::LOCKED);
    }

    resource_cache.set_resource_reload_callback(
        [this](const Mg::FileChangedEvent& event) { on_resource_reload(event); });

    make_hdr_target(app.window().settings().video_mode);
    make_blur_targets(app.window().settings().video_mode);

    app.gfx_device().set_clear_colour(0.0125f, 0.01275f, 0.025f, 1.0f);

    camera.set_aspect_ratio(app.window().aspect_ratio());
    camera.field_of_view = 80_degrees;
    current_state.cam_position.z = 1.0f;
    prev_state = current_state;

    make_input_map();

    load_models();
    load_materials();
    generate_lights();

    main_loop();
}

void Scene::main_loop()
{
    double accumulator = 0.0;
    double last_fps_write = time;
    size_t n_frames = 0;

    while (!exit) {
        double last_time = time;
        time = app.time_since_init();

        ++n_frames;

        if (time - last_fps_write > 1.0) {
            Mg::g_log.write_verbose(fmt::format("{} FPS", n_frames));
            last_fps_write = time;
            n_frames = 0;
        }

        accumulator += time - last_time;
        accumulator = std::min(accumulator, k_accumulator_max_steps * k_time_step);

        while (accumulator >= k_time_step) {
            time_step();
            accumulator -= k_time_step;
        }

        render_scene(accumulator / k_time_step);
    }
}

void Scene::time_step()
{
    using namespace Mg::literals;

    app.window().poll_input_events();
    input_map.refresh();

    if (input_map.was_pressed("exit") || app.window().should_close_flag()) {
        exit = true;
    }

    prev_state = current_state;

    // Mouselook
    float mouse_delta_x = input_map.state("look_x");
    float mouse_delta_y = input_map.state("look_y");
    mouse_delta_x *= app.config().as<float>("mouse_sensitivity_x");
    mouse_delta_y *= app.config().as<float>("mouse_sensitivity_y");

    if (!app.window().is_cursor_locked_to_window()) {
        mouse_delta_x = 0.0f;
        mouse_delta_y = 0.0f;
    }

    Mg::Angle cam_pitch = current_state.cam_rotation.pitch() -
                          Mg::Angle::from_radians(mouse_delta_y);
    Mg::Angle cam_yaw = current_state.cam_rotation.yaw() - Mg::Angle::from_radians(mouse_delta_x);

    cam_pitch = Mg::Angle::clamp(cam_pitch, -90_degrees, 90_degrees);

    // Set rotation from euler angles, clamping pitch between straight down & straight up.
    current_state.cam_rotation = Mg::Rotation{ { cam_pitch.radians(), 0.0f, cam_yaw.radians() } };

    // Camera movement
    auto const [forward_acc, right_acc, up_acc] = camera_acceleration(input_map);

    if (glm::distance(current_state.cam_velocity, {}) > camera_friction) {
        current_state.cam_velocity -= glm::normalize(current_state.cam_velocity) * camera_friction;
    }
    else {
        current_state.cam_velocity = {};
    }

    const glm::vec3 vec_forward = camera.rotation.forward();
    const glm::vec3 vec_right = camera.rotation.right();
    const glm::vec3 vec_up = camera.rotation.up();

    current_state.cam_velocity += vec_forward * forward_acc;
    current_state.cam_velocity += vec_right * right_acc;
    current_state.cam_velocity += vec_up * up_acc;

    if (auto vel = glm::distance(current_state.cam_velocity, {}); vel > camera_max_vel) {
        current_state.cam_velocity = glm::normalize(current_state.cam_velocity) * camera_max_vel;
    }

    current_state.cam_position += current_state.cam_velocity;

    // Fullscreen switching
    if (input_map.was_pressed("fullscreen")) {
        Mg::WindowSettings s = app.window().settings();
        s.fullscreen = !s.fullscreen;
        s.video_mode = {}; // reset resolution etc. to defaults
        app.window().apply_settings(s);
        camera.set_aspect_ratio(app.window().aspect_ratio());

        // Dispose of old render target textures. TODO: RAII
        {
            texture_repository.destroy(hdr_target->colour_target());
            texture_repository.destroy(hdr_target->depth_target());
            texture_repository.destroy(blur_targets.hor_pass_target_texture);
            texture_repository.destroy(blur_targets.vert_pass_target_texture);
        }

        make_hdr_target(app.window().settings().video_mode);
        make_blur_targets(app.window().settings().video_mode);

        app.window().release_cursor();
    }

    if (input_map.was_pressed("toggle_debug_vis")) {
        draw_debug = !draw_debug;
    }

#if 0
    {
        Mg::audio::ListenerState listener_state;
        listener_state.position = current_state.cam_position;
        listener_state.forward = current_state.cam_rotation.forward();
        listener_state.up = current_state.cam_rotation.up();
        listener_state.velocity = (current_state.cam_position - prev_state.cam_position) /
                                  float(k_time_step);

        Mg::audio::AudioContext::get().set_listener_state(listener_state);
    }

    Mg::audio::AudioContext::get().check_for_errors();
#endif
}

void Scene::render_scene(const double lerp_factor)
{
    MG_GFX_DEBUG_GROUP("Scene::render_scene")

    Scene::State render_state = lerp(prev_state, current_state, lerp_factor);
    camera.position = render_state.cam_position;
    camera.rotation = render_state.cam_rotation;

    // Draw meshes and billboards
    {
        render_command_producer.clear();

        for (auto&& model : scene_models) {
            add_to_render_list(model, render_command_producer);
        }

        hdr_target->bind();
        app.gfx_device().clear();

        const auto& commands = render_command_producer.finalise(camera,
                                                                Mg::gfx::SortFunc::NEAR_TO_FAR);

        Mg::gfx::RenderParameters params = {};
        params.current_time = Mg::narrow_cast<float>(time);
        params.camera_exposure = -6.0;

        mesh_renderer.render(camera, commands, scene_lights, params);

        billboard_renderer.render(camera, billboard_render_list, *billboard_material);
    }

    render_bloom();

    // Apply tonemap and render to window render target.
    {
        app.window().render_target.bind();
        app.gfx_device().clear();

        bloom_material->set_sampler("sampler_bloom",
                                    blur_targets.vert_pass_target_texture->handle());

        post_renderer.post_process(post_renderer.get_guard(),
                                   *bloom_material,
                                   hdr_target->colour_target()->handle());
    }

    // Draw UI
    {
        Mg::gfx::UIRenderer::TransformParams transform_params = {};
        transform_params.position = { 0.5f, 0.5f };
        transform_params.anchor = { 0.5f, 0.5f };
        transform_params.size = { 0.125f, 0.125f };
        transform_params.rotation = Mg::Angle::from_radians(Mg::narrow_cast<float>(time));
        ui_material->set_parameter("opacity", 0.5f);
        ui_renderer.aspect_ratio(app.window().aspect_ratio());
        ui_renderer.draw_rectangle(transform_params,
                                   ui_material,
                                   Mg::gfx::blend_mode_constants::bm_add);
    }

    // Debug geometry
    if (draw_debug) {
        render_light_debug_geometry();
    }

    app.window().refresh();
}

void Scene::setup_config()
{
    auto& cfg = app.config();
    cfg.set_default_value("mouse_sensitivity_x", 0.002f);
    cfg.set_default_value("mouse_sensitivity_y", 0.002f);
}

Mg::gfx::MeshHandle Scene::load_mesh(Mg::Identifier file)
{
    const Mg::Opt<Mg::gfx::MeshHandle> mesh_handle = mesh_repository.get(file);
    if (mesh_handle.has_value()) {
        return mesh_handle.value();
    }

    const Mg::ResourceAccessGuard access = resource_cache.access_resource<Mg::MeshResource>(file);
    return mesh_repository.create(*access);
}

Mg::gfx::Texture2D* Scene::load_texture(Mg::Identifier file)
{
    Mg::gfx::Texture2D* texture = texture_repository.get(file);
    if (texture) {
        return texture;
    }

    const Mg::ResourceAccessGuard access =
        resource_cache.access_resource<Mg::TextureResource>(file);
    return texture_repository.create(*access);
}

Mg::gfx::Material* Scene::load_material(Mg::Identifier file, Mg::span<const Mg::Identifier> options)
{
    auto handle = resource_cache.resource_handle<Mg::ShaderResource>("shaders/default.mgshader");
    Mg::gfx::Material* m = material_repository.create(file, handle);

    for (auto o : options) {
        m->set_option(o, true);
    }

    const std::string diffuse_filename = fmt::format("textures/{}_da.dds", file.str_view());
    const std::string normal_filename = fmt::format("textures/{}_n.dds", file.str_view());
    const std::string specular_filename = fmt::format("textures/{}_s.dds", file.str_view());

    const Mg::gfx::Texture2D* diffuse_texture =
        load_texture(Mg::Identifier::from_runtime_string(diffuse_filename));
    const Mg::gfx::Texture2D* normal_texture =
        load_texture(Mg::Identifier::from_runtime_string(normal_filename));
    const Mg::gfx::Texture2D* specular_texture =
        load_texture(Mg::Identifier::from_runtime_string(specular_filename));

    m->set_sampler("sampler_diffuse", diffuse_texture->handle());
    m->set_sampler("sampler_normal", normal_texture->handle());
    m->set_sampler("sampler_specular", specular_texture->handle());

    return m;
}

Model Scene::load_model(Mg::Identifier mesh_file,
                        Mg::span<const MaterialAssignment> material_files,
                        Mg::span<const Mg::Identifier> options)
{
    Model model;
    model.mesh = load_mesh(mesh_file);

    for (auto&& [submesh_index, material_fname] : material_files) {
        model.material_bindings.push_back(
            Mg::gfx::MaterialBinding{ submesh_index, load_material(material_fname, options) });
    }

    return model;
}

void Scene::make_input_map()
{
    using namespace Mg::input;

    const auto& kb = app.window().keyboard;
    const auto& mouse = app.window().mouse;

    // clang-format off
    input_map.bind("forward",          kb.key(Keyboard::Key::W));
    input_map.bind("backward",         kb.key(Keyboard::Key::S));
    input_map.bind("left",             kb.key(Keyboard::Key::A));
    input_map.bind("right",            kb.key(Keyboard::Key::D));
    input_map.bind("up",               kb.key(Keyboard::Key::Space));
    input_map.bind("down",             kb.key(Keyboard::Key::LeftControl));
    input_map.bind("fullscreen",       kb.key(Keyboard::Key::F4));
    input_map.bind("exit",             kb.key(Keyboard::Key::Esc));
    input_map.bind("toggle_debug_vis", kb.key(Keyboard::Key::F));
    input_map.bind("toggle_glflush", mouse.button(Mouse::Button::right));

    input_map.bind("look_x", mouse.axis(Mouse::Axis::delta_x));
    input_map.bind("look_y", mouse.axis(Mouse::Axis::delta_y));
    // clang-format on
}

void Scene::make_blur_targets(Mg::VideoMode video_mode)
{
    MG_GFX_DEBUG_GROUP("Scene::make_blur_targets")
    static constexpr int32_t k_num_mip_levels = 4;

    blur_targets = {};

    Mg::gfx::RenderTargetParams params{};
    params.filter_mode = Mg::gfx::TextureFilterMode::Linear;
    params.width = video_mode.width >> 2;
    params.height = video_mode.height >> 2;
    params.num_mip_levels = k_num_mip_levels;
    params.texture_format = Mg::gfx::RenderTargetParams::Format::RGBA16F;

    params.render_target_id = "Blur_horizontal";
    blur_targets.hor_pass_target_texture = texture_repository.create_render_target(params);

    params.render_target_id = "Blur_vertical";
    blur_targets.vert_pass_target_texture = texture_repository.create_render_target(params);

    for (int32_t mip_level = 0; mip_level < k_num_mip_levels; ++mip_level) {
        blur_targets.hor_pass_targets.emplace_back(Mg::gfx::TextureRenderTarget::with_colour_target(
            blur_targets.hor_pass_target_texture,
            Mg::gfx::TextureRenderTarget::DepthType::None,
            mip_level));

        blur_targets.vert_pass_targets.emplace_back(
            Mg::gfx::TextureRenderTarget::with_colour_target(
                blur_targets.vert_pass_target_texture,
                Mg::gfx::TextureRenderTarget::DepthType::None,
                mip_level));
    }
}

void Scene::make_hdr_target(Mg::VideoMode mode)
{
    MG_GFX_DEBUG_GROUP("Scene::make_hdr_target")

    Mg::gfx::RenderTargetParams params{};
    params.filter_mode = Mg::gfx::TextureFilterMode::Linear;
    params.width = mode.width;
    params.height = mode.height;
    params.render_target_id = "HDR.colour";
    params.texture_format = Mg::gfx::RenderTargetParams::Format::RGBA16F;

    Mg::gfx::Texture2D* colour_target = texture_repository.create_render_target(params);

    params.render_target_id = "HDR.depth";
    params.texture_format = Mg::gfx::RenderTargetParams::Format::Depth24;

    Mg::gfx::Texture2D* depth_target = texture_repository.create_render_target(params);

    hdr_target = Mg::gfx::TextureRenderTarget::with_colour_and_depth_targets(colour_target,
                                                                             depth_target);
}

void Scene::render_bloom()
{
    MG_GFX_DEBUG_GROUP("render_bloom")

    constexpr size_t k_num_blur_iterations = 2;
    const size_t num_blur_targets = blur_targets.hor_pass_targets.size();

    Mg::gfx::TextureRenderTarget::BlitSettings blit_settings = {};
    blit_settings.colour = true;
    blit_settings.depth = false;
    blit_settings.stencil = false;
    blit_settings.filter = Mg::gfx::TextureRenderTarget::BlitFilter::linear;

    // Init first pass by blitting to ping-pong buffers.
    Mg::gfx::TextureRenderTarget::blit(*hdr_target,
                                       *blur_targets.vert_pass_targets[0],
                                       blit_settings);

    Mg::gfx::PostProcessRenderer::RenderGuard post_render_guard = post_renderer.get_guard();

    for (size_t mip_i = 0; mip_i < num_blur_targets; ++mip_i) {
        Mg::gfx::TextureRenderTarget& hor_target = *blur_targets.hor_pass_targets[mip_i];
        Mg::gfx::TextureRenderTarget& vert_target = *blur_targets.vert_pass_targets[mip_i];

        // Source mip-level will be [0, 0, 1, 2, 3, ...]
        blur_material->set_parameter("source_mip_level", Mg::max(0, static_cast<int>(mip_i) - 1));
        blur_material->set_parameter("destination_mip_level", static_cast<int>(mip_i));

        // Render gaussian blur in separate horizontal and vertical passes.
        for (size_t u = 0; u < k_num_blur_iterations; ++u) {
            // Horizontal pass
            blur_material->set_option("HORIZONTAL", true);
            hor_target.bind();
            post_renderer.post_process(post_render_guard,
                                       *blur_material,
                                       vert_target.colour_target()->handle());

            blur_material->set_parameter("source_mip_level", static_cast<int>(mip_i));

            // Vertical pass
            blur_material->set_option("HORIZONTAL", false);
            vert_target.bind();
            post_renderer.post_process(post_render_guard,
                                       *blur_material,
                                       hor_target.colour_target()->handle());
        }
    }
}

void Scene::render_light_debug_geometry()
{
    MG_GFX_DEBUG_GROUP("Scene::render_light_debug_geometry")

    for (const Mg::gfx::Light& light : scene_lights) {
        if (light.vector.w == 0.0) {
            continue;
        }
        Mg::gfx::DebugRenderer::EllipsoidDrawParams params;
        params.centre = glm::vec3(light.vector);
        params.colour = glm::vec4(normalize(light.colour), 0.15f);
        params.dimensions = glm::vec3(std::sqrt(light.range_sqr));
        params.wireframe = true;
        debug_renderer.draw_ellipsoid(camera, params);
    }
}

void Scene::load_models()
{
    using namespace Mg::literals;

    {
        std::array<MaterialAssignment, 4> scene_mats;
        scene_mats[0] = { 0, "buildings/GreenBrick" };
        scene_mats[1] = { 1, "buildings/W31_1" };
        scene_mats[2] = { 2, "buildings/BigWhiteBricks" };
        scene_mats[3] = { 3, "buildings/GreenBrick" };

        std::array<Mg::Identifier, 1> scene_mat_options;
        scene_mat_options[0] = "PARALLAX";

        scene_models.emplace_back(
            load_model("meshes/misc/test_scene_2.mgm", scene_mats, scene_mat_options));
    }

    {
        std::array<MaterialAssignment, 1> hest_mats;
        hest_mats[0] = { 0, "actors/HestDraugr" };

        std::array<Mg::Identifier, 1> hest_mat_options;
        hest_mat_options[0] = "RIM_LIGHT";

        Model& hest_model = scene_models.emplace_back(
            load_model("meshes/misc/hestdraugr.mgm", hest_mats, hest_mat_options));

        hest_model.transform.position.x += 2.0f;
    }

    {
        std::array<MaterialAssignment, 1> narmask_mats;
        narmask_mats[0] = { 0, "actors/narmask" };

        std::array<Mg::Identifier, 1> narmask_mat_options;
        narmask_mat_options[0] = "RIM_LIGHT";

        Model& narmask_model = scene_models.emplace_back(
            load_model("meshes/misc/narmask.mgm", narmask_mats, narmask_mat_options));

        narmask_model.transform.position.x -= 2.0f;
        narmask_model.transform.rotation.yaw(90_degrees);
    }
}

void Scene::load_materials()
{
    // Create post-process materials
    const auto bloom_handle =
        resource_cache.resource_handle<Mg::ShaderResource>("shaders/post_process_bloom.mgshader");
    auto const blur_handle =
        resource_cache.resource_handle<Mg::ShaderResource>("shaders/post_process_blur.mgshader");

    bloom_material = material_repository.create("bloom_material", bloom_handle);
    blur_material = material_repository.create("blur_material", blur_handle);

    // Create billboard material
    const auto billboard_handle =
        resource_cache.resource_handle<Mg::ShaderResource>("shaders/simple_billboard.mgshader");

    billboard_material = material_repository.create("billboard_material", billboard_handle);
    billboard_material->set_sampler("sampler_diffuse",
                                    load_texture("textures/light_t.dds")->handle());

    // Create UI material
    const auto ui_handle =
        resource_cache.resource_handle<Mg::ShaderResource>("shaders/ui_render_test.mgshader");
    ui_material = material_repository.create("ui_material", ui_handle);
    ui_material->set_sampler("sampler_colour",
                             load_texture("textures/ui/book_open_da.dds")->handle());
}

// Create a lot of random lights
void Scene::generate_lights()
{
    std::srand(0xdeadbeef);

    for (size_t i = 0; i < k_num_lights; ++i) {
        auto pos = glm::vec3(rand(), rand(), 0.0f);
        pos /= float(RAND_MAX);
        pos -= glm::vec3(0.5f, 0.5f, 0.0f);
        pos *= 15.0f;
        pos.z += 1.125f;

        glm::vec4 light_colour(rand(), rand(), rand(), float(RAND_MAX));
        light_colour /= float(RAND_MAX);

        float s = float(std::sin(double(i) * 0.2));

        pos.z += s;

        // Draw a billboard sprite for each light
        {
            Mg::gfx::Billboard& billboard = billboard_render_list.add();
            billboard.pos = pos;
            billboard.colour = light_colour * 10.0f;
            billboard.colour.a = 1.0f;
            billboard.radius = 0.05f;
        }

        scene_lights.push_back(
            Mg::gfx::make_point_light(pos, light_colour * 100.0f, k_light_radius));
    }
}

void Scene::on_window_focus_change(const bool is_focused)
{
    if (is_focused) {
        resource_cache.refresh();
    }
}

void Scene::on_resource_reload(const Mg::FileChangedEvent& event)
{
    MG_GFX_DEBUG_GROUP("Scene::on_resource_reload")

    using namespace Mg::literals;

    const auto resource_name = event.resource.resource_id().str_view();
    Mg::g_log.write_message(fmt::format("File '{}' changed and was reloaded.", resource_name));
    switch (event.resource_type.hash()) {
    case "TextureResource"_hash: {
        Mg::ResourceAccessGuard<Mg::TextureResource> access(event.resource);
        texture_repository.update(*access);
        break;
    }
    case "MeshResource"_hash: {
        Mg::ResourceAccessGuard<Mg::MeshResource> access(event.resource);
        mesh_repository.update(*access);
        break;
    }
    case "ShaderResource"_hash:
        mesh_renderer.drop_shaders();
        billboard_renderer.drop_shaders();
        post_renderer.drop_shaders();
        ui_renderer.drop_shaders();
        break;
    }
}

int main(int /*argc*/, char* /*argv*/[])
{
    try {
        Scene scene;
        scene.init();
        scene.main_loop();
    }
    catch (const std::exception& e) {
        Mg::g_log.write_error(e.what());
        return 1;
    }
}
