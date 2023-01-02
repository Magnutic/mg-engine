//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_shader_related_types.h
 *
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_optional.h"

#include <string_view>

/** Types and utilities related to shaders. */
namespace Mg::shader {

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
    if (str == "sampler2D") {
        return SamplerType::Sampler2D;
    }
    if (str == "samplerCube") {
        return SamplerType::SamplerCube;
    }
    return nullopt;
}

enum class ParameterType { Int, Float, Vec2, Vec4 }; // Order matters, used for sorting

inline size_t parameter_type_num_elements(ParameterType type) noexcept
{
    switch (type) {
    case ParameterType::Int:
    case ParameterType::Float:
        return 1;
    case ParameterType::Vec2:
        return 2;
    case ParameterType::Vec4:
        return 4;
    }
    MG_ASSERT(false && "unreachable");
}

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
    if (str == "int") {
        return ParameterType::Int;
    }
    if (str == "float") {
        return ParameterType::Float;
    }
    if (str == "vec2") {
        return ParameterType::Vec2;
    }
    if (str == "vec4") {
        return ParameterType::Vec4;
    }
    return nullopt;
}

enum class Tag : uint32_t {
    opaque = 1 << 0,
    unlit = 1 << 1,
    defines_vertex_preprocess = 1 << 2,
    defines_light_model = 1 << 3
};

MG_DEFINE_BITMASK_OPERATORS(Tag);

inline Opt<Tag> string_to_tag(std::string_view str) noexcept
{
    if (str == "opaque") {
        return Tag::opaque;
    }
    if (str == "unlit") {
        return Tag::unlit;
    }
    if (str == "defines_vertex_preprocess") {
        return Tag::defines_vertex_preprocess;
    }
    if (str == "defines_light_model") {
        return Tag::defines_light_model;
    }
    return nullopt;
}

struct Sampler {
    Identifier name{ "" };
    SamplerType type{};
};

struct Parameter {
    static constexpr size_t k_max_size = 4 * sizeof(float);

    Identifier name{ "" };
    ParameterType type{};
    std::array<std::byte, k_max_size> value{};
};

struct Option {
    Identifier name{ "" };
    bool default_value = false;
};

} // namespace Mg::shader
