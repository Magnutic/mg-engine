#include "test_scene.h"

#include <mg/components/mg_advance_animations.h>
#include <mg/components/mg_animation_component.h>
#include <mg/components/mg_dynamic_body_component.h>
#include <mg/components/mg_enqueue_meshes_for_rendering.h>
#include <mg/components/mg_mesh_component.h>
#include <mg/components/mg_static_body_component.h>
#include <mg/components/mg_update_dynamic_body_transforms.h>
#include <mg/core/gfx/mg_debug_renderer.h>
#include <mg/core/gfx/mg_gfx_debug_group.h>
#include <mg/core/gfx/mg_light.h>
#include <mg/core/mg_application_context.h>
#include <mg/core/mg_log.h>
#include <mg/core/mg_unicode.h>
#include <mg/core/mg_window.h>
#include <mg/core/physics/mg_character_controller.h>
#include <mg/core/resources/mg_font_resource.h>
#include <mg/core/resources/mg_material_resource.h>
#include <mg/core/resources/mg_mesh_resource.h>
#include <mg/core/resources/mg_shader_resource.h>
#include <mg/core/resources/mg_sound_resource.h>
#include <mg/core/resources/mg_texture_resource.h>
#include <mg/utils/mg_angle.h>
#include <mg/utils/mg_file_io.h>
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

Scene::Scene(Mg::Config& config, Mg::Window& window)
    : config{ config }
    , window{ window }
    , resource_cache{ std::make_shared<Mg::ResourceCache>(
          std::make_unique<Mg::BasicFileLoader>("../samples/data")) }
    , mesh_pool{ std::make_shared<Mg::gfx::MeshPool>(resource_cache) }
    , texture_pool{ std::make_shared<Mg::gfx::TexturePool>(resource_cache) }
    , material_pool{ std::make_shared<Mg::gfx::MaterialPool>(resource_cache, texture_pool) }
{
    MG_GFX_DEBUG_GROUP_BY_FUNCTION
    using namespace Mg::literals;

    setup_config();
    window.set_cursor_lock_mode(Mg::CursorLockMode::LOCKED);
    window.set_focus_callback([this](bool is_focused) { on_window_focus_change(is_focused); });

    camera.set_aspect_ratio(window.aspect_ratio());
    camera.field_of_view = 80_degrees;

    renderer_data = make_renderer_data();
    Mg::gfx::SimpleSceneRendererConfig renderer_config = {
        .sky_material = material_pool->get_or_load("materials/skybox.hjson"),
        .blur_material = material_pool->load_as_mutable("blur", "materials/blur.hjson"),
        .bloom_material = material_pool->load_as_mutable("bloom", "materials/bloom.hjson"),
    };
    renderer = std::make_unique<Mg::gfx::SimpleSceneRenderer>(*resource_cache,
                                                              texture_pool,
                                                              window,
                                                              renderer_config,
                                                              renderer_data);

    sample_control_button_tracker = std::make_shared<Mg::input::ButtonTracker>(window);
    sample_control_button_tracker->bind("swap_movement_mode", Mg::input::Key::E);
    sample_control_button_tracker->bind("fullscreen", Mg::input::Key::F4);
    sample_control_button_tracker->bind("exit", Mg::input::Key::Esc);
    sample_control_button_tracker->bind("next_debug_visualization", Mg::input::Key::F);
    sample_control_button_tracker->bind("reset", Mg::input::Key::R);

    auto button_tracker = std::make_shared<Mg::input::ButtonTracker>(window);
    auto mouse_movement_tracker = std::make_shared<Mg::input::MouseMovementTracker>(window);
    player_controller = std::make_unique<Mg::PlayerController>(button_tracker,
                                                               mouse_movement_tracker);
    editor_controller = std::make_unique<Mg::EditorController>(button_tracker,
                                                               mouse_movement_tracker);
    current_controller = player_controller.get();

    physics_world = std::make_unique<Mg::physics::PhysicsWorld>();

    character_controller = std::make_unique<Mg::physics::CharacterController>(
        "Actor"_id, *physics_world, Mg::physics::CharacterControllerSettings{});

    entities.init<Mg::TransformComponent,
                  Mg::StaticBodyComponent,
                  Mg::DynamicBodyComponent,
                  Mg::MeshComponent,
                  Mg::AnimationComponent>();
    create_entities();
    generate_lights();
}

