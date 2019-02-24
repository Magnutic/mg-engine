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

/** @file mg_shader.h
 * Shader construction, lifetime management, and configuration wrapper.
 */

#pragma once

#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_object_id.h"

#include <optional>
#include <string_view>

namespace Mg::gfx {

enum class ShaderStage { VERTEX, FRAGMENT, GEOMETRY };

//--------------------------------------------------------------------------------------------------

namespace detail {
// Create shader and return its id, or 0 if construction failed.
uint32_t create_shader(ShaderStage type, std::string_view code);
// Delete shader with given id.
void delete_shader(uint32_t id);
} // namespace detail

//--------------------------------------------------------------------------------------------------

/** Shader object, code controlling a stage in the rendering pipeline. */
template<ShaderStage stage> class Shader {
public:
    /** Creates an shader using the supplied GLSL code. */
    static std::optional<Shader> make(std::string_view shader_code)
    {
        auto shader_id = detail::create_shader(stage, shader_code);
        if (shader_id == 0) return std::nullopt;
        return Shader{ shader_id };
    }

    // Allow moving but not copying as the object manages external state.
    MG_MAKE_DEFAULT_MOVABLE(Shader);
    MG_MAKE_NON_COPYABLE(Shader);

    ~Shader() { detail::delete_shader(m_gfx_api_id.value); }

    /** Get the identifier for the OpenGL object owned by this Shader. */
    uint32_t shader_id() const { return m_gfx_api_id.value; }

private:
    Shader(uint32_t gfx_api_id) : m_gfx_api_id(gfx_api_id) {}

    ObjectId m_gfx_api_id;
};

using VertexShader   = Shader<ShaderStage::VERTEX>;
using GeometryShader = Shader<ShaderStage::GEOMETRY>;
using FragmentShader = Shader<ShaderStage::FRAGMENT>;

//--------------------------------------------------------------------------------------------------

/** ShaderProgram is a linked set of Shaders. This is what is used to render objects.  */
class ShaderProgram {
public:
    enum class GfxApiHandle : uintptr_t;

    /** Construct a shader program with only a vertex shader. Useful for depth passes. */
    static std::optional<ShaderProgram> make(const VertexShader& vs);

    /** Construct a shader program by linking the supplied Shaders */
    static std::optional<ShaderProgram> make(const VertexShader& vs, const FragmentShader& fs);

    /** Construct a shader program by linking the supplied Shaders */
    static std::optional<ShaderProgram>
    make(const VertexShader& vs, const GeometryShader& gs, const FragmentShader& fs);

    // Allow moving but not copying as the object manages external state.
    MG_MAKE_DEFAULT_MOVABLE(ShaderProgram);
    MG_MAKE_NON_COPYABLE(ShaderProgram);

    /** Destroys OpenGL shader program as well on destruction. */
    ~ShaderProgram();

    /** Get opaque handle to shader in underlying graphics API. */
    GfxApiHandle gfx_api_handle() const { return GfxApiHandle{ m_gfx_api_id.value }; }

private:
    explicit ShaderProgram(uint32_t gfx_api_id) : m_gfx_api_id(gfx_api_id) {}

    ObjectId m_gfx_api_id;
};

} // namespace Mg::gfx
