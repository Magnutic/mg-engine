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

#include "mg/core/mg_root.h"

#include "mg/core/mg_config.h"
#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/core/mg_window.h"
#include "mg/gfx/mg_gfx_device.h"
#include "mg/mg_defs.h"
#include "mg/utils/mg_pointer.h"

#include <chrono>
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI // wingdi.h defines OPAQUE and TRANSPARENT as macros, causing conflicts.
#define NOMINMAX
#include <Windows.h>
#undef NOGDI
#undef WIN32_LEAN_AND_MEAN
#undef NOMINMAX
#endif

namespace Mg {

struct RootData {
    std::chrono::high_resolution_clock::time_point start_time;

    Ptr<Config>         config;
    Ptr<Window>         window;
    Ptr<gfx::GfxDevice> gfx_device;
};

#ifdef _WIN32
// Old code page to return to on application exit
static uint32_t s_old_codepage;
#endif

Root::Root()
{
    data().start_time = std::chrono::high_resolution_clock::now();

#ifdef _WIN32
    // Allow UTF-8 output to Windows console
    s_old_codepage = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
#endif

    g_log.write_message("Mg Engine initialising...");

    // Set up engine config
    data().config = Ptr<Config>::make(defs::k_default_config_file_name);

    // Create window
    {
        data().window = Window::make(WindowSettings{}, "Mg Engine");
        if (data().window == nullptr) {
            g_log.write_error("Failed to open window.");
            throw RuntimeError();
        }
    }

    // Create render context
    data().gfx_device = Ptr<gfx::GfxDevice>::make(*data().window);
}

Root::~Root()
{
    g_log.write_message("Mg Engine exiting...");
    data().config->write_to_file(defs::k_default_config_file_name);

#ifdef _WIN32
    // Return to code page used before the application started.
    SetConsoleOutputCP(s_old_codepage);
#endif
}

double Root::time_since_init()
{
    using namespace std::chrono;
    using seconds_double = duration<double, seconds::period>;
    return seconds_double(high_resolution_clock::now() - data().start_time).count();
}

Config& Root::config()
{
    return *data().config;
}

Window& Root::window()
{
    return *data().window;
}

gfx::GfxDevice& Root::gfx_device()
{
    return *data().gfx_device;
}

}; // namespace Mg