Scene::~Scene()
{
    Mg::write_display_settings(config, window.settings());
    config.write_to_file(config_file);
}

void Scene::simulation_step(const Mg::ApplicationTimeInfo /*time_info*/)
{
    using namespace Mg::literals;

    Mg::gfx::get_debug_render_queue().clear();

    window.poll_input_events();

    auto button_states = sample_control_button_tracker->get_button_events();

    if (button_states["exit"].was_pressed || window.should_close_flag()) {
        m_should_exit = true;
    }

    Mg::update_dynamic_body_transforms(entities);

    // Fullscreen switching
    if (button_states["fullscreen"].was_pressed) {
        Mg::WindowSettings s = window.settings();
        s.fullscreen = !s.fullscreen;
        s.video_mode = {}; // reset resolution etc. to defaults
        window.apply_settings(s);
        camera.set_aspect_ratio(window.aspect_ratio());

        if (window.is_cursor_locked_to_window() && !s.fullscreen) {
            window.release_cursor();
        }
    }

    if (button_states["next_debug_visualization"].was_pressed) {
        ++debug_visualization;
    }

    if (button_states["swap_movement_mode"].was_pressed) {
        if (current_controller == player_controller.get()) {
            current_controller = editor_controller.get();
        }
        else {
            current_controller = player_controller.get();
        }
    }

    if (button_states["reset"].was_pressed) {
        character_controller->reset();
        character_controller->set_position({ 0.0f, 0.0f, 0.0f });
        create_entities();
    }

    current_controller->handle_movement_inputs(*character_controller);
    physics_world->update(1.0f / k_steps_per_second);
    character_controller->update(1.0f / k_steps_per_second);
}

