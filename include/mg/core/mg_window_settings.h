//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_window_settings.h
 * Video mode settings for use with Mg::Window and associated config helpers.
 */

#pragma once

#include <cstdint>

namespace Mg {

class Config;

struct VideoMode {
    // Values of zero mean 'use default value'.
    int32_t width = 0;
    int32_t height = 0;
};

struct WindowSettings {
    VideoMode video_mode = {};
    bool fullscreen = {};
    bool vsync = {};
};

/** Read video mode settings from supplied Config. */
WindowSettings read_display_settings(Config& cfg);

/** Write video mode settings to supplied Config. */
void write_display_settings(Config& cfg, const WindowSettings& s);

} // namespace Mg
