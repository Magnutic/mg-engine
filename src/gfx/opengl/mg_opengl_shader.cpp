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

#include "mg/core/mg_log.h"
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

#include <fmt/core.h>

namespace Mg::gfx::opengl {

inline GLuint gl_program_id(ShaderHandle program)
{
    return static_cast<GLuint>(program);
}

inline Opt<GLuint> uniform_block_index(GLuint ubo_id, std::string_view block_name)
{
    auto block_index = glGetUniformBlockIndex(ubo_id, std::string(block_name).c_str());
    return block_index == GL_INVALID_INDEX ? nullopt : make_opt(block_index);
}

void use_program(ShaderHandle program)
{
    MG_ASSERT(gl_program_id(program) != 0);
    glUseProgram(gl_program_id(program));
}

int32_t uniform_location(ShaderHandle program, std::string_view uniform_name)
{
    auto ret_val = glGetUniformLocation(gl_program_id(program), std::string(uniform_name).c_str());
    return ret_val;
}

bool set_uniform_block_binding(ShaderHandle      program,
                               std::string_view  block_name,
                               UniformBufferSlot slot)
{
    const Opt<GLuint> opt_block_index = uniform_block_index(gl_program_id(program), block_name);
    const auto        slot_index      = static_cast<GLuint>(slot);

    auto bind = [&](GLuint block_index) {
        glUniformBlockBinding(gl_program_id(program), block_index, slot_index);
        return true;
    };

    auto log_no_such_block = [&] {
        static const auto msg =
            "set_uniform_block_binding(\"{}\"): "
            "no such active uniform block in shader.";
        g_log.write_warning(fmt::format(msg, block_name));
        return false;
    };

    return opt_block_index.map(bind).or_else(log_no_such_block).value();
}

//--------------------------------------------------------------------------------------------------

template<> void set_uniform<int32_t>(int32_t location, const int32_t& value)
{
    glUniform1iv(location, 1, &value);
}

template<> void set_uniform<uint32_t>(int32_t location, const uint32_t& value)
{
    glUniform1uiv(location, 1, &value);
}

template<> void set_uniform<float>(int32_t location, const float& value)
{
    glUniform1fv(location, 1, &value);
}

template<> void set_uniform<glm::ivec2>(int32_t location, const glm::ivec2& value)
{
    glUniform2iv(location, 1, &(value[0]));
}

template<> void set_uniform<glm::ivec3>(int32_t location, const glm::ivec3& value)
{
    glUniform3iv(location, 1, &(value[0]));
}

template<> void set_uniform<glm::ivec4>(int32_t location, const glm::ivec4& value)
{
    glUniform4iv(location, 1, &(value[0]));
}

template<> void set_uniform<glm::uvec2>(int32_t location, const glm::uvec2& value)
{
    glUniform2uiv(location, 1, &(value[0]));
}

template<> void set_uniform<glm::uvec3>(int32_t location, const glm::uvec3& value)
{
    glUniform3uiv(location, 1, &(value[0]));
}

template<> void set_uniform<glm::uvec4>(int32_t location, const glm::uvec4& value)
{
    glUniform4uiv(location, 1, &(value[0]));
}

template<> void set_uniform<glm::vec2>(int32_t location, const glm::vec2& value)
{
    glUniform2fv(location, 1, &(value[0]));
}

template<> void set_uniform<glm::vec3>(int32_t location, const glm::vec3& value)
{
    glUniform3fv(location, 1, &(value[0]));
}

template<> void set_uniform<glm::vec4>(int32_t location, const glm::vec4& value)
{
    glUniform4fv(location, 1, &(value[0]));
}

template<> void set_uniform<glm::mat2>(int32_t location, const glm::mat2& value)
{
    glUniformMatrix2fv(location, 1, 0, &(value[0][0]));
}

template<> void set_uniform<glm::mat3>(int32_t location, const glm::mat3& value)
{
    glUniformMatrix3fv(location, 1, 0, &(value[0][0]));
}

template<> void set_uniform<glm::mat4>(int32_t location, const glm::mat4& value)
{
    glUniformMatrix4fv(location, 1, 0, &(value[0][0]));
}

template<> void set_uniform<glm::mat2x3>(int32_t location, const glm::mat2x3& value)
{
    glUniformMatrix2x3fv(location, 1, 0, &(value[0][0]));
}

template<> void set_uniform<glm::mat3x2>(int32_t location, const glm::mat3x2& value)
{
    glUniformMatrix3x2fv(location, 1, 0, &(value[0][0]));
}

template<> void set_uniform<glm::mat2x4>(int32_t location, const glm::mat2x4& value)
{
    glUniformMatrix2x4fv(location, 1, 0, &(value[0][0]));
}

template<> void set_uniform<glm::mat4x2>(int32_t location, const glm::mat4x2& value)
{
    glUniformMatrix4x2fv(location, 1, 0, &(value[0][0]));
}

template<> void set_uniform<glm::mat3x4>(int32_t location, const glm::mat3x4& value)
{
    glUniformMatrix3x4fv(location, 1, 0, &(value[0][0]));
}

template<> void set_uniform<glm::mat4x3>(int32_t location, const glm::mat4x3& value)
{
    glUniformMatrix4x3fv(location, 1, 0, &(value[0][0]));
}

void set_sampler_binding(int32_t location, TextureUnit unit)
{
    set_uniform(location, static_cast<int32_t>(unit.get()));
}

} // namespace Mg::gfx::opengl
