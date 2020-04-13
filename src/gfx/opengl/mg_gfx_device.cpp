//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_gfx_device.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/core/mg_window.h"

#include <GLFW/glfw3.h>
#include <fmt/core.h>

#include <cstdint>

namespace Mg::gfx {

#if MG_ENABLE_DEBUG_LOGGING
namespace {

/** Wrapper for calling convention (GLAPIENTRY) */
static void APIENTRY ogl_error_callback_wrapper(uint32_t source,
                                                uint32_t type,
                                                uint32_t id,
                                                uint32_t severity,
                                                int32_t length,
                                                const GLchar* msg,
                                                const void* user_param)
{
    ogl_error_callback(source, type, id, severity, length, msg, user_param);
}

} // namespace
#endif

static GfxDevice* p_gfx_device = nullptr;

//--------------------------------------------------------------------------------------------------

GfxDevice::GfxDevice(Window& window)
{
    MG_GFX_DEBUG_GROUP("Creating GfxDevice")

    if (p_gfx_device != nullptr) {
        g_log.write_error("Only one Mg::gfx::GfxDevice may be constructed at a time.");
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
    if (const uint32_t error = glGetError(); error) {
        g_log.write_error(fmt::format("OpenGL initialisation: {}", gfx::gl_error_string(error)));
        throw RuntimeError();
    }

#if MG_ENABLE_DEBUG_LOGGING
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

GfxDevice::~GfxDevice()
{
    p_gfx_device = nullptr;
}

void GfxDevice::set_blend_mode(BlendMode blend_mode) noexcept
{
    MG_GFX_DEBUG_GROUP("GfxDevice::set_blend_mode")

    const auto col_mode = static_cast<uint32_t>(blend_mode.colour);
    const auto a_mode = static_cast<uint32_t>(blend_mode.alpha);
    const auto src_col = static_cast<uint32_t>(blend_mode.src_colour);
    const auto dst_col = static_cast<uint32_t>(blend_mode.dst_colour);
    const auto srcA = static_cast<uint32_t>(blend_mode.src_alpha);
    const auto dstA = static_cast<uint32_t>(blend_mode.dst_alpha);

    glBlendEquationSeparate(col_mode, a_mode);
    glBlendFuncSeparate(src_col, dst_col, srcA, dstA);
}

/** Enable/disable depth testing and set depth testing function. */
void GfxDevice::set_depth_test(DepthFunc func) noexcept
{
    MG_GFX_DEBUG_GROUP("GfxDevice::set_depth_test")
    if (func != DepthFunc::NONE) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(static_cast<uint32_t>(func));
    }
    else {
        glDisable(GL_DEPTH_TEST);
    }
}

void GfxDevice::set_depth_write(bool on) noexcept
{
    MG_GFX_DEBUG_GROUP("GfxDevice::set_depth_write")
    glDepthMask(GLboolean{ on });
}

void GfxDevice::set_colour_write(bool on) noexcept
{
    MG_GFX_DEBUG_GROUP("GfxDevice::set_colour_write")
    glColorMask(on, on, on, on);
}

/** Set colour & alpha to use when clearing render target. */
void GfxDevice::set_clear_colour(float red, float green, float blue, float alpha) noexcept
{
    MG_GFX_DEBUG_GROUP("GfxDevice::set_clear_colour")
    glClearColor(red, green, blue, alpha);
}

void GfxDevice::clear(bool colour, bool depth, bool stencil) noexcept
{
    MG_GFX_DEBUG_GROUP("GfxDevice::clear")
    glClear((colour ? GL_COLOR_BUFFER_BIT : 0u) | (depth ? GL_DEPTH_BUFFER_BIT : 0u) |
            (stencil ? GL_STENCIL_BUFFER_BIT : 0u));
}

/** Set which culling function to use. */
void GfxDevice::set_culling(CullFunc culling) noexcept
{
    MG_GFX_DEBUG_GROUP("GfxDevice::set_culling")
    if (culling == CullFunc::NONE) {
        glDisable(GL_CULL_FACE);
    }
    else {
        glEnable(GL_CULL_FACE);
        glCullFace(static_cast<uint32_t>(culling));
    }
}

/** Set whether to use blending when rendering to target. */
void GfxDevice::set_use_blending(bool enable) noexcept
{
    MG_GFX_DEBUG_GROUP("GfxDevice::set_use_blending")
    if (enable) {
        glEnable(GL_BLEND);
    }
    else {
        glDisable(GL_BLEND);
    }
}

/** Synchronise application with graphics device. */
void GfxDevice::synchronise() noexcept
{
    MG_GFX_DEBUG_GROUP("GfxDevice::synchronise")
    // N.B. I tried using fences with glClientWaitSync as I hear that is a better approach (for
    // unclear reasons) but it had nowhere near the same impact on reducing input lag as glFinish.
    glFinish();
}

} // namespace Mg::gfx

