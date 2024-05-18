#include "test_scene.h"
#include "mg/physics/mg_character_controller.h"
#include "mg/utils/mg_file_io.h"

#include <mg/core/mg_config.h>
#include <mg/core/mg_log.h>
#include <mg/core/mg_window.h>
#include <mg/gfx/mg_gfx_debug_group.h>
#include <mg/gfx/mg_gfx_device.h>
#include <mg/gfx/mg_light.h>
#include <mg/mg_unicode.h>
#include <mg/resources/mg_font_resource.h>
#include <mg/resources/mg_material_resource.h>
#include <mg/resources/mg_mesh_resource.h>
#include <mg/resources/mg_shader_resource.h>
#include <mg/resources/mg_sound_resource.h>
#include <mg/resources/mg_texture_resource.h>
#include <mg/utils/mg_angle.h>
#include <mg/utils/mg_rand.h>

#include <fmt/core.h>

namespace {

using namespace Mg::literals;

constexpr auto config_file = "mg_engine.cfg";
constexpr auto window_title = "Mg Engine Test Scene";

constexpr int k_steps_per_second = 60.0;
constexpr size_t k_num_lights = 128;
constexpr float k_light_radius = 3.0f;

} // namespace

Scene::Scene() : app(config_file, window_title) {}

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
    }

    app.gfx_device().set_clear_colour(0.0125f, 0.01275f, 0.025f, 1.0f);

    camera.set_aspect_ratio(app.window().aspect_ratio());
    camera.field_of_view = 80_degrees;

    sample_control_button_tracker = std::make_shared<Mg::input::ButtonTracker>(app.window());
    sample_control_button_tracker->bind("lock_camera", Mg::input::Key::E);
    sample_control_button_tracker->bind("fullscreen", Mg::input::Key::F4);
    sample_control_button_tracker->bind("exit", Mg::input::Key::Esc);
    sample_control_button_tracker->bind("next_debug_visualization", Mg::input::Key::F);
    sample_control_button_tracker->bind("reset", Mg::input::Key::R);

    physics_world = std::make_unique<Mg::physics::World>();
    player_controller = std::make_unique<Mg::PlayerController>(
        std::make_shared<Mg::input::ButtonTracker>(app.window()),
        std::make_shared<Mg::input::MouseMovementTracker>(app.window()));
    character_controller = std::make_unique<Mg::physics::CharacterController>(
        "Actor"_id, *physics_world, Mg::physics::CharacterControllerSettings{});

    create_entities();
    generate_lights();

    const auto font_resource =
        resource_cache->resource_handle<Mg::FontResource>("fonts/elstob/Elstob-Regular.ttf");
    std::vector<Mg::UnicodeRange> unicode_ranges = { Mg::get_unicode_range(
        Mg::UnicodeBlock::Basic_Latin) };
    font = std::make_unique<Mg::gfx::BitmapFont>(font_resource, 24, unicode_ranges);
}

void Scene::simulation_step()
{
    using namespace Mg::literals;

    Mg::gfx::get_debug_render_queue().clear();

    app.window().poll_input_events();

    auto button_states = sample_control_button_tracker->get_button_events();

    if (button_states["exit"].was_pressed || app.window().should_close_flag()) {
        m_should_exit = true;
    }

    entities.update();

    // Particle system
#if 0
    particle_system.emit(10);
    particle_system.position = dynamic_models["meshes/box.mgm"].physics_body->get_position();
    glm::mat3 rotation = dynamic_models["meshes/box.mgm"].physics_body->get_transform();
    particle_system.emission_direction =
        rotation * Mg::Rotation()
                       .pitch(-25_degrees)
                       .yaw(Mg::Angle::from_radians(float(app.time_since_init())))
                       .apply_to({ 0.0f, 0.0f, 1.0f });
#endif

    // Fullscreen switching
    if (button_states["fullscreen"].was_pressed) {
        Mg::WindowSettings s = app.window().settings();
        s.fullscreen = !s.fullscreen;
        s.video_mode = {}; // reset resolution etc. to defaults
        app.window().apply_settings(s);
        camera.set_aspect_ratio(app.window().aspect_ratio());

        if (app.window().is_cursor_locked_to_window() && !s.fullscreen) {
            app.window().release_cursor();
        }
    }

    if (button_states["next_debug_visualization"].was_pressed) {
        ++debug_visualization;
    }
    if (button_states["lock_camera"].was_pressed) {
        camera_locked = !camera_locked;
    }
    if (button_states["reset"].was_pressed) {
        character_controller->reset();
        character_controller->set_position({ 0.0f, 0.0f, 0.0f });
        create_entities();
    }

    player_controller->handle_movement_inputs(*character_controller);
    physics_world->update(1.0f / k_steps_per_second);
    character_controller->update(1.0f / k_steps_per_second);
}

