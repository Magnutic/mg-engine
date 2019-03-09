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

#include "mg/gfx/mg_shader.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

#include "mg/core/mg_log.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_gsl.h"

#include <fmt/core.h>

namespace Mg::gfx {

static constexpr GLenum shader_stage_to_gl_enum(ShaderStage stage)
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

static constexpr size_t k_shader_program_pool_size = 64;

template<ShaderStage stage>
std::optional<TypedShaderHandle<stage>> compile_shader(const std::string& code)
{
    auto gl_shader_type = shader_stage_to_gl_enum(stage);
    auto code_c_str     = code.c_str();

    // Upload and compile shader.
    auto id = glCreateShader(gl_shader_type);
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

        auto msg_type = result != 0 ? Log::Prio::Message : Log::Prio::Error;
        g_log.write(msg_type, fmt::format("Shader compilation message: {}", msg));
    }

    // Check whether shader compiled successfully.
    if (result == GL_FALSE) {
        glDeleteShader(id);
        return std::nullopt;
    }

    return TypedShaderHandle<stage>{ ShaderId{ id } };
}

std::optional<VertexShaderHandle> compile_vertex_shader(const std::string& code)
{
    return compile_shader<ShaderStage::Vertex>(code);
}

std::optional<GeometryShaderHandle> compile_geometry_shader(const std::string& code)
{
    return compile_shader<ShaderStage::Geometry>(code);
}

std::optional<FragmentShaderHandle> compile_fragment_shader(const std::string& code)
{
    return compile_shader<ShaderStage::Fragment>(code);
}

void destroy_shader(ShaderId handle)
{
    glDeleteShader(static_cast<GLuint>(handle.value));
}

//--------------------------------------------------------------------------------------------------
// Helpers for ShaderProgram implementation
//--------------------------------------------------------------------------------------------------

// Links the given shader program, returning whether linking was successful.
static bool link_program(uint32_t program_id)
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

        auto msg_type = result != 0 ? Log::Prio::Message : Log::Prio::Error;
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
    ShaderAttachGuard(GLuint program, GLuint shader) : _program(program), _shader(shader)
    {
        glAttachShader(program, shader);
    }

    MG_MAKE_NON_COPYABLE(ShaderAttachGuard);
    MG_MAKE_NON_MOVABLE(ShaderAttachGuard);

    ~ShaderAttachGuard() { glDetachShader(_program, _shader); }

    GLuint _program{};
    GLuint _shader{};
};

//--------------------------------------------------------------------------------------------------
// ShaderProgram implementation
//--------------------------------------------------------------------------------------------------

std::optional<ShaderHandle> link_shader_program(VertexShaderHandle vertex_shader)
{
    GLuint            program_id = glCreateProgram();
    ShaderAttachGuard guard_vs(program_id, static_cast<GLuint>(vertex_shader.value));

    if (link_program(program_id)) { return ShaderHandle(program_id); }

    return std::nullopt;
}

std::optional<ShaderHandle> link_shader_program(VertexShaderHandle   vertex_shader,
                                                FragmentShaderHandle fragment_shader)
{
    GLuint            program_id = glCreateProgram();
    ShaderAttachGuard guard_vs(program_id, static_cast<GLuint>(vertex_shader.value));
    ShaderAttachGuard guard_fs(program_id, static_cast<GLuint>(fragment_shader.value));

    if (link_program(program_id)) { return ShaderHandle(program_id); }

    return std::nullopt;
}

std::optional<ShaderHandle> link_shader_program(VertexShaderHandle   vertex_shader,
                                                GeometryShaderHandle geometry_shader,
                                                FragmentShaderHandle fragment_shader)
{
    GLuint            program_id = glCreateProgram();
    ShaderAttachGuard guard_vs(program_id, static_cast<GLuint>(vertex_shader.value));
    ShaderAttachGuard guard_fs(program_id, static_cast<GLuint>(fragment_shader.value));
    ShaderAttachGuard guard_gs(program_id, static_cast<GLuint>(geometry_shader.value));

    if (link_program(program_id)) { return ShaderHandle(program_id); }

    return std::nullopt;
}

std::optional<ShaderHandle> link_shader_program(VertexShaderHandle   vertex_shader,
                                                GeometryShaderHandle geometry_shader)
{
    GLuint            program_id = glCreateProgram();
    ShaderAttachGuard guard_vs(program_id, static_cast<GLuint>(vertex_shader.value));
    ShaderAttachGuard guard_gs(program_id, static_cast<GLuint>(geometry_shader.value));

    if (link_program(program_id)) { return ShaderHandle(program_id); }

    return std::nullopt;
}

void destroy_shader_program(ShaderHandle handle)
{
    glDeleteProgram(static_cast<GLuint>(handle));
}

} // namespace Mg::gfx