void Scene::render(const double lerp_factor, const Mg::ApplicationTimeInfo time_info)
{
    MG_GFX_DEBUG_GROUP_BY_FUNCTION

    // Mouselook. Doing this in render step instead of in simulation_step to minimize input lag.
    {
        window.poll_input_events();
        current_controller->handle_rotation_inputs(config.as<float>("mouse_sensitivity_x"),
                                                   config.as<float>("mouse_sensitivity_y"));
        camera.rotation = current_controller->get_rotation();
    }

    camera.position = character_controller->get_position(float(lerp_factor));
    camera.position.z += character_controller->current_height_smooth(float(lerp_factor)) * 0.95f;
    camera.exposure = -5.0f;

    Mg::advance_animations(entities, float(time_info.time_since_init));

    // Draw meshes and billboards.
    renderer_data->mesh_render_command_producer->clear();
    Mg::enqueue_meshes_for_rendering(entities,
                                     *renderer_data->mesh_render_command_producer,
                                     float(lerp_factor));

    // Draw UI
    {
        const glm::vec3 v = character_controller->velocity();
        const glm::vec3 p = character_controller->get_position();

        auto text = fmt::format("FPS: {:.2f}", time_info.frames_per_second);
        text += fmt::format("\nLast frame time: {:.2f} ms",
                            time_info.last_frame_time_seconds * 1'000);
        text += fmt::format("\nVelocity: {{{:.2f}, {:.2f}, {:.2f}}}", v.x, v.y, v.z);
        text += fmt::format("\nPosition: {{{:.2f}, {:.2f}, {:.2f}}}", p.x, p.y, p.z);
        text += fmt::format("\nGrounded: {:b}", character_controller->is_on_ground());
        renderer_data->ui_render_list->text_to_draw = std::move(text);
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
        physics_world->draw_debug(Mg::gfx::get_debug_render_queue());
        break;
    default:
        break;
    }

    renderer->render({ .camera = camera, .time_since_init = float(time_info.time_since_init) });
    window.swap_buffers();
}

Mg::UpdateTimerConfig Scene::update_timer_config() const
{
    return {
        .simulation_steps_per_second = k_steps_per_second,
        .max_frames_per_second = 120,
        .decouple_rendering_from_time_step = true,
        .max_time_steps_at_once = 10,
    };
}

void Scene::setup_config()
{
    config.set_default_value("mouse_sensitivity_x", 0.4f);
    config.set_default_value("mouse_sensitivity_y", 0.4f);
}

std::unique_ptr<Mg::gfx::BitmapFont> Scene::make_font() const
{
    const auto font_resource =
        resource_cache->resource_handle<Mg::FontResource>("fonts/elstob/Elstob-Regular.ttf");
    std::vector<Mg::UnicodeRange> unicode_ranges = { Mg::get_unicode_range(
        Mg::UnicodeBlock::Basic_Latin) };
    return std::make_unique<Mg::gfx::BitmapFont>(font_resource, 24, unicode_ranges);
}

std::shared_ptr<Mg::gfx::SimpleSceneRendererData> Scene::make_renderer_data()
{
    auto data = std::make_shared<Mg::gfx::SimpleSceneRendererData>();
    data->scene_lights = std::make_shared<Mg::gfx::SceneLights>();
    data->mesh_render_command_producer = std::make_shared<Mg::gfx::RenderCommandProducer>();
    data->billboard_render_list = std::make_shared<Mg::gfx::BillboardRenderList>();
    data->ui_render_list = std::make_shared<Mg::gfx::UIRenderList>();
    data->ui_render_list->font = make_font();
    return data;
}

void Scene::load_model(
    Mg::Identifier mesh_file,
    std::initializer_list<std::pair<Mg::Identifier, Mg::Identifier>> material_bindings,
    Mg::ecs::Entity entity)
{
    auto& mesh = entities.add_component<Mg::MeshComponent>(entity);
    mesh.mesh = mesh_pool->get_or_load(mesh_file);

    if (mesh.mesh->animation_data) {
        auto& animation = entities.add_component<Mg::AnimationComponent>(entity);
        animation.pose = mesh.mesh->animation_data->skeleton.get_bind_pose();
    }

    mesh.material_bindings.resize(material_bindings.size());
    for (auto&& [binding_id, filename] : material_bindings) {
        mesh.material_bindings.push_back({ .material_binding_id = binding_id,
                                           .material = material_pool->get_or_load(filename) });
    }

    for (auto&& submesh : mesh.mesh->submeshes) {
        auto has_binding_for_submesh = [&](const Mg::gfx::MaterialBinding& binding) {
            return binding.material_binding_id == submesh.material_binding_id;
        };
        if (std::ranges::find_if(mesh.material_bindings, has_binding_for_submesh) !=
            mesh.material_bindings.end()) {
            continue;
        }
        mesh.material_bindings.push_back(
            { .material_binding_id = submesh.material_binding_id, .material = default_material });
    }
}

Mg::ecs::Entity Scene::add_static_object(
    Mg::Identifier mesh_file,
    std::initializer_list<std::pair<Mg::Identifier, Mg::Identifier>> material_bindings,
    const glm::mat4& transform)
{
    const auto entity = entities.create_entity();
    load_model(mesh_file, material_bindings, entity);

    const Mg::ResourceAccessGuard access =
        resource_cache->access_resource<Mg::MeshResource>(mesh_file);

    Mg::physics::Shape* shape = physics_world->create_mesh_shape(access->data_view());

    auto static_body = physics_world->create_static_body(mesh_file, *shape, transform);
    entities.add_component<Mg::StaticBodyComponent>(entity, static_body);
    entities.add_component<Mg::TransformComponent>(entity, transform, transform);

    return entity;
}

Mg::ecs::Entity Scene::add_dynamic_object(
    Mg::Identifier mesh_file,
    std::initializer_list<std::pair<Mg::Identifier, Mg::Identifier>> material_bindings,
    glm::vec3 position,
    Mg::Rotation rotation,
    glm::vec3 scale,
    bool enable_physics)
{
    const auto entity = entities.create_entity();
    load_model(mesh_file, material_bindings, entity);

    auto& mesh = entities.get_component<Mg::MeshComponent>(entity);
    auto& transform = entities.add_component<Mg::TransformComponent>(entity);

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
        entities.add_component<Mg::DynamicBodyComponent>(entity, dynamic_body);

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

    for (const Mg::gfx::Light& light : renderer_data->scene_lights->point_lights) {
        if (light.vector.w == 0.0) {
            continue;
        }
        Mg::gfx::DebugRenderer::EllipsoidDrawParams params;
        params.centre = glm::vec3(light.vector);
        params.colour = glm::vec4(normalize(light.colour), 0.05f);
        params.dimensions = glm::vec3(std::sqrt(light.range_sqr));
        params.wireframe = true;
        Mg::gfx::get_debug_render_queue().draw_ellipsoid(params);
    }
}

void Scene::render_skeleton_debug_geometry()
{
    MG_GFX_DEBUG_GROUP("Scene::render_skeleton_debug_geometry")

    for (auto [entity, transform, mesh, animation] :
         entities.get_with<Mg::TransformComponent, Mg::MeshComponent, Mg::AnimationComponent>()) {
        Mg::gfx::get_debug_render_queue().draw_bones(transform.transform * mesh.mesh_transform,
                                                     mesh.mesh->animation_data->skeleton,
                                                     animation.pose);
    }
}

void Scene::create_entities()
{
    using namespace Mg::literals;

    entities.reset();

    add_static_object("meshes/misc/test_scene_2.mgm",
                      {
                          { "FloorMaterial", "materials/buildings/GreenBrick.hjson" },
                          { "WallMaterial", "materials/buildings/W31_1.hjson" },
                          { "PillarsAndStepsMaterial", "materials/buildings/BigWhiteBricks.hjson" },
                          { "AltarBorderMaterial", "materials/buildings/GreenBrick.hjson" },
                      },
                      glm::mat4(1.0f));

    auto fox_entity = add_dynamic_object("meshes/Fox.mgm",
                                         { { "fox_material", "materials/actors/fox.hjson" } },
                                         { 1.0f, 1.0f, 0.0f },
                                         Mg::Rotation(),
                                         { 0.01f, 0.01f, 0.01f },
                                         false);
    entities.get_component<Mg::AnimationComponent>(fox_entity).current_clip = 2;

    add_dynamic_object("meshes/misc/hestdraugr.mgm"_id,
                       { { "hestdraugr_material", "materials/actors/HestDraugr.hjson" } },
                       { -2.0f, 2.0f, 1.05f },
                       Mg::Rotation({ 0.0f, 0.0f, glm::radians(90.0f) }),
                       { 1.0f, 1.0f, 1.0f },
                       true);

    add_dynamic_object("meshes/box.mgm",
                       { { "cube_material", "materials/crate.hjson" } },
                       { 0.0f, 2.0f, 0.5f },
                       Mg::Rotation({ 0.0f, 0.0f, glm::radians(90.0f) }),
                       { 1.0f, 1.0f, 1.0f },
                       true);
}

// Create a lot of random lights
void Scene::generate_lights()
{
    Mg::Random rand(0xdeadbeefbadc0fee);

    auto& [material,
           billboards] = renderer_data->billboard_render_list->render_lists.emplace_back();
    material = material_pool->get_or_load("materials/billboard.hjson");

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
            auto& billboard = billboards.emplace_back();
            billboard.pos = pos;
            billboard.colour = light_colour * 10.0f;
            billboard.colour.w = 1.0f;
            billboard.radius = 0.05f;
        }

        renderer_data->scene_lights->point_lights.push_back(
            Mg::gfx::make_point_light(pos, light_colour * 10.0f, k_light_radius));
    }
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
        Mg::Config config{ config_file };
        Mg::Window window{ read_display_settings(config), window_title };

        Scene scene{ config, window };
        Mg::ApplicationContext{}.run_main_loop(scene);
    }
    catch (const std::exception& e) {
        Mg::log.error(e.what());
        return 1;
    }
}
