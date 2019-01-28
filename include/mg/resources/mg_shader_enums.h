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

/** @file mg_shader_enums.h
 * Enumeration types relating to shaders and associated functions.
 */

#pragma once

#include <optional>
#include <string_view>

namespace Mg {

enum class ShaderSamplerType { Sampler2D, SamplerCube };

inline std::string_view shader_sampler_type_to_string(ShaderSamplerType type)
{
    switch (type) {
    case ShaderSamplerType::Sampler2D: return "sampler2D";
    case ShaderSamplerType::SamplerCube: return "samplerCube";
    }
    throw "unreachable";
}

inline std::optional<ShaderSamplerType> string_to_shader_sampler_type(std::string_view str)
{
    if (str == "sampler2D") return ShaderSamplerType::Sampler2D;
    if (str == "samplerCube") return ShaderSamplerType::SamplerCube;
    return std::nullopt;
}

enum class ShaderParameterType { Float, Vec2, Vec4 }; // Order matters, used for sorting

inline std::string_view shader_parameter_type_to_string(ShaderParameterType type)
{
    switch (type) {
    case ShaderParameterType::Float: return "float";
    case ShaderParameterType::Vec2: return "vec2";
    case ShaderParameterType::Vec4: return "vec4";
    }
    throw "unreachable";
}

inline std::optional<ShaderParameterType> string_to_shader_parameter_type(std::string_view str)
{
    if (str == "float") return ShaderParameterType::Float;
    if (str == "vec2") return ShaderParameterType::Vec2;
    if (str == "vec4") return ShaderParameterType::Vec4;
    return std::nullopt;
}

namespace ShaderTag {
using Value = uint32_t;
enum Flags : Value {
    OPAQUE                    = 1 << 0,
    UNLIT                     = 1 << 1,
    DEFINES_VERTEX_PREPROCESS = 1 << 2,
    DEFINES_LIGHT_MODEL       = 1 << 3,
};
} // namespace ShaderTag


} // namespace Mg
