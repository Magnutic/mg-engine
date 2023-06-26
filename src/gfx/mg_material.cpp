//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_material.h"

#include "hjson/hjson.h"
#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/core/mg_value.h"
#include "mg/gfx/mg_shader_related_types.h"
#include "mg/gfx/mg_texture2d.h"
#include "mg/gfx/mg_texture_pool.h"
#include "mg/mg_defs.h"
#include "mg/resource_cache/mg_resource_access_guard.h"
#include "mg/resources/mg_shader_resource.h"
#include "mg/utils/mg_stl_helpers.h"
#include "mg/utils/mg_string_utils.h"
#include "mg/utils/mg_to_hjson.h"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <cstring> // memcpy
#include <type_traits>

namespace Mg::gfx {

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

void Material::set_sampler(Identifier sampler_name, const Texture2D* texture)
{
    auto opt_index = sampler_index(sampler_name);
    if (!opt_index.has_value()) {
        throw RuntimeError{ "Material '{}': set_sampler(\"{}\", ...): shader has no such sampler.",
                            m_id.str_view(),
                            sampler_name.str_view() };
    }

    auto& sampler = m_samplers[*opt_index];

    if (texture) {
        sampler.texture_id = texture->id();
        sampler.texture = texture->handle();
    }
    else {
        sampler.texture_id = "";
        sampler.texture = {};
    }
}

void Material::set_option(Option option, bool enabled)
{
    auto [found, index] = index_of(m_options, option);

    if (!found) {
        throw RuntimeError{ "Material '{}': set_option(\"{}\", ...): shader has no such option.",
                            m_id.str_view(),
                            option.str_view() };
    }

    m_option_flags.set(index, enabled);
}

bool Material::get_option(Option option) const
{
    auto [found, index] = index_of(m_options, option);

    if (!found) {
        throw RuntimeError{ "Material '{}': get_option(\"{}\"): shader has no such option.",
                            m_id.str_view(),
                            option.str_view() };
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
    case Value::Type::Bool:
        [[fallthrough]];

    case Value::Type::Int64:
        set_parameter(name, value.as_int().value());
        break;

    case Value::Type::Double:
        set_parameter(name, value.as_float().value());
        break;

    case Value::Type::Vec2:
        set_parameter(name, value.as_vec2().value());
        break;

    case Value::Type::Vec3:
        set_parameter(name, glm::vec4(value.as_vec3().value(), 0.0f));
        break;

    case Value::Type::Vec4:
        set_parameter(name, value.as_vec4().value());
        break;
    }
}

Opt<Value> Material::get_parameter(Identifier name) const
{
    const auto opt_data = _extract_parameter_data(name);
    if (!opt_data) {
        return nullopt;
    }

    const auto [data_as_vec4, parameter_type] = *opt_data;
    switch (parameter_type) {
    case shader::ParameterType::Int:
        return Value(narrow_cast<int64_t>(data_as_vec4[0]));

    case shader::ParameterType::Float:
        return Value(static_cast<double>(data_as_vec4[0]));

    case shader::ParameterType::Vec2:
        return Value(glm::vec2(data_as_vec4));

    case shader::ParameterType::Vec4:
        return Value(data_as_vec4);
    }

    MG_ASSERT(false && "unreachable");
}

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
                                   std::span<const std::byte> param_value,
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

Opt<std::pair<glm::vec4, shader::ParameterType>>
Material::_extract_parameter_data(Identifier name) const
{
    auto [p_param, offset] = find_parameter(name, m_params);
    glm::vec4 result = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (p_param == nullptr) {
        log.warning("Material '{}': get_parameter(\"{}\", ...): shader has no such parameter.",
                    id().str_view(),
                    name.str_view());
        return nullopt;
    }

    // Copy data from material buffer.
    const auto size = 4 * num_elems_for_param_type(p_param->type);
    MG_ASSERT(offset + size <= m_parameter_data.size());
    MG_ASSERT(sizeof(result) >= size);
    std::memcpy(&result[0], &m_parameter_data[offset], size);
    static_assert(std::is_trivially_copyable_v<glm::vec4>);

    return std::pair{ result, p_param->type };
}

Material::PipelineId Material::pipeline_identifier() const noexcept
{
    return { m_shader_resource.resource_id(), m_option_flags };
}

std::string Material::serialize() const
{
    Hjson::Value root;
    root.set_comment_before(fmt::format("// Material id: '{}'\n", id().str_view()));

    root["shader"] = m_shader_resource.resource_id().c_str();

    root["blend_mode"] = BlendMode::serialize(blend_mode);

    for (const Option& o : options()) {
        root["options"][o.c_str()] = get_option(o);
    }

    for (const Sampler& s : samplers()) {
        Hjson::Value param_value_map;
        param_value_map["type"] = std::string(shader::sampler_type_to_string(s.type));
        param_value_map["value"] = s.texture_id.c_str();
        root["parameters"][s.name.c_str()] = param_value_map;
    }

    for (const Parameter& p : parameters()) {
        Hjson::Value param_value_map;
        param_value_map["type"] = std::string(shader::parameter_type_to_string(p.type));
        param_value_map["value"] = get_parameter(p.name).value().to_hjson();
        root["parameters"][p.name.c_str()] = param_value_map;
    }

    return Hjson::Marshal(root, { .omitRootBraces = true });
}

} // namespace Mg::gfx
