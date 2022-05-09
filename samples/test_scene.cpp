#include "test_scene.h"
#include "mg/gfx/mg_texture_related_types.h"
#include "mg/resources/mg_material_resource.h"

#include <mg/core/mg_application_context.h>
#include <mg/core/mg_config.h>
#include <mg/core/mg_identifier.h>
#include <mg/core/mg_log.h>
#include <mg/core/mg_runtime_error.h>
#include <mg/gfx/mg_animation.h>
#include <mg/gfx/mg_blend_modes.h>
#include <mg/gfx/mg_debug_renderer.h>
#include <mg/gfx/mg_gfx_debug_group.h>
#include <mg/gfx/mg_mesh_renderer.h>
#include <mg/gfx/mg_render_command_list.h>
#include <mg/gfx/mg_render_target.h>
#include <mg/gfx/mg_skeleton.h>
#include <mg/gfx/mg_texture2d.h>
#include <mg/gfx/mg_texture_pool.h>
#include <mg/gfx/mg_ui_renderer.h>
#include <mg/mg_bounding_volumes.h>
#include <mg/mg_unicode.h>
#include <mg/physics/mg_physics.h>
#include <mg/resource_cache/internal/mg_resource_entry_base.h>
#include <mg/resource_cache/mg_resource_access_guard.h>
#include <mg/resources/mg_font_resource.h>
#include <mg/resources/mg_mesh_resource.h>
#include <mg/resources/mg_shader_resource.h>
#include <mg/resources/mg_sound_resource.h>
#include <mg/resources/mg_texture_resource.h>
#include <mg/utils/mg_angle.h>
#include <mg/utils/mg_math_utils.h>
#include <mg/utils/mg_rand.h>
#include <mg/utils/mg_string_utils.h>

#include <fmt/core.h>

#include <numeric>

namespace {

using namespace Mg::literals;

constexpr auto config_file = "mg_engine.cfg";

constexpr int k_steps_per_second = 60.0;
constexpr size_t k_num_lights = 128;
constexpr float k_light_radius = 3.0f;
constexpr float actor_acceleration = 0.6f;

std::array<float, 3> get_actor_acceleration(Mg::input::InputMap& input_map)
{
    auto is_held_value = [&input_map](Mg::Identifier mapping) -> float {
        return static_cast<float>(input_map.is_held(mapping));
    };
    auto forward_acc = actor_acceleration * (is_held_value("forward") - is_held_value("backward"));
    auto right_acc = actor_acceleration * (is_held_value("right") - is_held_value("left"));
    auto up_acc = 0.0f; // actor_acceleration * (is_held_value("jump") - is_held_value("crouch"));

    return { forward_acc, right_acc, up_acc };
}

void add_to_render_list(const Model& model, Mg::gfx::RenderCommandProducer& renderlist)
{
    const glm::mat4 transform = model.transform * model.vis_transform;

    if (model.skeleton && model.pose) {
        const auto num_joints = Mg::narrow<uint16_t>(model.skeleton->joints().size());
        auto palette = renderlist.allocate_skinning_matrix_palette(num_joints);
        Mg::gfx::calculate_skinning_matrices(transform,
                                             *model.skeleton,
                                             *model.pose,
                                             palette.skinning_matrices());
        renderlist.add_skinned_mesh(model.mesh, transform, model.material_assignments, palette);
    }
    else {
        renderlist.add_mesh(model.mesh, transform, model.material_assignments);
    }
}

} // namespace

