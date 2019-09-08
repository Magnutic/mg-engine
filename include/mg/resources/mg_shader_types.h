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

/** @file mg_shader_types.h
 * Types relating to shaders and associated functions.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_optional.h"

#include <array>
#include <cstddef>
#include <string_view>

namespace Mg {

/** Types and utilities related to shaders. */
namespace shader {

enum class SamplerType { Sampler2D, SamplerCube };

inline std::string_view sampler_type_to_string(SamplerType type) noexcept
{
    switch (type) {
    case SamplerType::Sampler2D:
        return "sampler2D";
    case SamplerType::SamplerCube:
        return "samplerCube";
    }
    MG_ASSERT(false && "unreachable");
}

inline Opt<SamplerType> string_to_sampler_type(std::string_view str) noexcept
{
    if (str == "sampler2D") { return SamplerType::Sampler2D; }
    if (str == "samplerCube") { return SamplerType::SamplerCube; }
    return nullopt;
}

enum class ParameterType { Int, Float, Vec2, Vec4 }; // Order matters, used for sorting

inline std::string_view parameter_type_to_string(ParameterType type) noexcept
{
    switch (type) {
    case ParameterType::Int:
        return "int";
    case ParameterType::Float:
        return "float";
    case ParameterType::Vec2:
        return "vec2";
    case ParameterType::Vec4:
        return "vec4";
    }
    MG_ASSERT(false && "unreachable");
}

inline Opt<ParameterType> string_to_parameter_type(std::string_view str) noexcept
{
    if (str == "int") { return ParameterType::Int; }
    if (str == "float") { return ParameterType::Float; }
    if (str == "vec2") { return ParameterType::Vec2; }
    if (str == "vec4") { return ParameterType::Vec4; }
    return nullopt;
}

namespace Tag {
using Value = uint32_t;
enum Flags : Value {
    OPAQUE                    = 1 << 0,
    UNLIT                     = 1 << 1,
    DEFINES_VERTEX_PREPROCESS = 1 << 2,
    DEFINES_LIGHT_MODEL       = 1 << 3,
};
} // namespace Tag

struct Sampler {
    Identifier  name{ "" };
    SamplerType type{};
};

struct Parameter {
    static constexpr size_t k_max_size = 4 * sizeof(float);

    Identifier                        name{ "" };
    ParameterType                     type{};
    std::array<std::byte, k_max_size> value{};
};

struct Option {
    Identifier name{ "" };
    bool       default_value = false;
};

} // namespace shader

} // namespace Mg