void Scene::render(const double lerp_factor)
{
    MG_GFX_DEBUG_GROUP("Scene::render_scene")

    // Mouselook. Doing this in render step instead of in simulation_step to minimize input lag.
    {
        app.window().poll_input_events();
        player_controller->handle_rotation_inputs(app.config().as<float>("mouse_sensitivity_x"),
                                                  app.config().as<float>("mouse_sensitivity_y"));
        camera.rotation = player_controller->rotation;
    }

    if (!camera_locked) {
        camera.position = character_controller->get_position(float(lerp_factor));
        camera.position.z += character_controller->current_height_smooth(float(lerp_factor)) *
                             0.95f;
    }

    scene_lights.back() =
        Mg::gfx::make_point_light(camera.position, glm::vec3(25.0f), 2.0f * k_light_radius);

    // Clear render target.
    app.gfx_device().clear(*render_targets.hdr_target);

    // Draw sky.
    renderers.skybox_renderer.draw(*render_targets.hdr_target, camera, *sky_material);

    entities.animate(float(app.time_since_init()));

    // Draw meshes and billboards.
    {
        render_command_producer.clear();

        entities.add_meshes_to_render_list(render_command_producer, float(lerp_factor));

        const auto& commands = render_command_producer.finalize(camera,
                                                                Mg::gfx::SortingMode::near_to_far);

        Mg::gfx::RenderParameters params = {};
        params.current_time = Mg::narrow_cast<float>(app.time_since_init());
        params.camera_exposure = -5.0;

        renderers.mesh_renderer.render(camera,
                                       commands,
                                       scene_lights,
                                       *render_targets.hdr_target,
                                       params);

        renderers.billboard_renderer.render(*render_targets.hdr_target,
                                            camera,
                                            billboard_render_list,
                                            *billboard_material);

        particle_system.update(static_cast<float>(app.performance_info().last_frame_time_seconds));
        renderers.billboard_renderer.render(*render_targets.hdr_target,
                                            camera,
                                            particle_system.particles(),
                                            *particle_material);
    }

    renderers.blur_renderer.render(renderers.post_renderer,
                                   *render_targets.hdr_target,
                                   *render_targets.blur_target);

    // Apply tonemap and render to window render target.
    {
        app.gfx_device().clear(app.window().render_target);

        bloom_material->set_sampler("sampler_bloom", render_targets.blur_target->target_texture());

        renderers.post_renderer.post_process(renderers.post_renderer.make_context(),
                                             *bloom_material,
                                             app.window().render_target,
                                             render_targets.hdr_target->colour_target()->handle());
    }

    // Draw UI
    renderers.ui_renderer.resolution(app.window().frame_buffer_size());

    {
        Mg::gfx::UIPlacement placement = {};
        placement.position = Mg::gfx::UIPlacement::top_left + glm::vec2(0.01f, -0.01f);
        placement.anchor = Mg::gfx::UIPlacement::top_left;

        Mg::gfx::TypeSetting typesetting = {};
        typesetting.line_spacing_factor = 1.25f;
        typesetting.max_width_pixels = renderers.ui_renderer.resolution().width;

        const glm::vec3 v = character_controller->velocity();
        const glm::vec3 p = character_controller->get_position();

        std::string text = fmt::format("FPS: {:.2f}", app.performance_info().frames_per_second);
        text += fmt::format("\nLast frame time: {:.2f} ms",
                            app.performance_info().last_frame_time_seconds * 1'000);
        text += fmt::format("\nVelocity: {{{:.2f}, {:.2f}, {:.2f}}}", v.x, v.y, v.z);
        text += fmt::format("\nPosition: {{{:.2f}, {:.2f}, {:.2f}}}", p.x, p.y, p.z);
        text += fmt::format("\nGrounded: {:b}", character_controller->is_on_ground());
        text += fmt::format("\nNum particles: {}", particle_system.particles().size());

        renderers.ui_renderer.draw_text(app.window().render_target,
                                        placement,
                                        font->prepare_text(text, typesetting));
    }

    // Debug geometry
    switch (debug_visualization % 4) {
    case 1:
        render_light_debug_geometry();
        break;
    case 2:
        render_skeleton_debug_geometry();
        break;
    case 3:
        physics_world->draw_debug(app.window().render_target,
                                  renderers.debug_renderer,
                                  camera.view_proj_matrix());
        break;
    default:
        break;
    }

    Mg::gfx::get_debug_render_queue().dispatch(app.window().render_target,
                                               renderers.debug_renderer,
                                               camera.view_proj_matrix());

    app.window().swap_buffers();
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
    cfg.set_default_value("mouse_sensitivity_x", 0.4f);
    cfg.set_default_value("mouse_sensitivity_y", 0.4f);
}

void Scene::load_model(Mg::Identifier mesh_file,
                       std::span<const Mg::gfx::MaterialBinding> material_bindings,
                       Mg::ecs::Entity entity)
{
    auto& mesh = entities.collection.add_component<MeshComponent>(entity);
    mesh.mesh = mesh_pool->get_or_load(mesh_file);

    if (mesh.mesh->animation_data) {
        auto& animation = entities.collection.add_component<AnimationComponent>(entity);
        animation.pose = mesh.mesh->animation_data->skeleton.get_bind_pose();
    }

    mesh.material_bindings.resize(material_bindings.size());
    std::ranges::copy(material_bindings, mesh.material_bindings.begin());
}

Mg::ecs::Entity
Scene::add_static_object(Mg::Identifier mesh_file,
                         std::span<const Mg::gfx::MaterialBinding> material_bindings,
                         const glm::mat4& transform)
{
    const auto entity = entities.collection.create_entity();
    load_model(mesh_file, material_bindings, entity);

    const Mg::ResourceAccessGuard access =
        resource_cache->access_resource<Mg::MeshResource>(mesh_file);

    Mg::physics::Shape* shape = physics_world->create_mesh_shape(access->data_view());

    auto static_body = physics_world->create_static_body(mesh_file, *shape, transform);
    entities.collection.add_component<StaticBodyComponent>(entity, static_body);
    entities.collection.add_component<TransformComponent>(entity, transform, transform);

    return entity;
}

Mg::ecs::Entity
Scene::add_dynamic_object(Mg::Identifier mesh_file,
                          std::span<const Mg::gfx::MaterialBinding> material_bindings,
                          glm::vec3 position,
                          Mg::Rotation rotation,
                          glm::vec3 scale,
                          bool enable_physics)
{
    const auto entity = entities.collection.create_entity();
    load_model(mesh_file, material_bindings, entity);

    auto& mesh = entities.collection.get_component<MeshComponent>(entity);
    auto& transform = entities.collection.add_component<TransformComponent>(entity);

    if (enable_physics) {
        const Mg::ResourceAccessGuard access =
            resource_cache->access_resource<Mg::MeshResource>(mesh_file);

        const glm::vec3 centre = access->bounding_sphere().centre;

        Mg::physics::DynamicBodyParameters body_params = {};
        body_params.type = Mg::physics::DynamicBodyType::Dynamic;
        body_params.mass = 50.0f;
        body_params.friction = 0.5f;

        Mg::physics::Shape* shape =
            physics_world->create_convex_hull(access->vertices(), centre, scale);

        auto dynamic_body =
            physics_world->create_dynamic_body(mesh_file,
                                               *shape,
                                               body_params,
                                               glm::translate(position) * rotation.to_matrix());
        entities.collection.add_component<DynamicBodyComponent>(entity, dynamic_body);

        // Add visualization translation relative to centre of mass.
        // Note unusual order: for once we translate before the scale, since the translation is in
        // model space, not world space.
        mesh.mesh_transform = glm::scale(scale) * glm::translate(-centre);
    }
    else {
        transform.transform = glm::translate(position) * rotation.to_matrix();
        transform.previous_transform = transform.transform;
        mesh.mesh_transform = glm::scale(scale);
    }

    return entity;
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
        renderers.debug_renderer.draw_ellipsoid(app.window().render_target,
                                                camera.view_proj_matrix(),
                                                params);
    }
}

