#include "test_scene.h"
#include "mg/gfx/mg_render_command_list.h"

#include <mg/core/mg_config.h>
#include <mg/core/mg_identifier.h>
#include <mg/core/mg_log.h>
#include <mg/gfx/mg_blend_modes.h>
#include <mg/gfx/mg_font_handler.h>
#include <mg/gfx/mg_gfx_debug_group.h>
#include <mg/gfx/mg_mesh_renderer.h>
#include <mg/gfx/mg_render_target.h>
#include <mg/gfx/mg_skeleton.h>
#include <mg/gfx/mg_texture2d.h>
#include <mg/gfx/mg_texture_pool.h>
#include <mg/gfx/mg_ui_renderer.h>
#include <mg/mg_unicode.h>
#include <mg/resource_cache/internal/mg_resource_entry_base.h>
#include <mg/resource_cache/mg_resource_access_guard.h>
#include <mg/resources/mg_font_resource.h>
#include <mg/resources/mg_mesh_resource.h>
#include <mg/resources/mg_shader_resource.h>
#include <mg/resources/mg_sound_resource.h>
#include <mg/resources/mg_texture_resource.h>
#include <mg/utils/mg_angle.h>
#include <mg/utils/mg_math_utils.h>
#include <mg/utils/mg_string_utils.h>

#include <fmt/core.h>

#include <numeric>

namespace {

using namespace Mg::literals;

constexpr double k_time_step = 1.0 / 60.0;
constexpr double k_accumulator_max_steps = 10;
constexpr size_t k_num_lights = 128;
constexpr float k_light_radius = 3.0f;

constexpr auto camera_max_vel = 0.2f;
constexpr auto camera_acc = 0.01f;
constexpr auto camera_friction = 0.005f;

std::array<float, 3> camera_acceleration(Mg::input::InputMap& input_map)
{
    auto is_held_value = [&input_map](Mg::Identifier mapping) -> float {
        return static_cast<float>(input_map.is_held(mapping));
    };
    auto forward_acc = camera_acc * (is_held_value("forward") - is_held_value("backward"));
    auto right_acc = camera_acc * (is_held_value("right") - is_held_value("left"));
    auto up_acc = camera_acc * (is_held_value("up") - is_held_value("down"));

    return { forward_acc, right_acc, up_acc };
}

void add_to_render_list(const Model& model, Mg::gfx::RenderCommandProducer& renderlist)
{
    if (model.skeleton && model.pose) {
        const auto num_joints = Mg::narrow<uint16_t>(model.skeleton->joints().size());
        auto palette = renderlist.allocate_skinning_matrix_palette(num_joints);
        Mg::gfx::calculate_skinning_matrices(model.transform,
                                             *model.skeleton,
                                             *model.pose,
                                             palette.skinning_matrices());
        renderlist.add_skinned_mesh(model.mesh, model.transform, model.material_bindings, palette);
    }
    else {
        renderlist.add_mesh(model.mesh, model.transform, model.material_bindings);
    }
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

    const auto font_resource =
        resource_cache.resource_handle<Mg::FontResource>("fonts/LiberationSerif-Regular.ttf");
    std::vector<Mg::UnicodeRange> unicode_ranges = { Mg::get_unicode_range(
        Mg::UnicodeBlock::Basic_Latin) };
    font = std::make_unique<Mg::gfx::BitmapFont>(font_resource, 24, unicode_ranges);

    main_loop();
}

static double frame_rate = 0.0f;

void Scene::main_loop()
{
    double accumulator = 0.0;

    std::array<double, 30> frame_time_samples = {};
    size_t frame_time_sample_index = 0;

    while (!exit) {
        const double last_time = time;
        time = app.time_since_init();

        ++frame_time_sample_index;
        frame_time_sample_index %= frame_time_samples.size();

        frame_rate = 1.0 /
                     (std::accumulate(frame_time_samples.begin(), frame_time_samples.end(), 0.0) /
                      frame_time_samples.size());

        const double time_delta = time - last_time;
        frame_time_samples[frame_time_sample_index] = time_delta;

        accumulator += time_delta;
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
            texture_pool.destroy(hdr_target->colour_target());
            texture_pool.destroy(hdr_target->depth_target());
            texture_pool.destroy(blur_targets.hor_pass_target_texture);
            texture_pool.destroy(blur_targets.vert_pass_target_texture);
        }

        make_hdr_target(app.window().settings().video_mode);
        make_blur_targets(app.window().settings().video_mode);

        app.window().release_cursor();
    }

