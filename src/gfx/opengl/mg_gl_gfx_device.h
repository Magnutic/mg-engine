//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Magnus Bergsten
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

/** @file mg_gl_gfx_device.h
 * OpenGL-specific implementation of and extensions to Mg::gfx::GfxDevice.
 */

#pragma once

#include "mg/gfx/mg_gfx_device.h"
#include "mg/gfx/mg_texture_handle.h"
#include "mg/gfx/mg_texture_related_types.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/utils/mg_simple_pimpl.h"

namespace Mg::gfx {
class BufferTexture;
}

namespace Mg::gfx::opengl {

struct OpenGLGfxDeviceData;

class OpenGLGfxDevice final : public GfxDevice, PimplMixin<OpenGLGfxDeviceData> {
public:
    explicit OpenGLGfxDevice(Mg::Window& window);
    ~OpenGLGfxDevice();

    static OpenGLGfxDevice& get();

    MG_MAKE_NON_MOVABLE(OpenGLGfxDevice);
    MG_MAKE_NON_COPYABLE(OpenGLGfxDevice);

    /** Set depth testing function. DepthFunc::NONE disables depth testing.  */
    void set_depth_test(DepthFunc func) override;

    /** Set whether to write depth when drawing to render target. */
    void set_depth_write(bool on) override;

    /** Set whether to write colour when drawing to render target. */
    void set_colour_write(bool on) override;

    /** Set colour & alpha to use when clearing render target. */
    void set_clear_colour(float red, float green, float blue, float alpha = 1.0f) override;

    /** Clear the currently bound render target. */
    void clear(bool colour = true, bool depth = true, bool stencil = true) override;

    /** Set which culling function to use. */
    void set_culling(CullFunc culling) override;

    /** Set whether to use blending when rendering to target. */
    void set_use_blending(bool enable) override;

    /** Sets the blend mode to use during next frame.
     * Blending must be enabled through set_use_blending().
     * @param blend_mode Which blend mode to use. Some pre-defined ones are Mg::c_blend_mode_xxxxx
     */
    void set_blend_mode(BlendMode blend_mode) override;

    MeshRepository&     mesh_repository() override;
    TextureRepository&  texture_repository() override;
    MaterialRepository& material_repository() override;

    //----------------------------------------------------------------------------------------------
    // OpenGL-specific functionality
    //----------------------------------------------------------------------------------------------

    void bind_texture(TextureUnit unit, TextureHandle texture);

    void bind_buffer_texture(TextureUnit unit, const BufferTexture& texture);

    void bind_uniform_buffer(UniformBufferSlot slot, const UniformBuffer& buffer);
};

} // namespace Mg::gfx::opengl