void Actor::update(glm::vec3 acceleration, float jump_impulse)
{
    // Walk slower when crouching.
    acceleration *= character_controller.get_is_standing() ? 1.0f : 0.5f;

    const auto max_speed = max_horizontal_speed * (character_controller.get_is_standing() ||
                                                           !character_controller.is_on_ground()
                                                       ? 1.0f
                                                       : 0.5f);

    const float current_friction = character_controller.is_on_ground() ? friction : 0.0f;

    // Keep old velocity for inertia, but discard velocity added by platform movement, as that makes
    // the actor far too prone to uncontrollably sliding off surfaces.
    auto horizontal_velocity = character_controller.velocity() -
                               character_controller.velocity_added_by_moving_surface();
    horizontal_velocity.x += acceleration.x;
    horizontal_velocity.y += acceleration.y;
    horizontal_velocity.z = 0.0f;

    // Compensate for friction.
    if (glm::length2(acceleration) > 0.0f) {
        horizontal_velocity += glm::normalize(acceleration) * current_friction;
    }

    const float horizontal_speed = glm::length(horizontal_velocity);
    if (horizontal_speed > max_speed) {
        horizontal_velocity.x *= max_speed / horizontal_speed;
        horizontal_velocity.y *= max_speed / horizontal_speed;
    }

    if (glm::length2(horizontal_velocity) >= 0.0f) {
        if (glm::length2(horizontal_velocity) <= current_friction * current_friction) {
            horizontal_velocity = glm::vec3(0.0f);
        }
        else {
            horizontal_velocity -= glm::normalize(horizontal_velocity) * current_friction;
        }
    }

    character_controller.move(horizontal_velocity);
    character_controller.jump(jump_impulse *
                              (character_controller.get_is_standing() ? 1.0f : 0.5f));
}

Scene::Scene() : app(config_file) {}

Scene::~Scene()
{
    Mg::write_display_settings(app.config(), app.window().settings());
    app.config().write_to_file(config_file);
}

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
        app.window().release_cursor(); // Don't lock instantly.
    }

    resource_cache->set_resource_reload_callback(
        [](void* scene, const Mg::FileChangedEvent& event) {
            static_cast<Scene*>(scene)->on_resource_reload(event);
        },
        this);

    make_hdr_target(app.window().settings().video_mode);
    make_blur_targets(app.window().settings().video_mode);

    app.gfx_device().set_clear_colour(0.0125f, 0.01275f, 0.025f, 1.0f);

    camera.set_aspect_ratio(app.window().aspect_ratio());
    camera.field_of_view = 80_degrees;

    make_input_map();

    physics_world = std::make_unique<Mg::physics::World>();
    actor = std::make_unique<Actor>(*physics_world, glm::vec3(0.0f, 0.0f, 5.0f), 70.0f);

    load_models();
    load_materials();
    generate_lights();

    const auto font_resource =
        resource_cache->resource_handle<Mg::FontResource>("fonts/LiberationMono-Regular.ttf");
    std::vector<Mg::UnicodeRange> unicode_ranges = { Mg::get_unicode_range(
        Mg::UnicodeBlock::Basic_Latin) };
    font = std::make_unique<Mg::gfx::BitmapFont>(font_resource, 24, unicode_ranges);

    // imgui = std::make_unique<Mg::ImguiOverlay>(app.window());
}

void Scene::simulation_step()
{
    using namespace Mg::literals;

    Mg::gfx::get_debug_render_queue().clear();

    app.window().poll_input_events();
    input_map.update();

    if (input_map.was_pressed("exit") || app.window().should_close_flag()) {
        m_should_exit = true;
    }

    // Actor movement
    const bool is_jumping = input_map.was_pressed("jump") &&
                            actor->character_controller.is_on_ground();

    const auto [forward_acc, right_acc, up_acc] = get_actor_acceleration(input_map);
    const Mg::Rotation rotation_horizontal(glm::vec3(0.0f, 0.0f, camera.rotation.euler_angles().z));
    const glm::vec3 vec_forward = rotation_horizontal.forward();
    const glm::vec3 vec_right = rotation_horizontal.right();

    const float factor = actor->character_controller.is_on_ground() ? 1.0f : 0.3f;
    actor->update((vec_forward * forward_acc + vec_right * right_acc) * factor,
                  (is_jumping ? 5.0f : 0.0f));

    // Fullscreen switching
    if (input_map.was_pressed("fullscreen")) {
        Mg::WindowSettings s = app.window().settings();
        s.fullscreen = !s.fullscreen;
        s.video_mode = {}; // reset resolution etc. to defaults
        app.window().apply_settings(s);
        camera.set_aspect_ratio(app.window().aspect_ratio());

        // Dispose of old render target textures. TODO: RAII
        {
            texture_pool->destroy(hdr_target->colour_target());
            texture_pool->destroy(hdr_target->depth_target());
            texture_pool->destroy(blur_targets.hor_pass_target_texture);
            texture_pool->destroy(blur_targets.vert_pass_target_texture);
        }

        make_hdr_target(app.window().settings().video_mode);
        make_blur_targets(app.window().settings().video_mode);

        app.window().release_cursor();
    }

    if (input_map.was_pressed("toggle_debug_vis")) {
        draw_debug = !draw_debug;
    }
    if (input_map.was_pressed("lock_camera")) {
        camera_locked = !camera_locked;
    }
    if (input_map.was_pressed("reset")) {
        actor->character_controller.set_position({ 0.0f, 0.0f, 2.0f });
        actor->character_controller.reset();
    }
    if (input_map.is_held("crouch")) {
        actor->character_controller.set_is_standing(false);
    }
    else {
        actor->character_controller.set_is_standing(true);
    }

    physics_world->update(1.0f / k_steps_per_second);
    actor->character_controller.update(1.0f / k_steps_per_second);

    // Vertical interpolation for camera to avoid sharp movements when e.g. stepping up stairs.
    last_camera_z = camera_z;
    const auto new_camera_z = actor->position(1.0f).z + actor->current_height() - 0.1f;
    camera_z = std::abs(last_camera_z - new_camera_z) < 1.0f
                   ? glm::mix(last_camera_z, new_camera_z, 0.35f)
                   : new_camera_z;
}

