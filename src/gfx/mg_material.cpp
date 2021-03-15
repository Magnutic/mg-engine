//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_material.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/mg_defs.h"
#include "mg/resource_cache/mg_resource_access_guard.h"
#include "mg/utils/mg_stl_helpers.h"

#include <fmt/core.h>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <cstring> // memcpy
#include <sstream>

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

Material::Material(Identifier material_id, ResourceHandle<ShaderResource> shader)
    : m_shader(shader), m_id(material_id)
{
    ResourceAccessGuard shader_resource_access(shader);

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
        m_samplers.push_back({ s.name, s.type, {} });
    }
}

void Material::set_sampler(Identifier name, TextureHandle texture)
{
    auto opt_index = sampler_index(name);

    if (!opt_index.has_value()) {
        log.error("Material '{}': set_sampler(\"{}\", ...): no such sampler.",
                  m_id.str_view(),
                  name.str_view());
        throw RuntimeError();
    }

    m_samplers[*opt_index].name = name;
    m_samplers[*opt_index].sampler = texture;
}

void Material::set_option(Identifier option, bool enabled)
{
    auto [found, index] = index_of(m_options, option);

    if (!found) {
        log.error("Material '{}': set_option(\"{}\", ...): no such option.",
                  m_id.str_view(),
                  option.str_view());
        throw RuntimeError();
    }

    m_option_flags.set(index, enabled);
}

bool Material::get_option(Identifier option) const
{
    auto [found, index] = index_of(m_options, option);

    if (!found) {
        log.error("Material '{}': get_option(\"{}\"): no such option.",
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

// Print error message when the wrong type is passed into Material::set_parameter
void wrong_type_error(Identifier material_id,
                      Identifier param_id,
                      shader::ParameterType expected,
                      shader::ParameterType actual)
{
    auto error_msg =
        fmt::format("Material '{}': set_parameter(\"{}\", ...): wrong type, expected {}, got {}.",
                    material_id.c_str(),
                    param_id.c_str(),
                    shader::parameter_type_to_string(expected),
                    shader::parameter_type_to_string(actual));

    log.error(error_msg);
}

} // namespace

void Material::_set_parameter_impl(Identifier name,
                                   span<const std::byte> param_value,
                                   shader::ParameterType param_type)
{
    Parameter* p_param = nullptr;

    // Determine where in the buffer the parameter data should go.
    // N.B. offset calculation assumes that params are sorted in order for optimal packing (i.e.
    // vec4 -> vec2 -> float). This is done in ShaderResource.
    size_t offset = 0;
    for (auto&& p : m_params) {
        if (p.name == name) {
            p_param = &p;
            break;
        }

        offset += offset_for_param_type(p.type);
    }

    if (p_param == nullptr) {
        log.warning(fmt::format("Material '{}': set_parameter(\"{}\", ...): no such parameter.",
                                id().c_str(),
                                name.c_str()));
        return;
    }

    if (p_param->type != param_type) {
        wrong_type_error(id(), p_param->name, p_param->type, param_type);
        return;
    }

    // Write data into local buffer
    const auto size = 4 * num_elems_for_param_type(param_type);
    MG_ASSERT(offset + size <= m_parameter_data.size());
    MG_ASSERT(param_value.size_bytes() >= size);
    std::memcpy(&m_parameter_data.buffer[offset], param_value.data(), size);
}

Material::PipelineId Material::pipeline_identifier() const noexcept
{
    return { m_shader.resource_id(), m_option_flags };
}

std::string Material::debug_print() const
{
    std::ostringstream oss;
    oss << "Material '" << id().c_str() << "': {";

    oss << "\n\tShaderResource: '" << m_shader.resource_id().c_str() << "'\n";

    oss << "\n\tOptions: {";

    for (const Option& o : options()) {
        oss << "\n\t\t" << o.c_str() << " = " << get_option(o);
    };

    oss << (options().empty() ? "}" : "\n\t}");

    oss << "\n\tSamplers: {";

    for (const Sampler& s : samplers()) {
        oss << "\n\t\t'" << s.name.c_str() << "' : " << shader::sampler_type_to_string(s.type);
    }

    oss << (samplers().empty() ? "}" : "\n\t}");

    oss << "\n\tParameters: {";

    for (const Parameter& p : parameters()) {
        oss << "\n\t\t'" << p.name.c_str() << "' : " << shader::parameter_type_to_string(p.type)
            << " = ";
        // TODO: fix
    }

    oss << (parameters().empty() ? "}" : "\n\t}");

    oss << "\n}";

    return oss.str();
}

} // namespace Mg::gfx
