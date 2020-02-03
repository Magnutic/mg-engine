//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_shader.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

#include "mg/core/mg_log.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_gsl.h"

#include <fmt/core.h>

namespace Mg::gfx {

namespace {

GLenum shader_stage_to_gl_enum(ShaderStage stage) noexcept
{
    switch (stage) {
    case ShaderStage::Vertex:
        return GL_VERTEX_SHADER;
    case ShaderStage::Fragment:
        return GL_FRAGMENT_SHADER;
    case ShaderStage::Geometry:
        return GL_GEOMETRY_SHADER;
    }

    MG_ASSERT(false && "unreachable");
}

template<ShaderStage stage> Opt<TypedShaderHandle<stage>> compile_shader(const std::string& code)
{
    const auto gl_shader_type = shader_stage_to_gl_enum(stage);
    const auto code_c_str = code.c_str();

    // Upload and compile shader.
    const auto id = glCreateShader(gl_shader_type);
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
        msg.resize(narrow<size_t>(log_length));
        glGetShaderInfoLog(id, log_length, nullptr, msg.data());

        const auto msg_type = result != 0 ? Log::Prio::Message : Log::Prio::Error;
        g_log.write(msg_type, fmt::format("Shader compilation message: {}", msg));
    }

    // Check whether shader compiled successfully.
    if (result == GL_FALSE) {
        glDeleteShader(id);
        return nullopt;
    }

    return TypedShaderHandle<stage>{ ShaderId{ id } };
}

} // namespace

Opt<VertexShaderHandle> compile_vertex_shader(const std::string& code)
{
    return compile_shader<ShaderStage::Vertex>(code);
}

Opt<GeometryShaderHandle> compile_geometry_shader(const std::string& code)
{
    return compile_shader<ShaderStage::Geometry>(code);
}

Opt<FragmentShaderHandle> compile_fragment_shader(const std::string& code)
{
    return compile_shader<ShaderStage::Fragment>(code);
}

void destroy_shader(ShaderId handle) noexcept
{
    glDeleteShader(narrow<GLuint>(handle.value));
}

//--------------------------------------------------------------------------------------------------
// Helpers for ShaderProgram implementation
//--------------------------------------------------------------------------------------------------

namespace {

// Links the given shader program, returning whether linking was successful.
bool link_program(uint32_t program_id)
{
    glLinkProgram(program_id);

    // Check the program for linking errors.
    int32_t result = GL_FALSE, log_length;
    glGetProgramiv(program_id, GL_LINK_STATUS, &result);
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

    std::string msg;

    // If there was log message, write to log.
    if (log_length > 1) {
        msg.resize(narrow<size_t>(log_length));
        glGetProgramInfoLog(program_id, log_length, nullptr, msg.data());

        const auto msg_type = result != 0 ? Log::Prio::Message : Log::Prio::Error;
        g_log.write(msg_type, fmt::format("Shader linking message: {}", msg));
    }

    // Check whether shaders linked successfully.
    if (result == GL_FALSE) {
        glDeleteProgram(program_id);
        return false;
    }

    return true;
}

/** RAII guard for attaching shader object to shader program. */
class ShaderAttachGuard {
public:
    ShaderAttachGuard(GLuint program, Opt<ShaderId> handle) noexcept
        : _program(program), _shader(handle)
    {
        _shader.map([&](ShaderId id) { glAttachShader(_program, narrow<GLuint>(id.value)); });
    }

    MG_MAKE_NON_COPYABLE(ShaderAttachGuard);
    MG_MAKE_NON_MOVABLE(ShaderAttachGuard);

    ~ShaderAttachGuard()
    {
        _shader.map([&](ShaderId id) { glDetachShader(_program, narrow<GLuint>(id.value)); });
    }

    GLuint _program{};
    Opt<ShaderId> _shader{};
};

} // namespace

//--------------------------------------------------------------------------------------------------
// ShaderProgram implementation
//--------------------------------------------------------------------------------------------------

Opt<ShaderHandle> link_shader_program(VertexShaderHandle vertex_shader,
                                      Opt<GeometryShaderHandle> geometry_shader,
                                      Opt<FragmentShaderHandle> fragment_shader)
{
    const GLuint program_id = glCreateProgram();
    ShaderAttachGuard guard_vs(program_id, vertex_shader);
    ShaderAttachGuard guard_gs(program_id, geometry_shader);
    ShaderAttachGuard guard_fs(program_id, fragment_shader);

    if (link_program(program_id)) {
        return ShaderHandle(program_id);
    }

    return nullopt;
}

void destroy_shader_program(ShaderHandle handle) noexcept
{
    glDeleteProgram(static_cast<GLuint>(handle));
}

} // namespace Mg::gfx
