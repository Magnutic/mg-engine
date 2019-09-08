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

#include "mg_opengl_shader.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

#include "mg/gfx/mg_shader.h"
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

namespace Mg::gfx::opengl {

namespace {

GLuint gl_program_id(ShaderHandle program) noexcept
{
    return static_cast<GLuint>(program);
}

Opt<GLuint> uniform_block_index(GLuint ubo_id, std::string_view block_name) noexcept
{
    auto block_index = glGetUniformBlockIndex(ubo_id, std::string(block_name).c_str());
    return block_index == GL_INVALID_INDEX ? nullopt : make_opt(block_index);
}

} // namespace

void use_program(ShaderHandle program) noexcept
{
    MG_ASSERT(gl_program_id(program) != 0);
    glUseProgram(gl_program_id(program));
}

Opt<UniformLocation> uniform_location(ShaderHandle program, std::string_view uniform_name) noexcept
{
    auto location = glGetUniformLocation(gl_program_id(program), std::string(uniform_name).c_str());
    return location == -1 ? nullopt : make_opt(UniformLocation{ location });
}

bool set_uniform_block_binding(ShaderHandle      program,
                               std::string_view  block_name,
                               UniformBufferSlot slot) noexcept
{
    const Opt<GLuint> opt_block_index = uniform_block_index(gl_program_id(program), block_name);
    const auto        slot_index      = static_cast<GLuint>(slot);

    if (opt_block_index) {
        glUniformBlockBinding(gl_program_id(program), *opt_block_index, slot_index);
        return true;
    }
    else {
        return false;
    }
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