void Scene::render_skeleton_debug_geometry()
{
    MG_GFX_DEBUG_GROUP("Scene::render_skeleton_debug_geometry")

    for (auto [entity, transform, mesh, animation] :
         entities.collection.get_with<TransformComponent, MeshComponent, AnimationComponent>()) {
        renderers.debug_renderer.draw_bones(app.window().render_target,
                                            camera.view_proj_matrix(),
                                            transform.transform * mesh.mesh_transform,
                                            mesh.mesh->animation_data->skeleton,
                                            animation.pose);
    }
}

void Scene::create_entities()
{
    using namespace Mg::literals;

    entities.collection.reset();

    add_static_object("meshes/misc/test_scene_2.mgm",
                      std::array{
                          Mg::gfx::MaterialBinding{
                              "FloorMaterial",
                              material_pool->get_or_load("materials/buildings/GreenBrick.hjson"),
                          },
                          Mg::gfx::MaterialBinding{
                              "WallMaterial",
                              material_pool->get_or_load("materials/buildings/W31_1.hjson"),
                          },
                          Mg::gfx::MaterialBinding{
                              "PillarsAndStepsMaterial",
                              material_pool->get_or_load(
                                  "materials/buildings/BigWhiteBricks.hjson"),
                          },
                          Mg::gfx::MaterialBinding{
                              "AltarBorderMaterial",
                              material_pool->get_or_load("materials/buildings/GreenBrick.hjson"),
                          },
                      },
                      glm::mat4(1.0f));

    auto man_entity =
        add_dynamic_object("meshes/CesiumMan.mgm",
                           std::array{
                               Mg::gfx::MaterialBinding{
                                   "Cesium_Man-effect",
                                   material_pool->get_or_load("materials/actors/fox.hjson"),
                               },
                           },
                           { 2.0f, 0.0f, 0.0f },
                           Mg::Rotation(),
                           { 1.0f, 1.0f, 1.0f },
                           false);
    entities.collection.get_component<AnimationComponent>(man_entity).current_clip = 0;

    auto fox_entity =
        add_dynamic_object("meshes/Fox.mgm",
                           std::array{
                               Mg::gfx::MaterialBinding{
                                   "fox_material",
                                   material_pool->get_or_load("materials/actors/fox.hjson"),
                               },
                           },
                           { 1.0f, 1.0f, 0.0f },
                           Mg::Rotation(),
                           { 0.01f, 0.01f, 0.01f },
                           false);
    entities.collection.get_component<AnimationComponent>(fox_entity).current_clip = 2;

    add_dynamic_object("meshes/misc/hestdraugr.mgm"_id,
                       std::array{
                           Mg::gfx::MaterialBinding{
                               "hestdraugr_material",
                               material_pool->get_or_load("materials/actors/HestDraugr.hjson"),
                           },
                       },
                       { -2.0f, 2.0f, 1.05f },
                       Mg::Rotation({ 0.0f, 0.0f, glm::radians(90.0f) }),
                       { 1.0f, 1.0f, 1.0f },
                       true);

    add_dynamic_object("meshes/box.mgm",
                       std::array{
                           Mg::gfx::MaterialBinding{
                               "cube_material",
                               material_pool->get_or_load("materials/crate.hjson"),
                           },
                       },
                       { 0.0f, 2.0f, 0.5f },
                       Mg::Rotation({ 0.0f, 0.0f, glm::radians(90.0f) }),
                       { 1.0f, 1.0f, 1.0f },
                       true);
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
            Mg::gfx::Billboard& billboard = billboard_render_list.emplace_back();
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
