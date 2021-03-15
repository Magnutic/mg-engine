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
#include "mg/gfx/mg_gfx_debug_group.h"

#include <GLFW/glfw3.h>
#include <fmt/core.h>

#include <cstdint>

namespace Mg::gfx {

#if MG_ENABLE_DEBUG_LOGGING
namespace {

/** Wrapper for calling convention (GLAPIENTRY) */
void APIENTRY ogl_error_callback_wrapper(uint32_t source,
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

namespace {
GfxDevice* p_gfx_device = nullptr;
}

//--------------------------------------------------------------------------------------------------

GfxDevice::GfxDevice(Window& window)
{
    if (p_gfx_device != nullptr) {
        log.error("Only one Mg::gfx::GfxDevice may be constructed at a time.");
        throw RuntimeError();
    }

    p_gfx_device = this;

    // Use the context provided by this GLFW window
    glfwMakeContextCurrent(window.glfw_window());

    // Init GLAD
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0) { // NOLINT
        log.error("Failed to initialise GLAD.");
        throw RuntimeError();
    }

    // Check for errors.
    if (const uint32_t error = glGetError(); error) {
        log.error("OpenGL initialisation: {}", gfx::gl_error_string(error));
        throw RuntimeError();
    }

#if MG_ENABLE_DEBUG_LOGGING
    // Add detailed OpenGL debug messaging in debug builds
    if (GLAD_GL_KHR_debug != 0) {
        glDebugMessageCallback(ogl_error_callback_wrapper, nullptr);

        GLint context_flags;
        glGetIntegerv(GL_CONTEXT_FLAGS, &context_flags);

        if ((context_flags & GL_CONTEXT_FLAG_DEBUG_BIT) != 0) {
            log.message("OpenGL debug context enabled.");
        }
    }
#endif

    // Automatically convert linear to sRGB when writing to sRGB frame buffers.
    glEnable(GL_FRAMEBUFFER_SRGB);
}

GfxDevice::~GfxDevice()
{
    p_gfx_device = nullptr;
}

/** Set colour & alpha to use when clearing render target. */
void GfxDevice::set_clear_colour(float red, float green, float blue, float alpha) noexcept
{
    MG_GFX_DEBUG_GROUP("GfxDevice::set_clear_colour")
    glClearColor(red, green, blue, alpha);
}

void GfxDevice::clear(bool colour, bool depth) noexcept
{
    MG_GFX_DEBUG_GROUP("GfxDevice::clear")

    // Change depth/colour write state if needed.
    std::array<GLboolean, 4> prev_colour_write = {};
    glGetBooleanv(GL_COLOR_WRITEMASK, prev_colour_write.data());
    const bool should_set_colour_write = colour &&
                                         std::any_of(prev_colour_write.begin(),
                                                     prev_colour_write.end(),
                                                     [](GLboolean b) { return b == GL_FALSE; });

    GLboolean prev_depth_write = GL_FALSE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &prev_depth_write);
    const bool should_set_depth_write = depth && prev_depth_write == GL_FALSE;

    if (should_set_colour_write) {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
    if (should_set_depth_write) {
        glDepthMask(GL_TRUE);
    }

    glClear((colour ? GL_COLOR_BUFFER_BIT : 0u) | (depth ? GL_DEPTH_BUFFER_BIT : 0u));

    // Revert depth/colour write state if it was changed.
    if (should_set_colour_write) {
        glColorMask(prev_colour_write[0],
                    prev_colour_write[1],
                    prev_colour_write[2],
                    prev_colour_write[3]);
    }
    if (should_set_depth_write) {
        glDepthMask(prev_depth_write);
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
