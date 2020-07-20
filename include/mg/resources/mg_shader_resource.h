//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_shader_resource.h
 *
 */

#pragma once

#include "mg/containers/mg_array.h"
#include "mg/resource_cache/mg_base_resource.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_optional.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

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

namespace Tag {
using Value = uint32_t;
enum Flags : Value {
    OPAQUE = 1 << 0,
    UNLIT = 1 << 1,
    DEFINES_VERTEX_PREPROCESS = 1 << 2,
    DEFINES_LIGHT_MODEL = 1 << 3,
};
} // namespace Tag

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

namespace Mg {

class ShaderResource : public BaseResource {
public:
    using BaseResource::BaseResource;

    span<const shader::Sampler> samplers() const noexcept { return m_samplers; }
    span<const shader::Parameter> parameters() const noexcept { return m_parameters; }
    span<const shader::Option> options() const noexcept { return m_options; }

    std::string_view vertex_code() const noexcept { return m_vertex_code; }
    std::string_view fragment_code() const noexcept { return m_fragment_code; }

    shader::Tag::Value tags() const noexcept { return m_tags; }

    bool should_reload_on_file_change() const override { return true; }

    std::string debug_print() const;

    Identifier type_id() const override { return "ShaderResource"; }

protected:
    LoadResourceResult load_resource_impl(const ResourceLoadingInput& input) override;

private:
    std::vector<shader::Parameter> m_parameters;
    std::vector<shader::Sampler> m_samplers;
    std::vector<shader::Option> m_options;

    std::string m_vertex_code;
    std::string m_fragment_code;

    shader::Tag::Value m_tags = 0;
};

} // namespace Mg
