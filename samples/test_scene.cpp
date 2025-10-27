#include "test_scene.h"
#include "mg/mg_game.h"

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
constexpr auto data_path = "../samples/data";

constexpr int k_steps_per_second = 60.0;
constexpr size_t k_num_lights = 128;
constexpr float k_light_radius = 3.0f;

std::vector<std::unique_ptr<Mg::IFileLoader>> make_file_loaders()
{
    std::vector<std::unique_ptr<Mg::IFileLoader>> result;
    result.push_back(std::make_unique<Mg::BasicFileLoader>(data_path));
    return result;
}
} // namespace

TestScene::TestScene()
    : Game{ {
          .window_title = window_title,
          .config_file_path = config_file,
          .file_loaders = make_file_loaders(),
          .max_num_entities = 1024,
      } }
{
    MG_GFX_DEBUG_GROUP_BY_FUNCTION
    using namespace Mg::literals;

    config().set_default_value("mouse_sensitivity_x", 0.4f);
    config().set_default_value("mouse_sensitivity_y", 0.4f);

    camera.set_aspect_ratio(window().aspect_ratio());
    camera.field_of_view = 80_degrees;

    m_renderer_data = make_renderer_data();
    Mg::gfx::SimpleSceneRendererConfig renderer_config = {
        .sky_material = material_pool()->get_or_load("materials/skybox.hjson"),
        .blur_material = material_pool()->load_as_mutable("blur", "materials/blur.hjson"),
        .bloom_material = material_pool()->load_as_mutable("bloom", "materials/bloom.hjson"),
    };
    m_renderer = std::make_unique<Mg::gfx::SimpleSceneRenderer>(*resource_cache(),
                                                                texture_pool(),
                                                                window(),
                                                                renderer_config,
                                                                m_renderer_data);

    sample_control_button_tracker = std::make_shared<Mg::input::ButtonTracker>(window());
    sample_control_button_tracker->bind("swap_movement_mode", Mg::input::Key::E);
    sample_control_button_tracker->bind("fullscreen", Mg::input::Key::F4);
    sample_control_button_tracker->bind("exit", Mg::input::Key::Esc);
    sample_control_button_tracker->bind("next_debug_visualization", Mg::input::Key::F);
    sample_control_button_tracker->bind("reset", Mg::input::Key::R);

    player_controller =
        std::make_unique<Mg::PlayerController>(std::make_shared<Mg::input::ButtonTracker>(window()),
                                               std::make_shared<Mg::input::MouseMovementTracker>(
                                                   window()));
    editor_controller =
        std::make_unique<Mg::EditorController>(std::make_shared<Mg::input::ButtonTracker>(window()),
                                               std::make_shared<Mg::input::MouseMovementTracker>(
                                                   window()));
    current_controller = player_controller.get();

    character_controller = std::make_unique<Mg::physics::CharacterController>(
        "Actor"_id, physics_world(), Mg::physics::CharacterControllerSettings{});

    create_entities();
    generate_lights();
}

TestScene::~TestScene()
{
    Mg::write_display_settings(config(), window().settings());
    config().write_to_file(config_file);
}

