//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/parser/mg_parser.h"

#include "mg/gfx/mg_blend_modes.h"
#include "mg/gfx/mg_shader_related_types.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_string_utils.h"

#include <fmt/core.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <hjson/hjson.h>

#include <algorithm>

namespace Mg::parser {

namespace {

void parse_error(std::string_view key, std::string_view reason)
{
    throw RuntimeError{ "Error parsing key '{}': {}", key, reason };
}

// Convert shader::ParameterType to compatible Mg::Value::Type.
Value::Type corresponding_value_type(shader::ParameterType type) noexcept
{
    switch (type) {
    case shader::ParameterType::Int:
        return Value::Type::Int64;

    case shader::ParameterType::Float:
        return Value::Type::Double;

    case shader::ParameterType::Vec2:
        return Value::Type::Vec2;

    case shader::ParameterType::Vec4:
        return Value::Type::Vec4;
    }
    MG_ASSERT(false && "unexpected shader::ParameterType");
}

std::string_view hjson_type_to_string(Hjson::Type type)
{
    using enum Hjson::Type;

    switch (type) {
    case Undefined:
        return "Undefined";
    case Null:
        return "Null";
    case Bool:
        return "Bool";
    case Double:
        return "Double";
    case Int64:
        return "Int64";
    case String:
        return "String";
    case Vector:
        return "Vector";
    case Map:
        return "Map";
    }

    MG_ASSERT(false && "unexpected Hjson::Type");
}

auto get_property(const Hjson::Value& value,
                  const char* key,
                  const Hjson::Type expected_type,
                  const bool mandatory = false)
{
    const Hjson::Value& property = value[key];
    if (property.defined() && property.type() != expected_type) {
        parse_error(key,
                    fmt::format("Unexpected type. Expected: {}. Was: {}.",
                                hjson_type_to_string(expected_type),
                                hjson_type_to_string(property.type())));
    }
    if (mandatory && !property.defined()) {
        parse_error(key,
                    fmt::format("Missing value of (expected type {}).",
                                hjson_type_to_string(expected_type)));
    }
    return property;
}

Mg::Value parse_value(const Hjson::Value& value_property,
                      const shader::ParameterType expected_type,
                      const bool allow_undefined)
{
    const Value::Type expected_value_type = corresponding_value_type(expected_type);

    if (allow_undefined && !value_property.defined()) {
        return Mg::Value(expected_value_type);
    }

    auto [result, error_reason] = Mg::Value::from_hjson(value_property);
    if (!result) {
        throw RuntimeError{ "{} (Note: expected a value of type {})",
                            error_reason,
                            Value::type_to_string(expected_value_type) };
    }

    return result.value();
}

struct ParameterParseConfig {
    std::string value_property_key = "value";
    bool allow_undefined_parameter_value = false;
    bool require_sampler_texture_resource_id = false;
};

void parse_parameters(const ParameterParseConfig& config,
                      const Hjson::Value& parameters,
                      std::vector<ParameterDeclaration>& parameters_out,
                      std::vector<SamplerDeclaration>& samplers_out)
{
    MG_ASSERT(parameters.type() == Hjson::Type::Map);

    for (const auto& [parameter_name, parameter_properties] : parameters) {
        try {
            const Hjson::Value& type =
                get_property(parameter_properties, "type", Hjson::Type::String, true);

            const auto parameter_type = shader::string_to_parameter_type(type.to_string());
            const auto sampler_type = shader::string_to_sampler_type(type.to_string());

            const Hjson::Value& value_property = parameter_properties[config.value_property_key];

            if (parameter_type) {
                ParameterDeclaration& out = parameters_out.emplace_back();
                out.name = Identifier::from_runtime_string(parameter_name);
                out.type = *parameter_type;

                out.default_value = parse_value(value_property,
                                                *parameter_type,
                                                config.allow_undefined_parameter_value);
            }
            else if (sampler_type) {
                auto& out = samplers_out.emplace_back();
                out.name = Identifier::from_runtime_string(parameter_name);
                out.type = sampler_type.value();

                if (config.require_sampler_texture_resource_id) {
                    if (value_property.to_string().empty()) {
                        throw RuntimeError(
                            "Missing or invalid 'value' (expected texture resource id)");
                    }

                    out.texture_resource_id =
                        Identifier::from_runtime_string(value_property.to_string());
                }
            }
            else {
                throw RuntimeError("Unexpected type {}", type.to_string());
            }
        }
        catch (const std::exception& e) {
            throw RuntimeError("Error parsing parameter '{}': {}", parameter_name, e.what());
        }
        catch (const RuntimeError& e) {
            throw RuntimeError("Error parsing parameter '{}': {}", parameter_name, e.what());
        }
    }
}

void parse_options(const Hjson::Value& options, std::vector<OptionDeclaration>& options_out)
{
    MG_ASSERT(options.type() == Hjson::Type::Map);

    for (const auto& [option_name, option_default_enabled] : options) {
        if (option_default_enabled.type() != Hjson::Type::Bool) {
            throw RuntimeError("Expected boolean value.");
        }

        auto& out = options_out.emplace_back();
        out.name = Identifier::from_runtime_string(option_name);
        out.value = option_default_enabled.to_int64() != 0;
    }
}


shader::Tag parse_tags(const Hjson::Value& tags)
{
    MG_ASSERT(tags.type() == Hjson::Type::Vector);

    shader::Tag result;

    const auto len = tags.size();
    for (int i = 0; i < as<int>(len); ++i) {
        if (tags[i].type() != Hjson::Type::String) {
            throw RuntimeError("Unexpected type {} for element in tags.",
                               hjson_type_to_string(tags[i].type()));
        }
        const auto& tag_name = tags[i].to_string();
        Opt<shader::Tag> tag_value = shader::string_to_tag(tag_name);
        if (!tag_value) {
            throw RuntimeError("Unexpected tag {}.", tag_name);
        }
        result |= tag_value.value();
    }

    return result;
}

} // namespace

MaterialParseResult parse_material(std::string_view material_resource_definition)
{
    Hjson::Value root = Hjson::Unmarshal(material_resource_definition.data(),
                                         material_resource_definition.size());

    const Hjson::Value& parameters = get_property(root, "parameters", Hjson::Type::Map);
    const Hjson::Value& options = get_property(root, "options", Hjson::Type::Map);
    const Hjson::Value& blend_mode = get_property(root, "blend_mode", Hjson::Type::Map);

    MaterialParseResult result;

    if (blend_mode.defined()) {
        result.blend_mode = gfx::BlendMode::deserialize(root["blend_mode"]);
    }

    if (parameters.defined()) {
        parse_parameters(
            {
                .value_property_key = "value",
                .allow_undefined_parameter_value = false,
                .require_sampler_texture_resource_id = true,
            },
            parameters,
            result.parameters,
            result.samplers);
    }

    if (options.defined()) {
        parse_options(options, result.options);
    }

    result.shader_resource_id = Identifier::from_runtime_string(
        get_property(root, "shader", Hjson::Type::String).to_string());

    return result;
}

ShaderParseResult parse_shader(std::string_view shader_resource_definition)
{
    Hjson::Value root = Hjson::Unmarshal(shader_resource_definition.data(),
                                         shader_resource_definition.size());

    const Hjson::Value& vertex_code = get_property(root, "vertex_code", Hjson::Type::String);
    const Hjson::Value& fragment_code = get_property(root, "fragment_code", Hjson::Type::String);
    const Hjson::Value& parameters = get_property(root, "parameters", Hjson::Type::Map);
    const Hjson::Value& options = get_property(root, "options", Hjson::Type::Map);
    const Hjson::Value& tags = get_property(root, "tags", Hjson::Type::Vector);

    ShaderParseResult result;
    result.vertex_code = vertex_code.to_string();
    result.fragment_code = fragment_code.to_string();

    if (parameters.defined()) {
        parse_parameters(
            {
                .value_property_key = "default",
                .allow_undefined_parameter_value = true,
                .require_sampler_texture_resource_id = false,
            },
            parameters,
            result.parameters,
            result.samplers);
    }

    if (options.defined()) {
        parse_options(options, result.options);
    }

    if (tags.defined()) {
        parse_tags(tags);
    }

    return result;
}

} // namespace Mg::parser
