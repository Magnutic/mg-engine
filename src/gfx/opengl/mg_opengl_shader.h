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

/** @file mg_opengl_shader.h
 * Functions for Mg::gfx::ShaderProgram providing OpenGL-specific functionality.
 */

#pragma once

#include <cstdint>
#include <string_view>

#include <mg/gfx/mg_shader.h>
#include <mg/gfx/mg_uniform_buffer.h>

/** OpenGL-backend-specific functionality. */
namespace Mg::gfx::opengl {

void use_program(const ShaderProgram& program);

/** Get the location for the given uniform.
 * @return Index number if uniform_name corresponds to an active uniform, -1 otherwise.
 */
int32_t uniform_location(const ShaderProgram& program, std::string_view uniform_name);

/** Type-safe wrapper for glUniform*.
 * Supports GLM vectors and matrices, int32_t, uint32_t, and float.
 * Defined in mg_opengl_shader.cpp
 */
template<typename T> void set_uniform(int32_t location, const T& value);

/** Bind Shader's block with name block_name to the given uniform buffer slot.
 * @return Whether successful (i.e. block_name corresponds to an active uniform block).
 */
bool set_uniform_block_binding(const ShaderProgram& program,
                               std::string_view     block_name,
                               UniformBufferSlot    slot);

} // namespace Mg::gfx::opengl
