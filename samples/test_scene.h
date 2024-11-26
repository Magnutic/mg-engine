#pragma once

#include <mg/core/gfx/mg_bitmap_font.h>
#include <mg/core/gfx/mg_camera.h>
#include <mg/core/gfx/mg_material.h>
#include <mg/core/gfx/mg_simple_scene_renderer.h>
#include <mg/core/input/mg_input.h>
#include <mg/core/mg_player_controller.h>
#include <mg/editor/mg_block_scene_editor.h>
#include <mg/mg_game.h>
#include <mg/scene/mg_block_scene.h>

class TestScene : public Mg::Game {
public:
    TestScene();
    ~TestScene() override;

    MG_MAKE_NON_COPYABLE(TestScene);
    MG_MAKE_NON_MOVABLE(TestScene);

    Mg::gfx::Camera camera;

    std::shared_ptr<Mg::input::ButtonTracker> sample_control_button_tracker;

    std::unique_ptr<Mg::PlayerController> player_controller;
    std::unique_ptr<Mg::EditorController> editor_controller;
    Mg::IPlayerController* current_controller = nullptr;

    std::unique_ptr<Mg::physics::CharacterController> character_controller;

    int debug_visualization = 0;

    void on_simulation_step(const Mg::ApplicationTimeInfo& time_info) override;
    void on_render(double lerp_factor, const Mg::ApplicationTimeInfo& time_info) override;

    std::shared_ptr<Mg::gfx::BitmapFont> font = make_font();

    Mg::BlockScene block_scene;
    Mg::BlockSceneMesh block_scene_mesh_data;
    const Mg::gfx::Mesh* block_scene_mesh = nullptr;
    std::shared_ptr<Mg::BlockSceneEditor> editor =
        std::make_shared<Mg::BlockSceneEditor>(block_scene, window(), font);
    Mg::Opt<Mg::ecs::Entity> block_scene_entity;

    bool should_exit() const override { return m_should_exit; }
    Mg::UpdateTimerConfig update_timer_config() const override;
    Mg::gfx::SceneRenderer& renderer() override { return *m_renderer; }
    const Mg::gfx::ICamera& active_camera() const override { return camera; }

private:
    std::shared_ptr<Mg::gfx::BitmapFont> make_font() const;
    void make_block_scene_entity();

    void create_entities();
    void generate_lights();

    void render_light_debug_geometry();
    void render_skeleton_debug_geometry();

    std::shared_ptr<Mg::gfx::SimpleSceneRendererData> m_renderer_data =
        std::make_shared<Mg::gfx::SimpleSceneRendererData>();
    std::unique_ptr<Mg::gfx::SceneRenderer> m_renderer;

    bool m_should_exit = false;
};
