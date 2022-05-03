//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_material.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/core/mg_value.h"
#include "mg/mg_defs.h"
#include "mg/resource_cache/mg_resource_access_guard.h"
#include "mg/resources/mg_shader_resource.h"
#include "mg/utils/mg_stl_helpers.h"
#include "mg/utils/mg_string_utils.h"
#include "mg/utils/mg_vec_to_string.h"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <cstring> // memcpy
#include <sstream>
#include <type_traits>

namespace Mg::gfx {

namespace {

size_t num_elems_for_param_type(shader::ParameterType type) noexcept
{
    switch (type) {
    case shader::ParameterType::Vec4:
        return 4;
    case shader::ParameterType::Vec2:
        return 2;
    case shader::ParameterType::Float:
        return 1;
    case shader::ParameterType::Int:
        return 1;
    }

    MG_ASSERT(false && "unreachable");
}

} // namespace

Material::Material(Identifier material_id, ResourceHandle<ShaderResource> shader_resource)
    : m_shader_resource(shader_resource), m_id(material_id)
{
    ResourceAccessGuard shader_resource_access(m_shader_resource);

    MG_ASSERT(shader_resource_access->samplers().size() <= defs::k_max_samplers_per_material);
    MG_ASSERT(shader_resource_access->options().size() <= defs::k_max_options_per_material);

    uint32_t opt_index = 0;
    for (const shader::Option& o : shader_resource_access->options()) {
        m_options.push_back(o.name);
        m_option_flags |= (static_cast<uint32_t>(o.default_value) << opt_index);
        ++opt_index;
    }

    for (const shader::Parameter& p : shader_resource_access->parameters()) {
        m_params.push_back({ p.name, p.type });
        _set_parameter_impl(p.name, p.value, p.type);
    };

    for (const shader::Sampler& s : shader_resource_access->samplers()) {
        m_samplers.push_back({ s.name, s.type, {}, {} });
    }
}

void Material::set_sampler(Identifier name,
                           TextureHandle texture,
                           Opt<Identifier> texture_resource_id)
{
    auto opt_index = sampler_index(name);

    if (!opt_index.has_value()) {
        log.error("Material '{}': set_sampler(\"{}\", ...): shader has no such sampler.",
                  m_id.str_view(),
                  name.str_view());
        throw RuntimeError();
    }

    m_samplers[*opt_index].name = name;
    m_samplers[*opt_index].texture = texture;
    m_samplers[*opt_index].texture_resource_id = texture_resource_id;
}

void Material::set_option(Option option, bool enabled)
{
    auto [found, index] = index_of(m_options, option);

    if (!found) {
        log.error("Material '{}': set_option(\"{}\", ...): shader has no such option.",
                  m_id.str_view(),
                  option.str_view());
        throw RuntimeError();
    }

    m_option_flags.set(index, enabled);
}

bool Material::get_option(Option option) const
{
    auto [found, index] = index_of(m_options, option);

    if (!found) {
        log.error("Material '{}': get_option(\"{}\"): shader has no such option.",
                  m_id.str_view(),
                  option.str_view());
        throw RuntimeError();
    }

    return m_option_flags.test(index);
}

Opt<size_t> Material::sampler_index(Identifier name)
{
    for (size_t i = 0; i < m_samplers.size(); ++i) {
        auto&& sampler = m_samplers[i];
        if (sampler.name == name) {
            return i;
        }
    }
    return nullopt;
}

void Material::set_parameter(Identifier name, int param)
{
    _set_parameter_impl(name, byte_representation(param), shader::ParameterType::Int);
}
void Material::set_parameter(Identifier name, float param)
{
    _set_parameter_impl(name, byte_representation(param), shader::ParameterType::Float);
}
void Material::set_parameter(Identifier name, const glm::vec2& param)
{
    _set_parameter_impl(name, byte_representation(param), shader::ParameterType::Vec2);
}
void Material::set_parameter(Identifier name, const glm::vec4& param)
{
    _set_parameter_impl(name, byte_representation(param), shader::ParameterType::Vec4);
}

void Material::set_parameter(Identifier name, const Value& value)
{
    switch (value.type()) {
    case Value::Type::Int:
        set_parameter(name, value.as_int().value());
        break;

    case Value::Type::Float:
        set_parameter(name, value.as_float().value());
        break;

    case Value::Type::Vec2:
        set_parameter(name, value.as_vec2().value());
        break;

    case Value::Type::Vec4:
        set_parameter(name, value.as_vec4().value());
        break;

    default:
        log.error(
            "Material '{}': set_parameter(\"{}\", ...): Wrong type. Cannot use value of type {} as "
            "a material parameter.",
            id().c_str(),
            name.c_str(),
            value.type_as_string());
    }
}

Opt<int> Material::get_parameter_int(Identifier name) const
{
    auto v = _get_parameter_impl(name, shader::ParameterType::Int);
    return v ? make_opt(narrow_cast<int>(v.value()[0])) : nullopt;
}
Opt<float> Material::get_parameter_float(Identifier name) const
{
    auto v = _get_parameter_impl(name, shader::ParameterType::Float);
    return v ? make_opt(v.value()[0]) : nullopt;
}
Opt<glm::vec2> Material::get_parameter_vec2(Identifier name) const
{
    return glm::vec2(
        _get_parameter_impl(name, shader::ParameterType::Vec2).value_or(glm::vec4(0.0f)));
}
Opt<glm::vec4> Material::get_parameter_vec4(Identifier name) const
{
    return _get_parameter_impl(name, shader::ParameterType::Vec4);
}

Opt<shader::ParameterType> Material::get_parameter_type(Identifier name) const
{
    auto it = find_if(m_params, [&](auto p) { return p.name == name; });
    return it != m_params.end() ? make_opt(it->type) : nullopt;
}

namespace {

// How far to advance in to parameters buffer for each type.
size_t offset_for_param_type(shader::ParameterType type) noexcept
{
    switch (type) {
    case shader::ParameterType::Vec4:
        return 16;
    case shader::ParameterType::Vec2:
        return 8;
    case shader::ParameterType::Float:
        return 4;
    case shader::ParameterType::Int:
        return 4;
    }
    MG_ASSERT(false && "unreachable");
}

struct FindParameterT {
    const Material::Parameter* parameter;
    size_t offset;
};

FindParameterT find_parameter(Identifier name, const Material::Parameters& parameters)
{
    const Material::Parameter* parameter = nullptr;

    // Determine where in the buffer the parameter data should go.
    // N.B. offset calculation assumes that params are sorted in order for optimal packing (i.e.
    // vec4 -> vec2 -> float). This is done in ShaderResource.
    size_t offset = 0;
    for (auto&& p : parameters) {
        if (p.name == name) {
            parameter = &p;
            break;
        }

        offset += offset_for_param_type(p.type);
    }

    return { parameter, offset };
}

} // namespace

void Material::_set_parameter_impl(Identifier name,
                                   span<const std::byte> param_value,
                                   shader::ParameterType param_type)
{
    auto [p_param, offset] = find_parameter(name, m_params);

    if (p_param == nullptr) {
        log.warning("Material '{}': set_parameter(\"{}\", ...): shader has no such parameter.",
                    id().str_view(),
                    name.str_view());
        return;
    }

    if (p_param->type != param_type) {
        log.error(
            "Material '{}': set_parameter(\"{}\", ...): Wrong type. Parameter is {}, tried to "
            "assign {}.",
            id().c_str(),
            name.c_str(),
            shader::parameter_type_to_string(p_param->type),
            shader::parameter_type_to_string(param_type));
        return;
    }

    // Write data into material parameter buffer
    const auto size = 4 * num_elems_for_param_type(param_type);
    MG_ASSERT(offset + size <= m_parameter_data.size());
    MG_ASSERT(param_value.size_bytes() >= size);
    std::memcpy(&m_parameter_data[offset], param_value.data(), size);
}

Opt<glm::vec4> Material::_get_parameter_impl(Identifier name,
                                             shader::ParameterType param_type) const
{
    auto [p_param, offset] = find_parameter(name, m_params);
    glm::vec4 result = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (p_param == nullptr) {
        log.warning("Material '{}': get_parameter(\"{}\", ...): shader has no such parameter.",
                    id().str_view(),
                    name.str_view());
        return nullopt;
    }

    if (p_param->type != param_type) {
        log.error(
            "Material '{}': get_parameter(\"{}\", ...): Wrong type. Parameter is {}, tried to "
            "get as {}.",
            id().c_str(),
            name.c_str(),
            shader::parameter_type_to_string(p_param->type),
            shader::parameter_type_to_string(param_type));
        return nullopt;
    }

    // Copy data from material buffer.
    const auto size = 4 * num_elems_for_param_type(p_param->type);
    MG_ASSERT(offset + size <= m_parameter_data.size());
    MG_ASSERT(sizeof(result) >= size);
    std::memcpy(&result[0], &m_parameter_data[offset], size);
    static_assert(std::is_trivially_copyable_v<glm::vec4>);

    return result;
}

std::string Material::_parameter_value_as_string(Identifier name) const
{
    const Opt<shader::ParameterType> parameter_type = get_parameter_type(name).value();
    if (!parameter_type) {
        return "<N/A>";
    }

    switch (parameter_type.value()) {
    case shader::ParameterType::Int:
        return std::to_string(get_parameter_int(name).value());
    case shader::ParameterType::Float:
        return std::to_string(get_parameter_float(name).value());
    case shader::ParameterType::Vec2:
        return to_string(get_parameter_vec2(name).value(), true, false);
    case shader::ParameterType::Vec4:
        return to_string(get_parameter_vec4(name).value(), true, false);
    }

    MG_ASSERT(false && "unreachable");
}

Material::PipelineId Material::pipeline_identifier() const noexcept
{
    return { m_shader_resource.resource_id(), m_option_flags };
}

std::string Material::serialize() const
{
    std::ostringstream oss;
    oss << "// Material id: '" << id().c_str() << "'\n";
    oss << "shader = \"" << m_shader_resource.resource_id().str_view() << "\"\n\n";

    {
        WriteBracedBlock options_block(oss, "OPTIONS");
        for (const Option& o : options()) {
            options_block.writeLine(o.str_view(), " = ", (get_option(o) ? "true" : "false"), ';');
        }
    }

    oss << '\n';

    {
        WriteBracedBlock parameters_block(oss, "PARAMETERS");

        for (const Sampler& s : samplers()) {
            parameters_block.writeLine(shader::sampler_type_to_string(s.type),
                                       ' ',
                                       s.name.str_view(),
                                       " = \"",
                                       s.texture_resource_id.value_or("").str_view(),
                                       "\";");
        }

        parameters_block.writeLine();

        for (const Parameter& p : parameters()) {
            parameters_block.writeLine(shader::parameter_type_to_string(p.type),
                                       ' ',
                                       p.name.str_view(),
                                       " = ",
                                       _parameter_value_as_string(p.name),
                                       ';');
        }
    }

    return oss.str();
}

} // namespace Mg::gfx
