#include "mg/core/mg_window_settings.h"

#include "mg/core/mg_config.h"

namespace Mg {

WindowSettings read_display_settings(Config& cfg)
{
    // Config values of zero leads to using user's desktop video mode.
    cfg.set_default_value("r_display_width", 0);
    cfg.set_default_value("r_display_height", 0);
    cfg.set_default_value("r_fullscreen", 0);
    cfg.set_default_value("r_vsync", 0);

    WindowSettings s;

    // Get window configuration
    s.video_mode.width = cfg.as<int32_t>("r_display_width");
    s.video_mode.height = cfg.as<int32_t>("r_display_height");
    s.fullscreen = cfg.as<bool>("r_fullscreen");
    s.vsync = cfg.as<bool>("r_vsync");

    return s;
}

/** Write video mode settings to supplied Config. */
void write_display_settings(Config& cfg, const WindowSettings& s)
{
    cfg.set_value("r_display_width", s.video_mode.width);
    cfg.set_value("r_display_height", s.video_mode.height);
    cfg.set_value("r_vsync", s.vsync);
    cfg.set_value("r_fullscreen", s.fullscreen);
}

} // namespace Mg
