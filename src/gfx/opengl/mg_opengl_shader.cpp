//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_opengl_shader.h"

#include "../mg_shader.h"
#include "mg_glad.h"

#include "mg/core/mg_log.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_optional.h"

#include <glm/mat2x2.hpp>
#include <glm/mat2x3.hpp>
#include <glm/mat2x4.hpp>
#include <glm/mat3x2.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat3x4.hpp>
#include <glm/mat4x2.hpp>
#include <glm/mat4x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <fmt/core.h>

namespace Mg::gfx::opengl {

//--------------------------------------------------------------------------------------------------
// Helpers for ShaderProgram implementation
//--------------------------------------------------------------------------------------------------

namespace {

// Links the given shader program, returning whether linking was successful.
bool link_program(GLuint program_id)
{
    glLinkProgram(program_id);

    // Check the program for linking errors.
    int32_t result = GL_FALSE;
    int32_t log_length = 0;
    glGetProgramiv(program_id, GL_LINK_STATUS, &result);
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

    std::string msg;

    // If there was log message, write to log.
    if (log_length > 1) {
        msg.resize(narrow<size_t>(log_length));
        glGetProgramInfoLog(program_id, log_length, nullptr, msg.data());

        const auto msg_type = result != 0 ? Log::Prio::Message : Log::Prio::Error;
        log.write(msg_type, fmt::format("Shader linking message: {}", msg));
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
    ShaderAttachGuard(GLuint program, Opt<GLuint> shader) noexcept
        : _program(program), _shader(shader)
    {
        if (_shader.has_value()) {
            glAttachShader(_program, _shader.value());
        }
    }

    MG_MAKE_NON_COPYABLE(ShaderAttachGuard);
    MG_MAKE_NON_MOVABLE(ShaderAttachGuard);

    ~ShaderAttachGuard()
    {
        if (_shader.has_value()) {
            glDetachShader(_program, _shader.value());
        }
    }

private:
    GLuint _program{};
    Opt<GLuint> _shader{};
};

Opt<GLuint> uniform_block_index(GLuint ubo_id, std::string_view block_name) noexcept
{
    auto block_index = glGetUniformBlockIndex(ubo_id, std::string(block_name).c_str());
    return block_index == GL_INVALID_INDEX ? nullopt : make_opt(block_index);
}

} // namespace

//--------------------------------------------------------------------------------------------------
// ShaderProgram implementation
//--------------------------------------------------------------------------------------------------

Opt<ShaderProgramHandle> link_shader_program(VertexShaderHandle vertex_shader,
                                             Opt<GeometryShaderHandle> geometry_shader,
                                             Opt<FragmentShaderHandle> fragment_shader)
{
    MG_GFX_DEBUG_GROUP("link_shader_program");

    auto get_and_narrow = [](const auto handle) { return handle.as_gl_id(); };

    const GLuint program_id = glCreateProgram();
    ShaderAttachGuard guard_vs(program_id, vertex_shader.as_gl_id());
    ShaderAttachGuard guard_gs(program_id, geometry_shader.map(get_and_narrow));
    ShaderAttachGuard guard_fs(program_id, fragment_shader.map(get_and_narrow));

    if (link_program(program_id)) {
        return ShaderProgramHandle(program_id);
    }

    return nullopt;
}

void destroy_shader_program(ShaderProgramHandle handle) noexcept
{
    glDeleteProgram(handle.as_gl_id());
}

void use_program(ShaderProgramHandle program) noexcept
{
    MG_ASSERT(program.as_gl_id() != 0);
    glUseProgram(program.as_gl_id());
}

Opt<UniformLocation> uniform_location(ShaderProgramHandle program,
                                      std::string_view uniform_name) noexcept
{
    auto location = glGetUniformLocation(program.as_gl_id(), std::string(uniform_name).c_str());
    return location == -1 ? nullopt : make_opt(UniformLocation{ location });
}

bool set_uniform_block_binding(ShaderProgramHandle program,
                               std::string_view block_name,
                               UniformBufferSlot slot) noexcept
{
    const Opt<GLuint> opt_block_index = uniform_block_index(program.as_gl_id(), block_name);
    const auto slot_index = static_cast<GLuint>(slot);

    if (opt_block_index) {
        glUniformBlockBinding(program.as_gl_id(), *opt_block_index, slot_index);
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------

template<> void set_uniform<int32_t>(UniformLocation location, const int32_t& value) noexcept
{
    glUniform1iv(static_cast<int32_t>(location), 1, &value);
}

template<> void set_uniform<uint32_t>(UniformLocation location, const uint32_t& value) noexcept
{
    glUniform1uiv(static_cast<int32_t>(location), 1, &value);
}

template<> void set_uniform<float>(UniformLocation location, const float& value) noexcept
{
    glUniform1fv(static_cast<int32_t>(location), 1, &value);
}

template<> void set_uniform<glm::ivec2>(UniformLocation location, const glm::ivec2& value) noexcept
{
    glUniform2iv(static_cast<int32_t>(location), 1, &(value[0]));
}

template<> void set_uniform<glm::ivec3>(UniformLocation location, const glm::ivec3& value) noexcept
{
    glUniform3iv(static_cast<int32_t>(location), 1, &(value[0]));
}

template<> void set_uniform<glm::ivec4>(UniformLocation location, const glm::ivec4& value) noexcept
{
    glUniform4iv(static_cast<int32_t>(location), 1, &(value[0]));
}

template<> void set_uniform<glm::uvec2>(UniformLocation location, const glm::uvec2& value) noexcept
{
    glUniform2uiv(static_cast<int32_t>(location), 1, &(value[0]));
}

template<> void set_uniform<glm::uvec3>(UniformLocation location, const glm::uvec3& value) noexcept
{
    glUniform3uiv(static_cast<int32_t>(location), 1, &(value[0]));
}

template<> void set_uniform<glm::uvec4>(UniformLocation location, const glm::uvec4& value) noexcept
{
    glUniform4uiv(static_cast<int32_t>(location), 1, &(value[0]));
}

template<> void set_uniform<glm::vec2>(UniformLocation location, const glm::vec2& value) noexcept
{
    glUniform2fv(static_cast<int32_t>(location), 1, &(value[0]));
}

template<> void set_uniform<glm::vec3>(UniformLocation location, const glm::vec3& value) noexcept
{
    glUniform3fv(static_cast<int32_t>(location), 1, &(value[0]));
}

template<> void set_uniform<glm::vec4>(UniformLocation location, const glm::vec4& value) noexcept
{
    glUniform4fv(static_cast<int32_t>(location), 1, &(value[0]));
}

template<> void set_uniform<glm::mat2>(UniformLocation location, const glm::mat2& value) noexcept
{
    glUniformMatrix2fv(static_cast<int32_t>(location), 1, 0, &(value[0][0]));
}

template<> void set_uniform<glm::mat3>(UniformLocation location, const glm::mat3& value) noexcept
{
    glUniformMatrix3fv(static_cast<int32_t>(location), 1, 0, &(value[0][0]));
}

template<> void set_uniform<glm::mat4>(UniformLocation location, const glm::mat4& value) noexcept
{
    glUniformMatrix4fv(static_cast<int32_t>(location), 1, 0, &(value[0][0]));
}

template<>
void set_uniform<glm::mat2x3>(UniformLocation location, const glm::mat2x3& value) noexcept
{
    glUniformMatrix2x3fv(static_cast<int32_t>(location), 1, 0, &(value[0][0]));
}

template<>
void set_uniform<glm::mat3x2>(UniformLocation location, const glm::mat3x2& value) noexcept
{
    glUniformMatrix3x2fv(static_cast<int32_t>(location), 1, 0, &(value[0][0]));
}

template<>
void set_uniform<glm::mat2x4>(UniformLocation location, const glm::mat2x4& value) noexcept
{
    glUniformMatrix2x4fv(static_cast<int32_t>(location), 1, 0, &(value[0][0]));
}

template<>
void set_uniform<glm::mat4x2>(UniformLocation location, const glm::mat4x2& value) noexcept
{
    glUniformMatrix4x2fv(static_cast<int32_t>(location), 1, 0, &(value[0][0]));
}

template<>
void set_uniform<glm::mat3x4>(UniformLocation location, const glm::mat3x4& value) noexcept
{
    glUniformMatrix3x4fv(static_cast<int32_t>(location), 1, 0, &(value[0][0]));
}

template<>
void set_uniform<glm::mat4x3>(UniformLocation location, const glm::mat4x3& value) noexcept
{
    glUniformMatrix4x3fv(static_cast<int32_t>(location), 1, 0, &(value[0][0]));
}

void set_sampler_binding(UniformLocation location, TextureUnit unit) noexcept
{
    set_uniform(location, static_cast<int32_t>(unit.get()));
}

} // namespace Mg::gfx::opengl
