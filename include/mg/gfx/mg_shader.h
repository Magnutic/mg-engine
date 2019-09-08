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

/** @file mg_shader.h
 * Types and functions for creating shaders.
 */

#pragma once

#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_opaque_handle.h"
#include "mg/utils/mg_optional.h"

#include <string>

namespace Mg::gfx {

/** Handle to a shader (of any shader-stage). */
struct ShaderId {
    uint64_t value;
}; // TODO: currently in the middle of major refactoring; this should
   // be called ShaderHandle but that name is taken at the moment.

/** Shader stage: which (programmable) part of the rendering pipeline the shader implements. */
enum class ShaderStage { Vertex, Fragment, Geometry };

/** Strongly typed handle to a shader of a particular shader stage. */
template<ShaderStage> struct TypedShaderHandle : ShaderId {};

/** Handle to a vertex shader. */
using VertexShaderHandle = TypedShaderHandle<ShaderStage::Vertex>;

/** Handle to a geometry shader. */
using GeometryShaderHandle = TypedShaderHandle<ShaderStage::Geometry>;

/** Handle to a fragment shader. */
using FragmentShaderHandle = TypedShaderHandle<ShaderStage::Fragment>;

Opt<VertexShaderHandle>   compile_vertex_shader(const std::string& code);
Opt<FragmentShaderHandle> compile_fragment_shader(const std::string& code);
Opt<GeometryShaderHandle> compile_geometry_shader(const std::string& code);

void destroy_shader(ShaderId handle) noexcept;

/** RAII-owning wrapper for shader handles. */
template<ShaderStage stage> class ShaderOwner {
public:
    explicit ShaderOwner(TypedShaderHandle<stage> handle) noexcept : m_handle(handle) {}
    ~ShaderOwner() { destroy_shader(m_handle); }

    MG_MAKE_NON_COPYABLE(ShaderOwner);
    MG_MAKE_DEFAULT_MOVABLE(ShaderOwner);

    TypedShaderHandle<stage> shader_handle() const noexcept { return m_handle; }

private:
    TypedShaderHandle<stage> m_handle;
};

enum class ShaderHandle : uint64_t;

/** Construct a shader program by linking the supplied Shaders */
Opt<ShaderHandle> link_shader_program(VertexShaderHandle        vertex_shader,
                                      Opt<GeometryShaderHandle> geometry_shader,
                                      Opt<FragmentShaderHandle> fragment_shader);

void destroy_shader_program(ShaderHandle handle) noexcept;

class ShaderProgramOwner {
public:
    explicit ShaderProgramOwner(ShaderHandle handle) noexcept : m_handle(handle) {}
    ~ShaderProgramOwner() { destroy_shader_program(m_handle); }

    MG_MAKE_DEFAULT_MOVABLE(ShaderProgramOwner);
    MG_MAKE_NON_COPYABLE(ShaderProgramOwner);

    ShaderHandle program_handle() const noexcept { return m_handle; }

private:
    ShaderHandle m_handle;
};

} // namespace Mg::gfx