    if (input_map.was_pressed("toggle_debug_vis")) {
        draw_debug = !draw_debug;
    }
}

void Scene::render_scene(const double lerp_factor)
{
    MG_GFX_DEBUG_GROUP("Scene::render_scene")

    Scene::State render_state = lerp(prev_state, current_state, lerp_factor);
    camera.position = render_state.cam_position;
    camera.rotation = render_state.cam_rotation;

    scene_lights.back() = Mg::gfx::make_point_light(current_state.cam_position,
                                                    glm::vec3(25.0f),
                                                    0.25f * k_light_radius);

    // Draw meshes and billboards
    {
        render_command_producer.clear();

        for (auto&& [model_id, model] : scene_models) {
            add_to_render_list(model, render_command_producer);
        }

        hdr_target->bind();
        app.gfx_device().clear();

        const auto& commands = render_command_producer.finalise(camera,
                                                                Mg::gfx::SortingMode::near_to_far);

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

        post_renderer.post_process(post_renderer.make_context(),
                                   *bloom_material,
                                   hdr_target->colour_target()->handle());
    }

    // Draw UI
    ui_renderer.resolution(
        { app.window().frame_buffer_size().width, app.window().frame_buffer_size().height });

    /*{
        Mg::gfx::UIPlacement placement = {};
        placement.position = Mg::gfx::UIPlacement::centre;
        placement.anchor = Mg::gfx::UIPlacement::centre;

        ui_material->set_parameter("opacity", 0.5f);
        ui_renderer.draw_rectangle(placement,
                                   { 100.0f, 100.0f },
                                   *ui_material,
                                   Mg::gfx::blend_mode_constants::bm_add);
    }*/
    {
        Mg::gfx::UIPlacement placement = {};
        placement.position = Mg::gfx::UIPlacement::top_left;
        placement.anchor = Mg::gfx::UIPlacement::top_left;

        Mg::gfx::TypeSetting typesetting = {};
        typesetting.line_spacing_factor = 1.25f;
        typesetting.max_width_pixels = ui_renderer.resolution().x;

        std::string text = fmt::format("FPS: {:.2f}", frame_rate);
        ui_renderer.draw_text(placement, font->prepare_text(text, typesetting));
    }

    Model& fox = scene_models["meshes/Fox.mgm"];
    const Mg::gfx::Mesh::JointId head_joint_id = fox.skeleton->find_joint("b_Head_05").value();
    const Mg::gfx::Mesh::JointId neck_joint_id = fox.skeleton->find_joint("b_Neck_04").value();
    const Mg::gfx::Mesh::JointId tail_2_joint_id = fox.skeleton->find_joint("b_Tail02_013").value();
    const Mg::Angle nod_angle = 25.0_degrees * std::sin(float(5.0 * time));
    const Mg::Angle shake_angle = 15.0_degrees * std::sin(float(10.0 * time));
    auto& head_pose = fox.pose.value().joint_poses[head_joint_id];
    auto& neck_pose = fox.pose.value().joint_poses[neck_joint_id];
    auto& tail_2_pose = fox.pose.value().joint_poses[tail_2_joint_id];
    head_pose.rotation.roll(-15_degrees + shake_angle - head_pose.rotation.roll());
    neck_pose.rotation.yaw(nod_angle - neck_pose.rotation.yaw());
    tail_2_pose.rotation.roll(30_degrees + nod_angle * 0.5f - tail_2_pose.rotation.roll());

    // Debug geometry
    if (draw_debug) {
        // render_light_debug_geometry();
        render_skeleton_debug_geometry();
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
    const Mg::Opt<Mg::gfx::MeshHandle> mesh_handle = mesh_pool.get(file);
    if (mesh_handle.has_value()) {
        return mesh_handle.value();
    }

    const Mg::ResourceAccessGuard access = resource_cache.access_resource<Mg::MeshResource>(file);
    return mesh_pool.create(*access);
}

Mg::Opt<Mg::gfx::Skeleton> Scene::load_skeleton(Mg::Identifier file)
{
    const Mg::ResourceAccessGuard access = resource_cache.access_resource<Mg::MeshResource>(file);
    if (access->joints().empty()) {
        return Mg::nullopt;
    }

    Mg::gfx::Skeleton skeleton(file, access->joints().size());
    for (size_t i = 0; i < skeleton.joints().size(); ++i) {
        skeleton.joints()[i] = access->joints()[i];
    }
    return skeleton;
}

Mg::gfx::Texture2D* Scene::load_texture(Mg::Identifier file)
{
    // Get form pool if it exists there.
    Mg::gfx::Texture2D* texture = texture_pool.get(file);
    if (texture) {
        return texture;
    }

    // Otherwise, load from file.
    if (resource_cache.file_exists(file)) {
        const Mg::ResourceAccessGuard access =
            resource_cache.access_resource<Mg::TextureResource>(file);
        return texture_pool.create(*access);
    }

    return nullptr;
}

Mg::gfx::Material* Scene::load_material(Mg::Identifier file, Mg::span<const Mg::Identifier> options)
{
    auto handle = resource_cache.resource_handle<Mg::ShaderResource>("shaders/default.mgshader");
    Mg::gfx::Material* m = material_pool.create(file, handle);

    for (auto o : options) {
        m->set_option(o, true);
    }

    const std::string diffuse_filename = fmt::format("textures/{}_da.dds", file.str_view());
    const std::string diffuse_filename_alt = fmt::format("textures/{}_d.dds", file.str_view());
    const std::string normal_filename = fmt::format("textures/{}_n.dds", file.str_view());
    const std::string specular_filename = fmt::format("textures/{}_s.dds", file.str_view());

    Mg::gfx::Texture2D* diffuse_texture =
        load_texture(Mg::Identifier::from_runtime_string(diffuse_filename));
    if (!diffuse_texture) {
        diffuse_texture = load_texture(Mg::Identifier::from_runtime_string(diffuse_filename_alt));
    }
    if (!diffuse_texture) {
        diffuse_texture =
            texture_pool.get_default_texture(Mg::gfx::TexturePool::DefaultTexture::Checkerboard);
    }

    Mg::gfx::Texture2D* normal_texture =
        load_texture(Mg::Identifier::from_runtime_string(normal_filename));
    if (!normal_texture) {
        normal_texture =
            texture_pool.get_default_texture(Mg::gfx::TexturePool::DefaultTexture::NormalsFlat);
    }

    Mg::gfx::Texture2D* specular_texture =
        load_texture(Mg::Identifier::from_runtime_string(specular_filename));
    if (!specular_texture) {
        specular_texture =
            texture_pool.get_default_texture(Mg::gfx::TexturePool::DefaultTexture::Transparent);
    }

    m->set_sampler("sampler_diffuse", diffuse_texture->handle());
    m->set_sampler("sampler_normal", normal_texture->handle());
    m->set_sampler("sampler_specular", specular_texture->handle());

    return m;
}

Model& Scene::load_model(Mg::Identifier mesh_file,
                         Mg::span<const MaterialAssignment> material_files,
                         Mg::span<const Mg::Identifier> options)
{
    const auto [it, inserted] = scene_models.insert({ mesh_file, Model{} });
    Model& model = it->second;
    model.mesh = load_mesh(mesh_file);

    for (auto&& [submesh_index, material_fname] : material_files) {
        model.material_bindings.push_back(
            Mg::gfx::MaterialBinding{ submesh_index, load_material(material_fname, options) });
    }

    model.skeleton = load_skeleton(mesh_file);
    if (model.skeleton) {
        model.pose = model.skeleton->get_bind_pose();
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
    blur_targets.hor_pass_target_texture = texture_pool.create_render_target(params);

    params.render_target_id = "Blur_vertical";
    blur_targets.vert_pass_target_texture = texture_pool.create_render_target(params);

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

    Mg::gfx::Texture2D* colour_target = texture_pool.create_render_target(params);

    params.render_target_id = "HDR.depth";
    params.texture_format = Mg::gfx::RenderTargetParams::Format::Depth24;

    Mg::gfx::Texture2D* depth_target = texture_pool.create_render_target(params);

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

    Mg::gfx::PostProcessRenderer::Context post_render_context = post_renderer.make_context();

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
            post_renderer.post_process(post_render_context,
                                       *blur_material,
                                       vert_target.colour_target()->handle());

            blur_material->set_parameter("source_mip_level", static_cast<int>(mip_i));

            // Vertical pass
            blur_material->set_option("HORIZONTAL", false);
            vert_target.bind();
            post_renderer.post_process(post_render_context,
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
        params.colour = glm::vec4(normalize(light.colour), 0.05f);
        params.dimensions = glm::vec3(std::sqrt(light.range_sqr));
        params.wireframe = true;
        debug_renderer.draw_ellipsoid(camera, params);
    }
}

void Scene::render_skeleton_debug_geometry()
{
    MG_GFX_DEBUG_GROUP("Scene::render_skeleton_debug_geometry")

    for (const auto& [model_id, model] : scene_models) {
        if (model.skeleton.has_value() && model.pose.has_value()) {
            debug_renderer.draw_bones(camera,
                                      model.transform.matrix(),
                                      *model.skeleton,
                                      *model.pose);
        }
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
        load_model("meshes/misc/test_scene_2.mgm", scene_mats, scene_mat_options);
    }

    {
        std::array<MaterialAssignment, 1> fox_mats;
        fox_mats[0] = { 0, "actors/fox" };

        std::array<Mg::Identifier, 1> fox_mat_options;
        fox_mat_options[0] = "RIM_LIGHT";

        Model& fox_model = load_model("meshes/Fox.mgm", fox_mats, fox_mat_options);

        fox_model.transform.position.x += 2.0f;
    }
}

void Scene::load_materials()
{
    // Create post-process materials
    const auto bloom_handle =
        resource_cache.resource_handle<Mg::ShaderResource>("shaders/post_process_bloom.mgshader");
    auto const blur_handle =
        resource_cache.resource_handle<Mg::ShaderResource>("shaders/post_process_blur.mgshader");

    bloom_material = material_pool.create("bloom_material", bloom_handle);
    blur_material = material_pool.create("blur_material", blur_handle);

    // Create billboard material
    const auto billboard_handle =
        resource_cache.resource_handle<Mg::ShaderResource>("shaders/simple_billboard.mgshader");

    billboard_material = material_pool.create("billboard_material", billboard_handle);
    billboard_material->set_sampler("sampler_diffuse",
                                    load_texture("textures/light_t.dds")->handle());

    // Create UI material
    const auto ui_handle =
        resource_cache.resource_handle<Mg::ShaderResource>("shaders/ui_render_test.mgshader");
    ui_material = material_pool.create("ui_material", ui_handle);
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

        const auto s = Mg::narrow_cast<float>(std::sin(double(i) * 0.2));

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
            Mg::gfx::make_point_light(pos, light_colour * 10.0f, k_light_radius));
    }

    scene_lights.emplace_back(); // temp dummy
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
    Mg::log.message(fmt::format("File '{}' changed and was reloaded.", resource_name));
    switch (event.resource_type.hash()) {
    case "TextureResource"_hash: {
        Mg::ResourceAccessGuard<Mg::TextureResource> access(event.resource);
        texture_pool.update(*access);
        break;
    }
    case "MeshResource"_hash: {
        Mg::ResourceAccessGuard<Mg::MeshResource> access(event.resource);
        mesh_pool.update(*access);
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
        Mg::log.error(e.what());
        return 1;
    }
}
