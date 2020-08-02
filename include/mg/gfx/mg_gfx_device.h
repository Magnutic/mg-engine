//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_gfx_device.h
 * Access to the graphics API's context.
 */

#pragma once

#include "mg/utils/mg_macros.h"

namespace Mg {
class Window;
}

/** Renderer and graphics utilities. */
namespace Mg::gfx {

/** Provides access to the graphics context.
 * N.B. only one GfxDevice object exist at a time.
 */
class GfxDevice {
public:
    explicit GfxDevice(Window& window);
    ~GfxDevice();

    MG_MAKE_NON_MOVABLE(GfxDevice);
    MG_MAKE_NON_COPYABLE(GfxDevice);

    /** Set colour & alpha to use when clearing render target. */
    void set_clear_colour(float red, float green, float blue, float alpha) noexcept;

    /** Clear the currently bound render target. */
    void clear(bool colour = true, bool depth = true) noexcept;

    /** Synchronise application with graphics device. */
    void synchronise() noexcept;
};

} // namespace Mg::gfx
