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

#include "mg_gl_gfx_device.h"

#include "mg_gl_debug.h"
#include "mg_texture_node.h"
#include "mg_glad.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/core/mg_window.h"
#include "mg/gfx/mg_buffer_texture.h"
#include "mg/gfx/mg_material_repository.h"
#include "mg/gfx/mg_mesh_repository.h"
#include "mg/gfx/mg_texture_repository.h"

#include <GLFW/glfw3.h>
#include <fmt/core.h>

#include <cstdint>

namespace Mg::gfx::opengl {

#ifndef NDEBUG
/** Wrapper for calling convention (GLAPIENTRY) */
static void APIENTRY ogl_error_callback_wrapper(uint32_t      source,
                                                uint32_t      type,
                                                uint32_t      id,
                                                uint32_t      severity,
                                                int32_t       length,
                                                const GLchar* msg,
                                                const void*   user_param)
{
    ogl_error_callback(source, type, id, severity, length, msg, user_param);
}
#endif

static OpenGLGfxDevice* p_gfx_device = nullptr;

struct OpenGLGfxDeviceData {
    MeshRepository     mesh_repository;
    TextureRepository  texture_repository;
    MaterialRepository material_repository;
};

//--------------------------------------------------------------------------------------------------

OpenGLGfxDevice::OpenGLGfxDevice(::Mg::Window& window)
{
    if (p_gfx_device != nullptr) {
        g_log.write_error("Only one Mg::gfx::OpenGLGfxDevice may be constructed at a time.");
        throw RuntimeError();
    }

    p_gfx_device = this;

    // Use the context provided by this GLFW window
    glfwMakeContextCurrent(window.glfw_window());

    // Init GLAD
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0) { // NOLINT
        g_log.write_error("Failed to initialise GLAD.");
        throw RuntimeError();
    }

    // Check for errors.
    if (uint32_t error = glGetError(); error) {
        g_log.write_error(fmt::format("OpenGL initialisation: {}", gfx::gl_error_string(error)));
        throw RuntimeError();
    }

#ifndef NDEBUG
    // Add detailed OpenGL debug messaging in debug builds
    if (GLAD_GL_KHR_debug != 0) {
        glDebugMessageCallback(ogl_error_callback_wrapper, nullptr);

        GLint context_flags;
        glGetIntegerv(GL_CONTEXT_FLAGS, &context_flags);

        if ((context_flags & GL_CONTEXT_FLAG_DEBUG_BIT) != 0) {
            g_log.write_message("OpenGL debug context enabled.");
        }
    }
#endif

    // Automatically convert linear to sRGB when writing to sRGB frame buffers.
    glEnable(GL_FRAMEBUFFER_SRGB);

    set_culling(CullFunc::BACK);
    set_depth_test(DepthFunc::LESS);
}

OpenGLGfxDevice& OpenGLGfxDevice::get()
{
    if (p_gfx_device == nullptr) {
        g_log.write_error("Attempting to access OpenGLGfxDevice outside of its lifetime.");
        throw RuntimeError();
    }
    return *p_gfx_device;
}

OpenGLGfxDevice::~OpenGLGfxDevice()
{
    p_gfx_device = nullptr;
}

void OpenGLGfxDevice::set_blend_mode(BlendMode blend_mode)
{
    auto col_mode = uint32_t(blend_mode.colour);
    auto a_mode   = uint32_t(blend_mode.alpha);
    auto src_col  = uint32_t(blend_mode.src_colour);
    auto dst_col  = uint32_t(blend_mode.dst_colour);
    auto srcA     = uint32_t(blend_mode.src_alpha);
    auto dstA     = uint32_t(blend_mode.dst_alpha);

    glBlendEquationSeparate(col_mode, a_mode);
    glBlendFuncSeparate(src_col, dst_col, srcA, dstA);
}

/** Enable/disable depth testing and set depth testing function. */
void OpenGLGfxDevice::set_depth_test(DepthFunc func)
{
    if (func != DepthFunc::NONE) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(uint32_t(func));
    }
    else {
        glDisable(GL_DEPTH_TEST);
    }
}

void OpenGLGfxDevice::set_depth_write(bool on)
{
    glDepthMask(GLboolean(on));
}

void OpenGLGfxDevice::set_colour_write(bool on)
{
    auto gb_on = GLboolean(on);
    glColorMask(gb_on, gb_on, gb_on, gb_on);
}

/** Set colour & alpha to use when clearing render target. */
void OpenGLGfxDevice::set_clear_colour(float red, float green, float blue, float alpha)
{
    glClearColor(red, green, blue, alpha);
}

void OpenGLGfxDevice::clear(bool colour, bool depth, bool stencil)
{
    glClear((colour ? GL_COLOR_BUFFER_BIT : 0u) | (depth ? GL_DEPTH_BUFFER_BIT : 0u) |
            (stencil ? GL_STENCIL_BUFFER_BIT : 0u));
}

/** Set which culling function to use. */
void OpenGLGfxDevice::set_culling(CullFunc culling)
{
    if (culling == CullFunc::NONE) { glDisable(GL_CULL_FACE); }
    else {
        glEnable(GL_CULL_FACE);
        glCullFace(static_cast<uint32_t>(culling));
    }
}

/** Set whether to use blending when rendering to target. */
void OpenGLGfxDevice::set_use_blending(bool enable)
{
    if (enable) { glEnable(GL_BLEND); }
    else {
        glDisable(GL_BLEND);
    }
}

MeshRepository& OpenGLGfxDevice::mesh_repository()
{
    return data().mesh_repository;
}

TextureRepository& OpenGLGfxDevice::texture_repository()
{
    return data().texture_repository;
}

MaterialRepository& OpenGLGfxDevice::material_repository()
{
    return data().material_repository;
}

void OpenGLGfxDevice::bind_texture(TextureUnit unit, TextureHandle texture)
{
    auto handle        = internal::texture_node(texture).texture.gfx_api_handle();
    auto gl_texture_id = static_cast<GLuint>(handle);

    glActiveTexture(GL_TEXTURE0 + unit.get());
    glBindTexture(GL_TEXTURE_2D, gl_texture_id);
}

void OpenGLGfxDevice::bind_buffer_texture(TextureUnit unit, const BufferTexture& texture)
{
    auto gl_texture_id = static_cast<GLuint>(texture.internal_texture_id());

    glActiveTexture(GL_TEXTURE0 + unit.get());
    glBindTexture(GL_TEXTURE_BUFFER, gl_texture_id);
}

void OpenGLGfxDevice::bind_uniform_buffer(UniformBufferSlot slot, const UniformBuffer& buffer)
{
    auto gl_slot = static_cast<uint32_t>(slot);
    glBindBufferBase(GL_UNIFORM_BUFFER, gl_slot, buffer.internal_id());
}

} // namespace Mg::gfx::opengl

namespace Mg::gfx {

std::unique_ptr<GfxDevice> make_opengl_gfx_device(Mg::Window& window)
{
    return std::make_unique<opengl::OpenGLGfxDevice>(window);
}

} // namespace Mg::gfx
