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

#include <cstdlib>
#include <exception>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI // wingdi.h defines OPAQUE and TRANSPARENT as macros, causing conflicts.
#define NOMINMAX
#include <Windows.h>
#undef NOGDI
#undef WIN32_LEAN_AND_MEAN
#undef NOMINMAX
#endif

#define GLFW_INCLUDE_NONE // Do not let GLFW include OpenGL headers.
#include <GLFW/glfw3.h>
#undef GLFW_INCLUDE_NONE

#include "mg/core/mg_config.h"
#include "mg/core/mg_log.h"
#include "mg/core/mg_window.h"
#include "mg/gfx/mg_gfx_device.h"
#include "mg/mg_defs.h"

namespace Mg {

Root* g_root = nullptr;

#ifdef _WIN32
// Old code page to return to on application exit
static uint32_t s_old_codepage;
#endif

Root::Root()
{
    if (g_root != nullptr) {
        throw std::logic_error{ "Attempting to construct multiple instances of Mg::Root." };
    }

    g_root = this;

#ifdef _WIN32
    // Allow UTF-8 output to Windows console
    s_old_codepage = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
#endif

    g_log.write_message("Mg Engine initialising...");

    // Set up engine config
    m_config = std::make_unique<Config>(defs::k_default_config_file_name);

    // Create window
    {
        auto window = Window::make(WindowSettings{}, "Mg Engine");
        if (!window) { throw std::runtime_error("Failed to open window."); }
        m_window = std::move(window.value());
    }

    // Create render context
    m_render_context = std::make_unique<gfx::GfxDevice>(*m_window);
}

Root::~Root()
{
    g_log.write_message("Mg Engine exiting...");
    m_config->write_to_file(defs::k_default_config_file_name);

#ifdef _WIN32
    // Return to code page used before the application started.
    SetConsoleOutputCP(s_old_codepage);
#endif

    g_root = nullptr;
}

double Root::time_since_init()
{
    return glfwGetTime();
}

Root& Root::instance()
{
    if (g_root == nullptr) {
        throw std::logic_error{ "Mg::Root::instance() called outside Mg::Root lifetime." };
    }

    return *g_root;
}

}; // namespace Mg
