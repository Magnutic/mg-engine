//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_opengl_shader.h
 * ShaderProgram, a linked set of shaders for the OpenGL back-end.
 */

#pragma once

#include "mg/gfx/mg_shader.h"
#include "mg/gfx/mg_texture_related_types.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/utils/mg_optional.h"

#include <cstdint>
#include <string_view>

/** Functionality specific to the OpenGL backend. */
namespace Mg::gfx::opengl {

void use_program(ShaderHandle program) noexcept;

enum class UniformLocation : int32_t;

/** Get the location for the given uniform.
 * @return Index number if uniform_name corresponds to an active uniform, nullopt otherwise.
 */
Opt<UniformLocation> uniform_location(ShaderHandle program, std::string_view uniform_name) noexcept;

/** Type-safe wrapper for glUniform*.
 * Supports GLM vectors and matrices, int32_t, uint32_t, and float.
 * Defined in mg_opengl_shader.cpp
 */
template<typename T> void set_uniform(UniformLocation location, const T& value) noexcept;

/** Set texture unit to use for the given sampler uniform.
 *
 * This could be done directly using set_uniform, but OpenGL is a bit particular about types here: a
 * texture unit from the application's side must be an unsigned integer, but the shader uniform must
 * be a signed integer. Hence, this function will automatically ensure the correct type, making it
 * preferable to use.
 */
void set_sampler_binding(UniformLocation location, TextureUnit unit) noexcept;

/** Bind Shader's block with name block_name to the given uniform buffer slot.
 * @return Whether successful (i.e. block_name corresponds to an active uniform block).
 */
[[nodiscard]] bool set_uniform_block_binding(ShaderHandle program,
                                             std::string_view block_name,
                                             UniformBufferSlot slot) noexcept;

} // namespace Mg::gfx::opengl
