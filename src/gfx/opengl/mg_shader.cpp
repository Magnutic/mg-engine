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

#include <mg/gfx/mg_shader.h>

#include <memory>
#include <stdexcept>
#include <string>

#include <fmt/core.h>

#include <mg/core/mg_log.h>
#include <mg/utils/mg_gsl.h>
#include <mg/utils/mg_macros.h>

#include "mg_gl_debug.h"
#include "mg_glad.h"

namespace Mg::gfx {

inline auto alloc_message(int32_t msg_len)
{
    return std::make_unique<char[]>(narrow<size_t>(msg_len + 1));
}

inline GLenum shader_stage_to_gl_enum(ShaderStage stage)
{
    switch (stage) {
    case ShaderStage::VERTEX: return GL_VERTEX_SHADER;
    case ShaderStage::FRAGMENT: return GL_FRAGMENT_SHADER;
    case ShaderStage::GEOMETRY: return GL_GEOMETRY_SHADER;
    }

    throw std::runtime_error("Reached unreachable code.");
}

namespace detail {

uint32_t create_shader(ShaderStage type, std::string_view code)
{
    auto gl_shader_type = shader_stage_to_gl_enum(type);
    auto code_str       = std::string(code);
    auto code_c_str     = code_str.c_str();

    // Upload and compile shader
    auto id = glCreateShader(gl_shader_type);
    glShaderSource(id, 1, &code_c_str, nullptr);
    glCompileShader(id);

    // Check shader for compilation errors
    int32_t result = GL_FALSE;
    int32_t log_length;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &log_length);

    std::unique_ptr<char[]> msg;

    // If there was a message, write to log
    if (log_length > 1) {
        msg = alloc_message(log_length + 1);
        glGetShaderInfoLog(id, log_length, nullptr, msg.get());

        auto msg_type = result != 0 ? Log::Prio::Message : Log::Prio::Error;
        g_log.write(msg_type, fmt::format("Shader compilation message: {}", msg.get()));
    }

    // Check whether shader compiled successfully
    if (result == GL_FALSE) {
        glDeleteShader(id);
        return 0;
    }

    return id;
}

void delete_shader(uint32_t id)
{
    if (id == 0) { return; }
    glDeleteShader(id);
}

} // namespace detail

//--------------------------------------------------------------------------------------------------
// Helpers for ShaderProgram implementation
//--------------------------------------------------------------------------------------------------

// Links the given shader program, returning whether linking was successful.
static bool link_program(uint32_t program_id)
{
    glLinkProgram(program_id);

    // Check the program for linking errors
    int32_t result = GL_FALSE, log_length;
    glGetProgramiv(program_id, GL_LINK_STATUS, &result);
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

    std::unique_ptr<char[]> error_msg;

    // If there was a log message, write to Mg Engine log
    if (log_length > 1) {
        error_msg = alloc_message(log_length + 1);
        glGetProgramInfoLog(program_id, log_length, nullptr, error_msg.get());

        auto msg_type = result != 0 ? Log::Prio::Message : Log::Prio::Error;
        g_log.write(msg_type, fmt::format("Shader linking message: {}", error_msg.get()));
    }

    // Check whether shaders linked successfully
    if (result == GL_FALSE) {
        glDeleteProgram(program_id);
        return false;
    }

    return true;
}

/** RAII guard for attaching shader object to shader program. */
class ShaderAttachGuard {
public:
    ShaderAttachGuard(uint32_t program, uint32_t shader) : _program(program), _shader(shader)
    {
        glAttachShader(program, shader);
    }

    MG_MAKE_NON_COPYABLE(ShaderAttachGuard);
    MG_MAKE_NON_MOVABLE(ShaderAttachGuard);

    ~ShaderAttachGuard() { glDetachShader(_program.value, _shader.value); }

    ObjectId _program{};
    ObjectId _shader{};
};

//--------------------------------------------------------------------------------------------------
// ShaderProgram implementation
//--------------------------------------------------------------------------------------------------

std::optional<ShaderProgram> ShaderProgram::make(const VertexShader& vs)
{
    auto              program_id = glCreateProgram();
    ShaderAttachGuard guard_vs(program_id, vs.shader_id());

    if (link_program(program_id)) { return ShaderProgram(program_id); }

    return std::nullopt;
}

std::optional<ShaderProgram> ShaderProgram::make(const VertexShader& vs, const FragmentShader& fs)
{
    auto              program_id = glCreateProgram();
    ShaderAttachGuard guard_vs(program_id, vs.shader_id());
    ShaderAttachGuard guard_fs(program_id, fs.shader_id());

    if (link_program(program_id)) { return ShaderProgram(program_id); }

    return std::nullopt;
}

std::optional<ShaderProgram>
ShaderProgram::make(const VertexShader& vs, const GeometryShader& gs, const FragmentShader& fs)
{
    auto              program_id = glCreateProgram();
    ShaderAttachGuard guard_vs(program_id, vs.shader_id());
    ShaderAttachGuard guard_fs(program_id, fs.shader_id());
    ShaderAttachGuard guard_gs(program_id, gs.shader_id());

    if (link_program(program_id)) { return ShaderProgram(program_id); }

    return std::nullopt;
}

ShaderProgram::~ShaderProgram()
{
    glDeleteProgram(m_gfx_api_id.value);
}


} // namespace Mg::gfx
