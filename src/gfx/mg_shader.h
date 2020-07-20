//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_shader.h
 * Types and functions for creating shaders.
 */

#pragma once

#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/utils/mg_optional.h"

#include <string>

namespace Mg::gfx {

Opt<VertexShaderHandle> compile_vertex_shader(const std::string& code);
Opt<FragmentShaderHandle> compile_fragment_shader(const std::string& code);
Opt<GeometryShaderHandle> compile_geometry_shader(const std::string& code);

void destroy_shader(VertexShaderHandle handle) noexcept;
void destroy_shader(FragmentShaderHandle handle) noexcept;
void destroy_shader(GeometryShaderHandle handle) noexcept;

} // namespace Mg::gfx
