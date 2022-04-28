//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "../mg_shader.h"

#include "mg_glad.h"

#include "mg/core/mg_log.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_gsl.h"

#include <fmt/core.h>

namespace Mg::gfx {

namespace {

template<GLenum shader_stage> Opt<GLuint> compile_shader(const std::string& code)
{
    const auto code_c_str = code.c_str();

    // Upload and compile shader.
    const auto id = glCreateShader(shader_stage);
    glShaderSource(id, 1, &code_c_str, nullptr);
    glCompileShader(id);

    // Check shader for compilation errors.
    int32_t result = GL_FALSE;
    int32_t log_length;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &log_length);

    std::string msg;

    // If there was a message, write to log.
    if (log_length > 1) {
        msg.resize(as<size_t>(log_length));
        glGetShaderInfoLog(id, log_length, nullptr, msg.data());

        const auto msg_type = result != 0 ? Log::Prio::Message : Log::Prio::Error;
        log.write(msg_type, fmt::format("Shader compilation message: {}", msg));
    }

    // Check whether shader compiled successfully.
    if (result == GL_FALSE) {
        glDeleteShader(id);
        return nullopt;
    }

    return id;
}

} // namespace

Opt<VertexShaderHandle> compile_vertex_shader(const std::string& code)
{
    auto wrap_in_handle = [](GLuint shader_id) { return VertexShaderHandle{ shader_id }; };
    return compile_shader<GL_VERTEX_SHADER>(code).map(wrap_in_handle);
}

Opt<GeometryShaderHandle> compile_geometry_shader(const std::string& code)
{
    auto wrap_in_handle = [](GLuint shader_id) { return GeometryShaderHandle{ shader_id }; };
    return compile_shader<GL_GEOMETRY_SHADER>(code).map(wrap_in_handle);
}

Opt<FragmentShaderHandle> compile_fragment_shader(const std::string& code)
{
    auto wrap_in_handle = [](GLuint shader_id) { return FragmentShaderHandle{ shader_id }; };
    return compile_shader<GL_FRAGMENT_SHADER>(code).map(wrap_in_handle);
}

void destroy_shader(VertexShaderHandle handle) noexcept
{
    glDeleteShader(handle.as_gl_id());
}
void destroy_shader(FragmentShaderHandle handle) noexcept
{
    glDeleteShader(handle.as_gl_id());
}
void destroy_shader(GeometryShaderHandle handle) noexcept
{
    glDeleteShader(handle.as_gl_id());
}

} // namespace Mg::gfx
