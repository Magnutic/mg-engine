//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
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
    int32_t width  = 0;
    int32_t height = 0;
};

struct WindowSettings {
    VideoMode video_mode = {};
    bool      fullscreen = {};
    bool      vsync      = {};
};

/** Read video mode settings from supplied Config. */
WindowSettings read_display_settings(Config& cfg);

/** Write video mode settings to supplied Config. */
inline void write_display_settings(Config& cfg, WindowSettings& s);

} // namespace Mg