void Scene::render(const double lerp_factor)
{
    MG_GFX_DEBUG_GROUP("Scene::render_scene")

    physics_world->interpolate(static_cast<float>(lerp_factor));

    // Mouselook
    {
        app.window().poll_input_events();
        input_map.refresh();

        const auto cursor_position = app.window().mouse.get_cursor_position();
        float mouse_delta_x = cursor_position.x - last_cursor_position.x;
        float mouse_delta_y = cursor_position.y - last_cursor_position.y;
        mouse_delta_x *= app.config().as<float>("mouse_sensitivity_x");
        mouse_delta_y *= app.config().as<float>("mouse_sensitivity_y");
        if (!app.window().is_cursor_locked_to_window()) {
            mouse_delta_x = 0.0f;
            mouse_delta_y = 0.0f;
        }

        Mg::Angle cam_pitch = camera.rotation.pitch() - Mg::Angle::from_radians(mouse_delta_y);
        Mg::Angle cam_yaw = camera.rotation.yaw() - Mg::Angle::from_radians(mouse_delta_x);

        cam_pitch = Mg::Angle::clamp(cam_pitch, -90_degrees, 90_degrees);

        // Set rotation from euler angles, clamping pitch between straight down & straight up.
        camera.rotation = Mg::Rotation{ { cam_pitch.radians(), 0.0f, cam_yaw.radians() } };

        last_cursor_position = cursor_position;
    }

    if (!camera_locked) {
        camera.position = actor->position(float(lerp_factor));
        camera.position.z = glm::mix(last_camera_z, camera_z, lerp_factor);
    }

    for (auto& [id, model] : dynamic_models) {
        model.update();
    }

    scene_lights.back() =
        Mg::gfx::make_point_light(camera.position, glm::vec3(25.0f), 2.0f * k_light_radius);

    // Draw meshes and billboards
    {
        render_command_producer.clear();

        for (auto&& [model_id, model] : scene_models) {
            add_to_render_list(model, render_command_producer);
        }

        for (auto&& [model_id, model] : dynamic_models) {
            add_to_render_list(model, render_command_producer);
        }

        app.gfx_device().clear(*hdr_target);

        const auto& commands = render_command_producer.finalize(camera,
                                                                Mg::gfx::SortingMode::near_to_far);

        Mg::gfx::RenderParameters params = {};
        params.current_time = Mg::narrow_cast<float>(app.time_since_init());
        params.camera_exposure = -5.0;

        mesh_renderer.render(camera, commands, scene_lights, *hdr_target, params);

        billboard_renderer.render(*hdr_target, camera, billboard_render_list, *billboard_material);
    }

    render_bloom();

    // Apply tonemap and render to window render target.
    {
        app.gfx_device().clear(app.window().render_target);

        bloom_material->set_sampler("sampler_bloom",
                                    blur_targets.vert_pass_target_texture->handle());

        post_renderer.post_process(post_renderer.make_context(),
                                   *bloom_material,
                                   app.window().render_target,
                                   hdr_target->colour_target()->handle());
    }

    // Draw UI
    ui_renderer.resolution(
        { app.window().frame_buffer_size().width, app.window().frame_buffer_size().height });

#if 0
    {
        Mg::gfx::UIPlacement placement = {};
        placement.position = Mg::gfx::UIPlacement::centre;
        placement.anchor = Mg::gfx::UIPlacement::centre;

        ui_material->set_parameter("opacity", 0.5f);
        ui_renderer.draw_rectangle(placement,
                                   { 100.0f, 100.0f },
                                   *ui_material,
                                   Mg::gfx::blend_mode_constants::bm_add);
    }
#endif
    {
        Mg::gfx::UIPlacement placement = {};
        placement.position = Mg::gfx::UIPlacement::top_left;
        placement.anchor = Mg::gfx::UIPlacement::top_left;

        Mg::gfx::TypeSetting typesetting = {};
        typesetting.line_spacing_factor = 1.25f;
        typesetting.max_width_pixels = ui_renderer.resolution().x;

        const glm::vec3 v = actor->character_controller.velocity();
        const glm::vec3 p = actor->position();

        std::string text = fmt::format("FPS: {:.2f}", app.performance_info().frames_per_second);
        text += fmt::format("\nLast frame time: {:.2f} ms",
                            app.performance_info().last_frame_time_seconds * 1'000);
        text += fmt::format("\nVelocity: {{{:.2f}, {:.2f}, {:.2f}}}", v.x, v.y, v.z);
        text += fmt::format("\nPosition: {{{:.2f}, {:.2f}, {:.2f}}}", p.x, p.y, p.z);
        text += fmt::format("\nGrounded: {:b}", actor->character_controller.is_on_ground());

        ui_renderer.draw_text(app.window().render_target,
                              placement,
                              font->prepare_text(text, typesetting));

#if 0
        for (const auto& collision : physics_world->get_collisions()) {
            Mg::gfx::DebugRenderer::EllipsoidDrawParams params;
            params.dimensions = glm::vec3(0.05f);
            params.centre = glm::vec3(collision.contact_point_on_a);
            params.colour = glm::vec4(1.0f, 0.0f, 1.0f, 0.5f);
            debug_renderer.draw_ellipsoid(camera.view_proj_matrix(), params);
            params.centre = glm::vec3(collision.contact_point_on_b);
            params.colour = glm::vec4(0.0f, 1.0f, 0.0f, 0.5f);
            debug_renderer.draw_ellipsoid(camera.view_proj_matrix(), params);
        }
#endif
    }

    Model& fox = dynamic_models["meshes/Fox.mgm"];
    {
        Mg::gfx::animate_skeleton(fox.clips[1], fox.pose.value(), app.time_since_init());
    }

    // Debug geometry
    if (draw_debug) {
        // render_light_debug_geometry();
        // render_skeleton_debug_geometry();
        physics_world->draw_debug(app.window().render_target,
                                  debug_renderer,
                                  camera.view_proj_matrix());
        Mg::gfx::get_debug_render_queue().dispatch(app.window().render_target,
                                                   debug_renderer,
                                                   camera.view_proj_matrix());
    }

#if 0 // Raycast from camera test.
    std::vector<Mg::physics::RayHit> results;
    physics_world->raycast(camera.position,
                           camera.position + camera.rotation.forward() * 1000.0f,
                           ~Mg::physics::CollisionGroup::Character,
                           results);
    for (auto& rayhit : results) {
        Mg::gfx::DebugRenderer::EllipsoidDrawParams params;
        params.dimensions = glm::vec3(0.05f);
        params.centre = glm::vec3(rayhit.hit_point_worldspace);
        params.colour = glm::vec4(1.0f, 0.0f, 1.0f, 0.5f);
        debug_renderer.draw_ellipsoid(camera.view_proj_matrix(), params);
        debug_renderer.draw_line(camera.view_proj_matrix(),
                                 rayhit.hit_point_worldspace,
                                 rayhit.hit_point_worldspace + rayhit.hit_normal_worldspace * 0.2f,
                                 { 0.0f, 0.0f, 1.0f, 1.0f });
    }
#endif

    app.window().refresh();
}

Mg::UpdateTimerSettings Scene::update_timer_settings() const
{
    Mg::UpdateTimerSettings settings;
    settings.max_frames_per_second = 120;
    settings.max_time_steps_at_once = 10;
    settings.decouple_rendering_from_time_step = true;
    settings.simulation_steps_per_second = k_steps_per_second;
    return settings;
}

void Scene::setup_config()
{
    auto& cfg = app.config();
    cfg.set_default_value("mouse_sensitivity_x", 0.002f);
    cfg.set_default_value("mouse_sensitivity_y", 0.002f);
}

Mg::gfx::Texture2D* Scene::load_texture(Mg::Identifier file, const bool sRGB)
{
    // Get from pool if it exists there.
    Mg::gfx::Texture2D* texture = texture_pool->get(file);
    if (texture) {
        return texture;
    }

    Mg::gfx::TextureSettings settings = {};
    settings.sRGB = sRGB ? Mg::gfx::SRGBSetting::sRGB : Mg::gfx::SRGBSetting::Linear;

    // Otherwise, load from file.
    if (resource_cache->file_exists(file)) {
        const Mg::ResourceAccessGuard access =
            resource_cache->access_resource<Mg::TextureResource>(file);
        return texture_pool->from_resource(*access, settings);
    }

    return nullptr;
}

Mg::gfx::Material* Scene::load_material(Mg::Identifier file, Mg::span<const Mg::Identifier> options)
{
#if 1
    const std::string diffuse_filename = fmt::format("textures/{}_da.dds", file.str_view());
    const std::string diffuse_filename_alt = fmt::format("textures/{}_d.dds", file.str_view());
    const std::string normal_filename = fmt::format("textures/{}_n.dds", file.str_view());
    const std::string specular_filename = fmt::format("textures/{}_s.dds", file.str_view());
    const std::string ao_roughness_metallic_filename = fmt::format("textures/{}_arm.dds",
                                                                   file.str_view());

    Mg::gfx::Texture2D* diffuse_texture =
        load_texture(Mg::Identifier::from_runtime_string(diffuse_filename), true);
    if (!diffuse_texture) {
        diffuse_texture = load_texture(Mg::Identifier::from_runtime_string(diffuse_filename_alt),
                                       true);
    }
    if (!diffuse_texture) {
        diffuse_texture =
            texture_pool->get_default_texture(Mg::gfx::TexturePool::DefaultTexture::Checkerboard);
    }

    Mg::gfx::Texture2D* normal_texture =
        load_texture(Mg::Identifier::from_runtime_string(normal_filename), false);
    if (!normal_texture) {
        normal_texture =
            texture_pool->get_default_texture(Mg::gfx::TexturePool::DefaultTexture::NormalsFlat);
    }

    Mg::gfx::Texture2D* ao_roughness_metallic_texture =
        load_texture(Mg::Identifier::from_runtime_string(ao_roughness_metallic_filename), false);

    Mg::gfx::Texture2D* specular_texture = nullptr;

    if (!ao_roughness_metallic_texture) {
        specular_texture = load_texture(Mg::Identifier::from_runtime_string(specular_filename),
                                        true);

        if (!specular_texture) {
            specular_texture = texture_pool->get_default_texture(
                Mg::gfx::TexturePool::DefaultTexture::Transparent);
        }
    }

    const bool use_metallic_workflow = ao_roughness_metallic_texture != nullptr;

    Mg::gfx::Material* m = nullptr;
    if (use_metallic_workflow) {
        auto handle = resource_cache->resource_handle<Mg::ShaderResource>(
            "shaders/default_metallic_workflow.mgshader");
        m = material_pool.create(file, handle);
        m->set_sampler("sampler_ao_roughness_metallic", ao_roughness_metallic_texture->handle());
    }
    else {
        auto handle = resource_cache->resource_handle<Mg::ShaderResource>(
            "shaders/default_specular_workflow.mgshader");
        m = material_pool.create(file, handle);
        m->set_sampler("sampler_specular", specular_texture->handle());
    }

    for (auto o : options) {
        m->set_option(o, true);
    }

    m->set_sampler("sampler_diffuse", diffuse_texture->handle());
    m->set_sampler("sampler_normal", normal_texture->handle());

    mesh_renderer.prepare_shader(*m, true, true);

    return m;
#else
    auto material_access =
        resource_cache->access_resource<Mg::MaterialResource>("materials/default.mgmaterial");
    return material_pool.create(*material_access);
#endif
}

Model::Model() = default;
Model::~Model() = default;

void Model::update()
{
    if (physics_body) {
        if (auto dynamic_body = physics_body->as_dynamic_body(); dynamic_body.has_value()) {
            transform = dynamic_body->interpolated_transform();
        }
        else {
            transform = physics_body->get_transform();
        }
    }
}

Model Scene::load_model(Mg::Identifier mesh_file,
                        Mg::span<const MaterialFileAssignment> material_files,
                        Mg::span<const Mg::Identifier> options)
{
    Model model = {};
    model.id = mesh_file;

    const Mg::ResourceAccessGuard access =
        resource_cache->access_resource<Mg::MeshResource>(mesh_file);

    const Mg::Opt<Mg::gfx::MeshHandle> mesh_handle = mesh_pool.get(mesh_file);
    if (mesh_handle.has_value()) {
        model.mesh = mesh_handle.value();
    }
    else {
        model.mesh = mesh_pool.create(*access);
    }

    model.centre = access->bounding_sphere().centre;
    model.aabb = access->axis_aligned_bounding_box();

    // Load skeleton, if any.
    if (!access->joints().empty()) {
        model.skeleton.emplace(mesh_file,
                               access->skeleton_root_transform(),
                               access->joints().size());

        for (size_t i = 0; i < model.skeleton->joints().size(); ++i) {
            model.skeleton->joints()[i] = access->joints()[i];
        }
    }

    // Load animation clips, if any.
    for (const auto& clip : access->animation_clips()) {
        model.clips.push_back(clip);
    }

    // Assign materials to submeshes.
    for (auto&& [submesh_index, material_fname] : material_files) {
        model.material_assignments.push_back(
            { submesh_index, load_material(material_fname, options) });
    }

    if (model.skeleton) {
        model.pose = model.skeleton->get_bind_pose();
    }

    return model;
}

Model& Scene::add_scene_model(Mg::Identifier mesh_file,
                              Mg::span<const MaterialFileAssignment> material_files,
                              Mg::span<const Mg::Identifier> options)
{
    const auto [it, inserted] =
        scene_models.insert({ mesh_file, load_model(mesh_file, material_files, options) });
    Model& model = it->second;

    const Mg::ResourceAccessGuard access =
        resource_cache->access_resource<Mg::MeshResource>(mesh_file);

    Mg::physics::Shape* shape = physics_world->create_mesh_shape(access->data_view());
    model.physics_body = physics_world->create_static_body(mesh_file, *shape, glm::mat4{ 1.0f });

    return model;
}

Model& Scene::add_dynamic_model(Mg::Identifier mesh_file,
                                Mg::span<const MaterialFileAssignment> material_files,
                                Mg::span<const Mg::Identifier> options,
                                glm::vec3 position,
                                Mg::Rotation rotation,
                                glm::vec3 scale,
                                bool enable_physics)
{
    const auto [it, inserted] =
        dynamic_models.insert({ mesh_file, load_model(mesh_file, material_files, options) });
    Model& model = it->second;

    if (enable_physics) {
        const Mg::ResourceAccessGuard access =
            resource_cache->access_resource<Mg::MeshResource>(mesh_file);

        Mg::physics::DynamicBodyParameters body_params = {};
        body_params.type = Mg::physics::DynamicBodyType::Dynamic;
        body_params.mass = 50.0f;
        body_params.friction = 0.5f;

        Mg::physics::Shape* shape =
            physics_world->create_convex_hull(access->vertices(), model.centre, scale);
        // physics_world->create_box_shape(model.aabb.max_corner - model.aabb.min_corner);
        model.physics_body =
            physics_world->create_dynamic_body(mesh_file,
                                               *shape,
                                               body_params,
                                               glm::translate(position) * rotation.to_matrix());
        // model.physics_body->as_dynamic_body()->set_filter_mask(
        //~Mg::physics::CollisionGroup::Character);

        // Add visualization translation relative to centre of mass.
        // Note unusual order: for once we translate before the scale, since the translation is in
        // model space, not world space.
        model.vis_transform = glm::scale(scale) * glm::translate(-model.centre);
    }
    else {
        model.transform = glm::translate(position) * rotation.to_matrix();
        model.vis_transform = glm::scale(scale);
    }

    return model;
}

void Scene::make_input_map()
{
    using namespace Mg::input;

    const auto& kb = app.window().keyboard;

    // clang-format off
    input_map.bind("forward",          kb.key(Keyboard::Key::W));
    input_map.bind("backward",         kb.key(Keyboard::Key::S));
    input_map.bind("left",             kb.key(Keyboard::Key::A));
    input_map.bind("right",            kb.key(Keyboard::Key::D));
    input_map.bind("jump",             kb.key(Keyboard::Key::Space));
    input_map.bind("crouch",           kb.key(Keyboard::Key::LeftControl));
    input_map.bind("lock_camera",      kb.key(Keyboard::Key::E));
    input_map.bind("fullscreen",       kb.key(Keyboard::Key::F4));
    input_map.bind("exit",             kb.key(Keyboard::Key::Esc));
    input_map.bind("toggle_debug_vis", kb.key(Keyboard::Key::F));
    input_map.bind("reset",            kb.key(Keyboard::Key::R));
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
    blur_targets.hor_pass_target_texture = texture_pool->create_render_target(params);

    params.render_target_id = "Blur_vertical";
    blur_targets.vert_pass_target_texture = texture_pool->create_render_target(params);

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

    Mg::gfx::Texture2D* colour_target = texture_pool->create_render_target(params);

    params.render_target_id = "HDR.depth";
    params.texture_format = Mg::gfx::RenderTargetParams::Format::Depth24;

    Mg::gfx::Texture2D* depth_target = texture_pool->create_render_target(params);

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
            post_renderer.post_process(post_render_context,
                                       *blur_material,
                                       hor_target,
                                       vert_target.colour_target()->handle());

            blur_material->set_parameter("source_mip_level", static_cast<int>(mip_i));

            // Vertical pass
            blur_material->set_option("HORIZONTAL", false);
            post_renderer.post_process(post_render_context,
                                       *blur_material,
                                       vert_target,
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
        debug_renderer.draw_ellipsoid(app.window().render_target,
                                      camera.view_proj_matrix(),
                                      params);
    }
}

void Scene::render_skeleton_debug_geometry()
{
    MG_GFX_DEBUG_GROUP("Scene::render_skeleton_debug_geometry")

    for (const auto& [model_id, model] : scene_models) {
        if (model.skeleton.has_value() && model.pose.has_value()) {
            debug_renderer.draw_bones(app.window().render_target,
                                      camera.view_proj_matrix(),
                                      model.transform,
                                      *model.skeleton,
                                      *model.pose);
        }
    }
}

void Scene::load_models()
{
    using namespace Mg::literals;

    add_scene_model("meshes/misc/test_scene_2.mgm",
                    { MaterialFileAssignment{ 0, "buildings/GreenBrick"_id },
                      MaterialFileAssignment{ 1, "buildings/W31_1"_id },
                      MaterialFileAssignment{ 2, "buildings/BigWhiteBricks"_id },
                      MaterialFileAssignment{ 3, "buildings/GreenBrick"_id } },
                    { "PARALLAX"_id });

    add_dynamic_model("meshes/Fox.mgm",
                      { MaterialFileAssignment{ 0, "actors/fox"_id } },
                      { "RIM_LIGHT"_id },
                      { 2.0f, 0.0f, 0.0f },
                      Mg::Rotation(),
                      { 0.01f, 0.01f, 0.01f },
                      false);

    add_dynamic_model("meshes/misc/hestdraugr.mgm"_id,
                      { MaterialFileAssignment{ 0, "actors/HestDraugr"_id } },
                      { "RIM_LIGHT"_id },
                      { -2.0f, 2.0f, 1.0f },
                      Mg::Rotation({ 0.0f, 0.0f, glm::radians(90.0f) }),
                      { 1.0f, 1.0f, 1.0f },
                      true);

    add_dynamic_model("meshes/box.mgm",
                      { MaterialFileAssignment{ 0, "crate"_id } },
                      { "PARALLAX"_id },
                      { 0.0f, 0.0f, 10.0f },
                      Mg::Rotation({ 0.0f, 0.0f, glm::radians(90.0f) }),
                      { 1.0f, 1.0f, 1.0f },
                      true);
}

void Scene::load_materials()
{
    // Create post-process materials
    const auto bloom_handle =
        resource_cache->resource_handle<Mg::ShaderResource>("shaders/post_process_bloom.mgshader");
    auto const blur_handle =
        resource_cache->resource_handle<Mg::ShaderResource>("shaders/post_process_blur.mgshader");

    bloom_material = material_pool.create("bloom_material", bloom_handle);
    blur_material = material_pool.create("blur_material", blur_handle);

    // Create billboard material
    const auto billboard_handle =
        resource_cache->resource_handle<Mg::ShaderResource>("shaders/simple_billboard.mgshader");

    billboard_material = material_pool.create("billboard_material", billboard_handle);
    billboard_material->set_sampler("sampler_diffuse",
                                    load_texture("textures/light_t.dds", true)->handle());

    // Create UI material
    const auto ui_handle =
        resource_cache->resource_handle<Mg::ShaderResource>("shaders/ui_render_test.mgshader");
    ui_material = material_pool.create("ui_material", ui_handle);
    ui_material->set_sampler("sampler_colour",
                             load_texture("textures/ui/book_open_da.dds", true)->handle());

    for (Mg::gfx::Material* material : material_pool.get_all_materials()) {
        Mg::log.message(material->serialize());
    }
}

// Create a lot of random lights
void Scene::generate_lights()
{
    Mg::Random rand(0xdeadbeefbadc0fee);

    for (size_t i = 0; i < k_num_lights; ++i) {
        auto r1 = rand.range(-7.5f, 7.5f);
        auto r2 = rand.range(-7.5f, 7.5f);
        auto pos = glm::vec3(r1, r2, 1.125f);
        auto r3 = rand.f32();
        auto r4 = rand.f32();
        auto r5 = rand.f32();
        glm::vec4 light_colour(r3, r4, r5, 1.0f);

        const auto s = Mg::narrow_cast<float>(std::sin(double(i) * 0.2));

        pos.z += s;

        // Draw a billboard sprite for each light
        {
            Mg::gfx::Billboard& billboard = billboard_render_list.add();
            billboard.pos = pos;
            billboard.colour = light_colour * 10.0f;
            billboard.colour.w = 1.0f;
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
        resource_cache->refresh();
    }
}

void Scene::on_resource_reload(const Mg::FileChangedEvent& event)
{
    MG_GFX_DEBUG_GROUP("Scene::on_resource_reload")

    using namespace Mg::literals;

    const auto resource_name = event.resource.resource_id().str_view();
    Mg::log.message(fmt::format("File '{}' changed and was reloaded.", resource_name));

    auto try_reload = [&](auto&& reload_action) {
        try {
            reload_action();
        }
        catch (...) {
            Mg::log.error("An error occurred when reloading resource from '{}'", resource_name);
        }
    };

    switch (event.resource_type.hash()) {
    case "TextureResource"_hash: {
        try_reload([&] {
            Mg::ResourceAccessGuard<Mg::TextureResource> access(event.resource);
            texture_pool->update(*access);
        });
        break;
    }
    case "MeshResource"_hash: {
        try_reload([&] {
            Mg::ResourceAccessGuard<Mg::MeshResource> access(event.resource);
            mesh_pool.update(*access);
        });
        break;
    }
    case "MaterialResource"_hash: {
        try_reload([&] {
            Mg::ResourceAccessGuard<Mg::MaterialResource> access(event.resource);
            material_pool.update(*access);
        });
        [[fallthrough]];
    }
    case "ShaderResource"_hash:
        try_reload([&] {
            mesh_renderer.drop_shaders();
            billboard_renderer.drop_shaders();
            post_renderer.drop_shaders();
            ui_renderer.drop_shaders();
        });
        break;
    }
}

int main(int /*argc*/, char* /*argv*/[])
{
    try {
        Scene scene;
        scene.init();
        scene.app.run_main_loop(scene);
    }
    catch (const std::exception& e) {
        Mg::log.error(e.what());
        return 1;
    }
}
