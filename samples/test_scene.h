#pragma once

#include "mg/core/gfx/mg_camera.h"
#include <mg/core/gfx/mg_bitmap_font.h>
#include <mg/core/gfx/mg_material.h>
#include <mg/core/gfx/mg_simple_scene_renderer.h>
#include <mg/core/input/mg_input.h>
#include <mg/core/mg_player_controller.h>
#include <mg/mg_game.h>

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
    bool should_exit() const override { return m_should_exit; }
    Mg::UpdateTimerConfig update_timer_config() const override;
    Mg::gfx::SceneRenderer& renderer() override { return *m_renderer; }
    const Mg::gfx::ICamera& active_camera() const override { return camera; }

private:
    std::unique_ptr<Mg::gfx::BitmapFont> make_font() const;
    std::shared_ptr<Mg::gfx::SimpleSceneRendererData> make_renderer_data();
    void create_entities();
    void generate_lights();

    void render_light_debug_geometry();
    void render_skeleton_debug_geometry();

    std::shared_ptr<Mg::gfx::SimpleSceneRendererData> m_renderer_data;
    std::unique_ptr<Mg::gfx::SceneRenderer> m_renderer;

    bool m_should_exit = false;
};
