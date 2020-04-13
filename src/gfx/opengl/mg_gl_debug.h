//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_gl_debug.h
 * OpenGL debugging utilities */

#pragma once

#include "mg_glad.h"

#include <mg/utils/mg_macros.h>

#include <cstddef>
#include <string_view>

/** Check for and notify on OpenGL errors. */
#define MG_CHECK_GL_ERROR()                                       \
    ::Mg::gfx::check_gl_error(static_cast<const char*>(__FILE__), \
                              static_cast<const char*>(__func__), \
                              __LINE__)

namespace Mg::gfx {

/** Get string stating error type for the given GL error code. */
const char* gl_error_string(uint32_t error_code) noexcept;

void check_gl_error(std::string_view file, std::string_view function, size_t line) noexcept;

//--------------------------------------------------------------------------------------------------
// KHR_debug extension utilities
//--------------------------------------------------------------------------------------------------

const char* source_string(uint32_t source) noexcept;

const char* type_string(uint32_t type) noexcept;

const char* severity_string(uint32_t severity) noexcept;

void ogl_error_callback(uint32_t source,
                        uint32_t type,
                        uint32_t id,
                        uint32_t severity,
                        int32_t /* length */,
                        const char* msg,
                        const void* /* user_param */
                        ) noexcept;


namespace detail {

class OpenGlDebugGroupGuard {
public:
    OpenGlDebugGroupGuard(const char* message)
    {
        if (GLAD_GL_KHR_debug != 0) {
            glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, message);
        }
    }

    ~OpenGlDebugGroupGuard()
    {
        if (GLAD_GL_KHR_debug != 0) {
            glPopDebugGroup();
        }
    }

    MG_MAKE_NON_MOVABLE(OpenGlDebugGroupGuard);
    MG_MAKE_NON_COPYABLE(OpenGlDebugGroupGuard);
};

#if MG_ENABLE_OPENGL_DEBUG_GROUPS
#    define MG_GFX_DEBUG_GROUP(message) \
        ::Mg::gfx::detail::OpenGlDebugGroupGuard gfxDebugGroup_(message);
#else
#    define MG_GFX_DEBUG_GROUP(message)
#endif

} // namespace detail

} // namespace Mg::gfx
