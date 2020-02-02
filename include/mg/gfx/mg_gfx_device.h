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

/** @file mg_gfx_device.h
 * Access to the graphics API's context.
 */

#pragma once

#include "mg/gfx/mg_blend_modes.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

#include <memory>

namespace Mg {
class Window;
class ShaderResource;
} // namespace Mg

/** Renderer and graphics utilities. */
namespace Mg::gfx {

class TextureRepository;
class MeshRepository;
class MaterialRepository;

/** Types of comparison functions to use in depth testing. */
enum class DepthFunc {
    NONE = 0,
    LESS = 0x201,
    EQUAL = 0x202,
    LEQUAL = 0x203,
    GREATER = 0x204,
    NOTEQUAL = 0x205,
    GEQUAL = 0x206,
};

/** Types of functions to use in culling. */
enum class CullFunc { NONE = 0, FRONT = 0x404, BACK = 0x405 };

struct GfxDeviceData;

/** Provides access to the graphics context.
 * N.B. only one GfxDevice object exist at a time.
 */
class GfxDevice : PImplMixin<GfxDeviceData> {
public:
    explicit GfxDevice(Window& window);
    ~GfxDevice();

    MG_MAKE_NON_MOVABLE(GfxDevice);
    MG_MAKE_NON_COPYABLE(GfxDevice);

    /** Set depth testing function. DepthFunc::NONE disables depth testing.  */
    void set_depth_test(DepthFunc func) noexcept;

    /** Set whether to write depth when drawing to render target. */
    void set_depth_write(bool on) noexcept;

    /** Set whether to write colour when drawing to render target. */
    void set_colour_write(bool on) noexcept;

    /** Set colour & alpha to use when clearing render target. */
    void set_clear_colour(float red, float green, float blue, float alpha = 1.0f) noexcept;

    /** Clear the currently bound render target. */
    void clear(bool colour = true, bool depth = true, bool stencil = true) noexcept;

    /** Set which culling function to use. */
    void set_culling(CullFunc culling) noexcept;

    /** Set whether to use blending when rendering to target. */
    void set_use_blending(bool enable) noexcept;

    /** Sets the blend mode to use during next frame.
     * Blending must be enabled through set_use_blending().
     * @param blend_mode Which blend mode to use. Some pre-defined ones are Mg::c_blend_mode_xxxxx
     */
    void set_blend_mode(BlendMode blend_mode) noexcept;

    /** Synchronise application with graphics device. */
    void synchronise() noexcept;

    MeshRepository& mesh_repository() noexcept;
    TextureRepository& texture_repository() noexcept;
    MaterialRepository& material_repository() noexcept;
};

} // namespace Mg::gfx
