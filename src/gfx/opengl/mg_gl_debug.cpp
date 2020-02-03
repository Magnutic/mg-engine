//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_gl_debug.h"

#include <fmt/core.h>

#include "mg/core/mg_log.h"

#include "mg_glad.h"

namespace Mg::gfx {

const char* gl_error_string(uint32_t error_code) noexcept
{
    switch (error_code) {
    case 0:
        return "GL_NO_ERROR";
    case 0x0500:
        return "GL_INVALID_ENUM";
    case 0x0501:
        return "GL_INVALID_VALUE";
    case 0x0502:
        return "GL_INVALID_OPERATION";
    case 0x0506:
        return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case 0x0505:
        return "GL_OUT_OF_MEMORY";
    case 0x0504:
        return "GL_STACK_UNDERFLOW";
    case 0x0503:
        return "GL_STACK_OVERFLOW";
    default:
        return "Unknown error code";
    }
}

void check_gl_error(std::string_view file, std::string_view function, size_t line) noexcept
{
    uint32_t error_enum = glGetError();

    while (error_enum != GL_NO_ERROR) {
        constexpr char msg[] =
            "OpenGL error detected in file: {:s}, function: {:s}, line: {:d}: {:s}";
        g_log.write_error(fmt::format(msg, file, function, line, gl_error_string(error_enum)));

        error_enum = glGetError();
    }
}

//--------------------------------------------------------------------------------------------------
// KHR_debug extension utilities
//--------------------------------------------------------------------------------------------------

const char* source_string(uint32_t source) noexcept
{
    switch (source) {
    case GL_DEBUG_SOURCE_API:
        return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        return "WINDOW_SYSTEM";
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        return "SHADER_COMPILER";
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        return "THIRD_PARTY";
    case GL_DEBUG_SOURCE_APPLICATION:
        return "APPLICATION";
    case GL_DEBUG_SOURCE_OTHER:
        return "OTHER";
    default:
        return "UNKNOWN";
    }
}

const char* type_string(uint32_t type) noexcept
{
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        return "ERROR";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        return "DEPRECATED_BEHAVIOR";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        return "UNDEFINED_BEHAVIOR";
    case GL_DEBUG_TYPE_PORTABILITY:
        return "PORTABILITY";
    case GL_DEBUG_TYPE_PERFORMANCE:
        return "PERFORMANCE";
    case GL_DEBUG_TYPE_OTHER:
        return "OTHER";
    case GL_DEBUG_TYPE_MARKER:
        return "MARKER";
    default:
        return "UNKNOWN";
    }
}

const char* severity_string(uint32_t severity) noexcept
{
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        return "HIGH";
    case GL_DEBUG_SEVERITY_MEDIUM:
        return "MEDIUM";
    case GL_DEBUG_SEVERITY_LOW:
        return "LOW";
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        return "NOTIFICATION";
    default:
        return "UNKNOWN";
    }
}

void ogl_error_callback(uint32_t source,
                        uint32_t type,
                        uint32_t id,
                        uint32_t severity,
                        int32_t /* length */,
                        const char* msg,
                        const void* /* user_param */) noexcept
{
    auto src_str = source_string(source);
    auto type_str = type_string(type);
    auto severity_str = severity_string(severity);

    Log::Prio prio;

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        prio = Log::Prio::Error;
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        prio = Log::Prio::Warning;
        break;
    case GL_DEBUG_SEVERITY_LOW: // Fallthrough
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        prio = Log::Prio::Verbose;
        break;
    default:
        prio = Log::Prio::Error;
        break;
    }

    constexpr char msg_str[] =
        "OpenGL debug message: [source: {:s}] [type: {:s}] [severity: {:s}] [id: {:d}] {:s}";

    g_log.write(prio, fmt::format(msg_str, src_str, type_str, severity_str, id, msg));
}

} // namespace Mg::gfx
