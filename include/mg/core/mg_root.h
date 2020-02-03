//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_root.h
 * Engine root, creates and owns all subsystems.
 */

#pragma once

#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

/** Mg Engine */
namespace Mg {

class Config;
class Window;
class ResourceCache;

namespace gfx {
class GfxDevice;
}

struct RootData;

/** Mg Engine root. Initialises and owns subsystems.
 * Only one instance of the engine root may exist at a time in an application
 * (due to global state such as config, log, and -- more fundamentally -- graphics API context).
 */
class Root : PImplMixin<RootData> {
public:
    Root();
    MG_MAKE_NON_COPYABLE(Root);
    MG_MAKE_NON_MOVABLE(Root);
    ~Root();

    /** Gets the high-precision time since the engine was started.
     * @return Time since engine start, in seconds.
     */
    double time_since_init() noexcept;

    Config& config();

    Window& window();

    gfx::GfxDevice& gfx_device();
};

} // namespace Mg
