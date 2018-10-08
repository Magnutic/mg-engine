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

/** @file mg_root.h
 * Engine root, creates and owns all subsystems.
 */

#pragma once

#include <memory>

#include <mg/utils/mg_macros.h>

/** Mg Engine */
namespace Mg {

class Config;
class Window;
class ResourceCache;

namespace gfx {
class GfxDevice;
}

/** Mg Engine root. Initialises and owns subsystems.
 * Only one instance of the engine root may exist at a time in an application
 * (due to global state such as config, log, and -- more fundamentally -- graphics API context).
 */
class Root {
public:
    Root();
    MG_MAKE_NON_COPYABLE(Root);
    MG_MAKE_NON_MOVABLE(Root);
    ~Root();

    /** Gets the high-precision time since the engine was started.
     * @return Time since engine start, in seconds.
     */
    static double time_since_init();

    static Config& config() { return *instance().m_config; }

    Window& window() { return *m_window; }

    gfx::GfxDevice& gfx_context() { return *m_render_context; }

private:
    static Root& instance();

    std::unique_ptr<Config>         m_config;
    std::unique_ptr<Window>         m_window;
    std::unique_ptr<gfx::GfxDevice> m_render_context;
};

} // namespace Mg
