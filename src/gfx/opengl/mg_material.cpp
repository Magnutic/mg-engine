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

#include "mg/gfx/mg_material.h"

#include <cstring> // memcpy
#include <sstream>

#include <fmt/core.h>

#include "mg/core/mg_log.h"
#include "mg/core/mg_resource_cache.h"
#include "mg/resources/mg_shader_resource.h"
#include "mg/utils/mg_hash_combine.h"
#include "mg/utils/mg_stl_helpers.h"

namespace Mg::gfx {

Material::Material(Identifier material_id, ResourceHandle<ShaderResource> shader)
    : m_id(material_id), m_shader(shader)
{
    auto shader_resource_access = shader.access();

    MG_ASSERT(shader_resource_access->samplers().size() <= defs::k_max_samplers_per_material);

    uint32_t opt_index = 0;
    for (const ShaderResource::Option& o : shader_resource_access->options()) {
        m_options.push_back(o.name);
        m_option_flags |= (uint32_t(o.default_value) << opt_index);
        ++opt_index;
    }

    for (const ShaderResource::Parameter& p : shader_resource_access->parameters()) {
        // TODO handle value
        m_params.push_back({ p.name, p.type });
        _set_parameter_impl(p.name, p.value, p.type);
    };

    for (const ShaderResource::Sampler& s : shader_resource_access->samplers()) {
        m_samplers.push_back({ s.name, s.type, {} });
    }
}

void Material::set_sampler(Identifier name, TextureHandle texture)
{
    auto opt_index = sampler_index(name);
    if (opt_index.has_value()) {
        auto index                = opt_index.value();
        m_samplers[index].name    = name;
        m_samplers[index].sampler = texture;
    }
    else {
        throw std::runtime_error(
            fmt::format("Material '{}': set_sampler(\"{}\", ...): no such sampler.",
                        m_id.c_str(),
                        name.c_str()));
    }
}

void Material::set_option(Identifier option, bool enabled)
{
    auto [found, index] = index_of(m_options, option);

    if (!found) {
        throw std::runtime_error(
            fmt::format("Material '{}': set_option(\"{}\", ...): no such option.",
                        m_id.c_str(),
                        option.c_str()));
    }

    if (enabled) { m_option_flags |= (1u << index); }
    else {
        m_option_flags &= ~(1u << index);
    }
}

bool Material::get_option(Identifier option) const
{
    auto [found, index] = index_of(m_options, option);

    if (!found) {
        throw std::runtime_error(fmt::format("Material '{}': get_option(\"{}\"): no such option.",
                                             m_id.c_str(),
                                             option.c_str()));
    }

    return (m_option_flags & (1u << index)) != 0;
}

std::optional<size_t> Material::sampler_index(Identifier name)
{
    for (size_t i = 0; i < m_samplers.size(); ++i) {
        auto&& sampler = m_samplers[i];
        if (sampler.name == name) { return i; }
    }
    return std::nullopt;
}

void Material::set_parameter(Identifier name, float param)
{
    _set_parameter_impl(name, glm::vec4(param), ShaderParameterType::Float);
}
void Material::set_parameter(Identifier name, glm::vec2 param)
{
    _set_parameter_impl(name, glm::vec4(param, 0.f, 0.f), ShaderParameterType::Vec2);
}
void Material::set_parameter(Identifier name, glm::vec4 param)
{
    _set_parameter_impl(name, param, ShaderParameterType::Vec4);
}

// How far to advance in to parameters buffer for each type.
static size_t offset_for_param_type(ShaderParameterType type)
{
    switch (type) {
    case ShaderParameterType::Vec4:
        return 16;
    case ShaderParameterType::Vec2:
        return 8;
    case ShaderParameterType::Float:
        return 4;
    }
    throw "unreachable";
}

static size_t num_elems_for_param_type(ShaderParameterType type)
{
    switch (type) {
    case ShaderParameterType::Vec4:
        return 4;
    case ShaderParameterType::Vec2:
        return 2;
    case ShaderParameterType::Float:
        return 1;
    }
    throw "unreachable";
}

// Print error message when the wrong type is passed into Material::set_parameter
static void wrong_type_error(Identifier          material_id,
                             Identifier          param_id,
                             ShaderParameterType expected,
                             ShaderParameterType actual)
{
    auto error_msg = fmt::format(
        "Material '{}': set_parameter(\"{}\", ...): wrong type, expected {}, got {}.",
        material_id.c_str(),
        param_id.c_str(),
        shader_parameter_type_to_string(expected),
        shader_parameter_type_to_string(actual));

    g_log.write_error(error_msg);
}

void Material::_set_parameter_impl(Identifier name, glm::vec4 param, ShaderParameterType enum_v)
{
    size_t     num_elems = num_elems_for_param_type(enum_v);
    Parameter* p_param   = nullptr;

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
        fmt::format("Material '{}': set_parameter(\"{}\", ...): no such parameter.",
                    id().c_str(),
                    name.c_str());
        return;
    }

    if (p_param->type != enum_v) {
        wrong_type_error(id(), p_param->name, p_param->type, enum_v);
        return;
    }

    // Write data into local buffer
    const auto size = num_elems * sizeof(float);
    MG_ASSERT(offset + size <= m_parameter_data.size());
    std::memcpy(&m_parameter_data.buffer[offset], &param, size);
}

uint32_t Material::shader_hash() const
{
    // TODO: this is not a good hash
    return hash_combine(m_shader.resource_id().hash(), m_option_flags);
}

std::string Material::debug_print() const
{
    std::ostringstream oss;
    oss << "Material '" << id() << "': {";

    oss << "\n\tShaderResource: '" << m_shader.resource_id() << "'\n";

    oss << "\n\tOptions: {";

    for (const Option& o : options()) { oss << "\n\t\t" << o << " = " << get_option(o); };

    oss << (options().empty() ? "}" : "\n\t}");

    oss << "\n\tSamplers: {";

    for (const Sampler& s : samplers()) {
        oss << "\n\t\t'" << s.name << "' : " << shader_sampler_type_to_string(s.type);
    }

    oss << (samplers().empty() ? "}" : "\n\t}");

    oss << "\n\tParameters: {";

    for (const Parameter& p : parameters()) {
        oss << "\n\t\t'" << p.name << "' : " << shader_parameter_type_to_string(p.type) << " = ";
        // TODO: fix
    }

    oss << (parameters().empty() ? "}" : "\n\t}");

    oss << "\n}";

    return oss.str();
}

std::string shader_interface_code(const Material& material)
{
    std::string snippet;
    snippet.reserve(256);

    // Include definition of each paramater
    snippet += "layout (std140) uniform MaterialParams {\n";
    for (const Material::Parameter& p : material.parameters()) {
        snippet += '\t';
        snippet += shader_parameter_type_to_string(p.type);
        snippet += " ";
        snippet += p.name.str_view();
        snippet += ";\n";
    }
    snippet += "} material_params;\n";

    // Include definition of each sampler
    for (const Material::Sampler& s : material.samplers()) {
        snippet += "uniform ";
        snippet += shader_sampler_type_to_string(s.type);
        snippet += " ";
        snippet += s.name.str_view();
        snippet += ";\n";
    }

    // Include pre-processor #defines for each enabled option
    for (const Material::Option& o : material.options()) {
        snippet += fmt::format("#define {} {:d}\n", o.c_str(), material.get_option(o));
    }

    return snippet;
}

} // namespace Mg::gfx
