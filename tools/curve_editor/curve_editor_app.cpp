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
    explicit CurveEditorApp()
        : m_app("curve_editor.cfg", "Mg Engine Curve Editor")
        , m_editor(m_app.window(), get_settings())
    {
        Mg::WindowSettings settings = {};
        settings.fullscreen = false;
        settings.video_mode = Mg::VideoMode{ 1280, 768 };
        settings.vsync = true;
        m_app.window().set_title("Curve Editor");
        m_app.window().apply_settings(settings);
    }

    void run() { m_app.run_main_loop(*this); }

    Mg::Curve curve;

protected:
    void simulation_step() override
    {
        // Do nothing, all work is done in render.
    }

    void render(double /*unused*/) override
    {
        m_app.window().poll_input_events();
        m_editor.update(curve);
        m_app.window().swap_buffers();
    }

    bool should_exit() const override { return m_app.window().should_close_flag(); }

    Mg::UpdateTimerSettings update_timer_settings() const override
    {
        Mg::UpdateTimerSettings settings;
        settings.max_frames_per_second = 60;
        settings.simulation_steps_per_second = 60;
        settings.decouple_rendering_from_time_step = false;
        settings.max_time_steps_at_once = 10;
        return settings;
    }

private:
    Mg::ApplicationContext m_app;
    Mg::editor::CurveEditor m_editor;
};

int main()
{
    CurveEditorApp app;

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

    app.run();
}
