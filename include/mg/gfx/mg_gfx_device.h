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
    NONE     = 0,
    LESS     = 0x201,
    EQUAL    = 0x202,
    LEQUAL   = 0x203,
    GREATER  = 0x204,
    NOTEQUAL = 0x205,
    GEQUAL   = 0x206,
};

/** Types of functions to use in culling. */
enum class CullFunc { NONE = 0, FRONT = 0x404, BACK = 0x405 };

/** Provides access to the graphics context.
 * N.B. only one GfxDevice object exist at a time.
 */
class GfxDevice {
public:
    MG_INTERFACE_BOILERPLATE(GfxDevice);

    /** Set depth testing function. DepthFunc::NONE disables depth testing.  */
    virtual void set_depth_test(DepthFunc func) = 0;

    /** Set whether to write depth when drawing to render target. */
    virtual void set_depth_write(bool on) = 0;

    /** Set whether to write colour when drawing to render target. */
    virtual void set_colour_write(bool on) = 0;

    /** Set colour & alpha to use when clearing render target. */
    virtual void set_clear_colour(float red, float green, float blue, float alpha = 1.0f) = 0;

    /** Clear the currently bound render target. */
    virtual void clear(bool colour = true, bool depth = true, bool stencil = true) = 0;

    /** Set which culling function to use. */
    virtual void set_culling(CullFunc culling) = 0;

    /** Set whether to use blending when rendering to target. */
    virtual void set_use_blending(bool enable) = 0;

    /** Sets the blend mode to use during next frame.
     * Blending must be enabled through set_use_blending().
     * @param blend_mode Which blend mode to use. Some pre-defined ones are Mg::c_blend_mode_xxxxx
     */
    virtual void set_blend_mode(BlendMode blend_mode) = 0;

    virtual MeshRepository&     mesh_repository()     = 0;
    virtual TextureRepository&  texture_repository()  = 0;
    virtual MaterialRepository& material_repository() = 0;
};

std::unique_ptr<GfxDevice> make_opengl_gfx_device(Mg::Window& window);

} // namespace Mg::gfx
