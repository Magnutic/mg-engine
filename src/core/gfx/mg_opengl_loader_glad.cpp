//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_opengl_loader_glad.h"

#include "mg/core/mg_runtime_error.h"
#include "mg/core/mg_window.h"
#include "mg_gl_debug.h"

#include <GLFW/glfw3.h>

#include <cstdint>

namespace Mg::gfx {

#if MG_ENABLE_DEBUG_LOGGING

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

#endif

void init_opengl_context(Window& window)
{
    // Use the context provided by this GLFW window
    glfwMakeContextCurrent(window.glfw_window());

    // Init GLAD
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0) { // NOLINT
        throw RuntimeError{ "Failed to initialize GLAD." };
    }

    // Check for errors.
    if (const uint32_t error = glGetError(); error) {
        throw RuntimeError{ "OpenGL initialization: {}", gl_error_string(error) };
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

    // Seamless texture filtering between cubemap faces.
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

} // namespace Mg
