#include <mg/core/mg_application_context.h>
#include <mg/core/mg_window.h>
#include <mg/editor/mg_curve_editor.h>

Mg::editor::CurveEditorSettings get_settings()
{
    Mg::editor::CurveEditorSettings settings;
    return settings; // Use defaults;
}

class CurveEditorApp : public Mg::IApplication {
public:
    explicit CurveEditorApp(Mg::Window& window)
        : m_window{ window }, m_editor{ window, get_settings() }
    {}

    Mg::Curve curve;

protected:
    void simulation_step(Mg::ApplicationTimeInfo /*time_info*/) override
    {
        // Do nothing, all work is done in render.
    }

    void render(double /*lerp_factor*/, Mg::ApplicationTimeInfo /*time_info*/) override
    {
        m_window.poll_input_events();
        m_editor.update(curve);
        m_window.swap_buffers();
    }

    bool should_exit() const override { return m_window.should_close_flag(); }

    Mg::UpdateTimerConfig update_timer_config() const override
    {
        Mg::UpdateTimerConfig config;
        config.max_frames_per_second = 60;
        config.simulation_steps_per_second = 60;
        config.decouple_rendering_from_time_step = false;
        config.max_time_steps_at_once = 10;
        return config;
    }

private:
    Mg::Window& m_window;
    Mg::editor::CurveEditor m_editor;
};

int main()
{
    Mg::Window window{ { .video_mode = { 1280, 768 }, .fullscreen = false, .vsync = true },
                       "Curve Editor" };
    CurveEditorApp app{ window };

    // TODO TEMP test some curve points
    app.curve.insert({ 0.0f, 0.0f });
    app.curve.insert({ 0.1f, 1.0f });
    app.curve.insert({ 0.2f, 0.0f });
    app.curve.insert({ 0.3f, 1.0f });
    app.curve.insert({ 0.4f, 0.0f });
    app.curve.insert({ 0.5f, 1.0f });
    app.curve.insert({ 0.6f, 0.0f });
    app.curve.insert({ 0.7f, 1.0f });
    app.curve.insert({ 0.8f, 0.0f });
    app.curve.insert({ 0.9f, 1.0f });

    app.curve.set_right_tangent(0, 1.0);
    app.curve.set_left_tangent(1, 1.0);

    Mg::ApplicationContext{}.run_main_loop(app);
}