void TestScene::on_simulation_step(const Mg::ApplicationTimeInfo& /*time_info*/)
{
    using namespace Mg::literals;

    auto button_states = sample_control_button_tracker->get_button_events();

    if (button_states["exit"].was_pressed || window().should_close_flag()) {
        m_should_exit = true;
    }

    // Fullscreen switching
    if (button_states["fullscreen"].was_pressed) {
        Mg::WindowSettings s = window().settings();
        s.fullscreen = !s.fullscreen;
        s.video_mode = {}; // reset resolution etc. to defaults
        window().apply_settings(s);
        camera.set_aspect_ratio(window().aspect_ratio());

        if (window().is_cursor_locked_to_window() && !s.fullscreen) {
            window().release_cursor();
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
    character_controller->update(1.0f / k_steps_per_second);
}

void TestScene::on_render(const double lerp_factor, const Mg::ApplicationTimeInfo& time_info)
{
    MG_GFX_DEBUG_GROUP_BY_FUNCTION

    // Mouselook. Doing this in render step instead of in simulation_step to minimize input lag.
    {
        window().poll_input_events();
        current_controller->handle_rotation_inputs(config().as<float>("mouse_sensitivity_x"),
                                                   config().as<float>("mouse_sensitivity_y"));
        camera.rotation = current_controller->get_rotation();
    }

    camera.position = character_controller->get_position(float(lerp_factor));
    camera.position.z += character_controller->current_height_smooth(float(lerp_factor)) * 0.90f;
    camera.exposure = -5.0f;

    // Draw meshes and billboards.
    m_renderer_data->mesh_render_command_producer->clear();
    Mg::enqueue_meshes_for_rendering(entities(),
                                     *m_renderer_data->mesh_render_command_producer,
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
        m_renderer_data->ui_render_list->text_to_draw = std::move(text);
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
        physics_world().draw_debug(Mg::gfx::get_debug_render_queue());
        break;
    default:
        break;
    }

    m_renderer->render({ .camera = camera, .time_since_init = float(time_info.time_since_init) });
    window().swap_buffers();
}

Mg::UpdateTimerConfig TestScene::update_timer_config() const
{
    return {
        .simulation_steps_per_second = k_steps_per_second,
        .max_frames_per_second = 120,
        .decouple_rendering_from_time_step = true,
        .max_time_steps_at_once = 10,
    };
}

std::unique_ptr<Mg::gfx::BitmapFont> TestScene::make_font() const
{
    const auto font_resource =
        resource_cache()->resource_handle<Mg::FontResource>("fonts/elstob/Elstob-Regular.ttf");
    std::vector<Mg::UnicodeRange> unicode_ranges = { Mg::get_unicode_range(
        Mg::UnicodeBlock::Basic_Latin) };
    return std::make_unique<Mg::gfx::BitmapFont>(font_resource, 24, unicode_ranges);
}

std::shared_ptr<Mg::gfx::SimpleSceneRendererData> TestScene::make_renderer_data()
{
    auto data = std::make_shared<Mg::gfx::SimpleSceneRendererData>();
    data->scene_lights = std::make_shared<Mg::gfx::SceneLights>();
    data->mesh_render_command_producer = std::make_shared<Mg::gfx::RenderCommandProducer>();
    data->billboard_render_list = std::make_shared<Mg::gfx::BillboardRenderList>();
    data->ui_render_list = std::make_shared<Mg::gfx::UIRenderList>();
    data->ui_render_list->font = make_font();
    return data;
}

void TestScene::render_light_debug_geometry()
{
    MG_GFX_DEBUG_GROUP("TestScene::render_light_debug_geometry")

    for (const Mg::gfx::Light& light : m_renderer_data->scene_lights->point_lights) {
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

void TestScene::render_skeleton_debug_geometry()
{
    MG_GFX_DEBUG_GROUP("TestScene::render_skeleton_debug_geometry")

    for (auto [entity, transform, mesh, animation] :
         entities().get_with<Mg::TransformComponent, Mg::MeshComponent, Mg::AnimationComponent>()) {
        Mg::gfx::get_debug_render_queue().draw_bones(transform.transform * mesh.mesh_transform,
                                                     mesh.mesh->animation_data->skeleton,
                                                     animation.pose);
    }
}

void TestScene::create_entities()
{
    using namespace Mg::literals;

    entities().reset();

    add_static_object(
        { .mesh_file = "meshes/misc/test_scene_2.mgm",
          .material_bindings = { { "FloorMaterial", "materials/buildings/GreenBrick.hjson" },
                                 { "WallMaterial", "materials/buildings/W31_1.hjson" },
                                 { "PillarsAndStepsMaterial",
                                   "materials/buildings/BigWhiteBricks.hjson" },
                                 { "AltarBorderMaterial",
                                   "materials/buildings/GreenBrick.hjson" } } },
        {});

    auto fox_entity =
        add_dynamic_object("fox",
                           {
                               .mesh_file = "meshes/Fox.mgm",
                               .material_bindings = { { "fox_material",
                                                        "materials/actors/fox.hjson" } },
                           },
                           {
                               .position = { 1.0f, 1.0f, 0.0f },
                               .rotation = {},
                               .scale = { 0.01f, 0.01f, 0.01f },
                           },
                           Mg::nullopt);

    entities().get_component<Mg::AnimationComponent>(fox_entity).current_clip = 2;

    add_dynamic_object("hest",
                       {
                           .mesh_file = "meshes/misc/hestdraugr.mgm"_id,
                           .material_bindings = { { "hestdraugr_material",
                                                    "materials/actors/HestDraugr.hjson" } },
                       },
                       {
                           .position{ -2.0f, 2.0f, 1.05f },
                           .rotation = Mg::Rotation({ 0.0f, 0.0f, glm::radians(90.0f) }),
                           .scale = { 1.0f, 1.0f, 1.0f },
                       },
                       Mg::physics::DynamicBodyParameters{ .mass = 150 });

    add_dynamic_object("box",
                       {
                           .mesh_file = "meshes/box.mgm",
                           .material_bindings = { { "cube_material", "materials/crate.hjson" } },
                       },
                       {
                           .position = { 0.0f, 2.0f, 0.5f },
                           .rotation = Mg::Rotation({ 0.0f, 0.0f, glm::radians(90.0f) }),
                           .scale = { 1.0f, 1.0f, 1.0f },
                       },
                       Mg::physics::DynamicBodyParameters{ .mass = 30 });
}

// Create a lot of random lights
void TestScene::generate_lights()
{
    Mg::Random rand(0xdeadbeefbadc0fee);

    auto& [material,
           billboards] = m_renderer_data->billboard_render_list->render_lists.emplace_back();
    material = material_pool()->get_or_load("materials/billboard.hjson");

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

        m_renderer_data->scene_lights->point_lights.push_back(
            Mg::gfx::make_point_light(pos, light_colour * 10.0f, k_light_radius));
    }
}

int main(int /*argc*/, char* /*argv*/[])
{
    try {
        TestScene scene;
        Mg::ApplicationContext{}.run_main_loop(scene);
    }
    catch (const std::exception& e) {
        Mg::log.error(e.what());
        return 1;
    }
}
