//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_material_resource.h
 *
 */

#pragma once

#include "mg/core/mg_value.h"
#include "mg/resource_cache/mg_base_resource.h"
#include "mg/resource_cache/mg_resource_handle.h"
#include "mg/resources/mg_shader_resource.h"
#include "mg/utils/mg_gsl.h"

#include <memory>
#include <string>
#include <vector>

namespace Mg {

class TextureResource;

namespace material {

struct Sampler {
    Identifier name;
    shader::SamplerType type{};
    Identifier texture_resource_id;
};

struct Parameter {
    Identifier name;
    shader::ParameterType type{};
    Mg::Value value;
};

struct Option {
    Identifier name{ "" };
    bool default_value = false;
};

} // namespace material

class MaterialResource : public BaseResource {
public:
    using BaseResource::BaseResource;

    span<const material::Sampler> samplers() const noexcept { return m_samplers; }
    span<const material::Parameter> parameters() const noexcept { return m_parameters; }
    span<const material::Option> options() const noexcept { return m_options; }
    ResourceHandle<ShaderResource> shader_resource() const noexcept { return m_shader_resource; }

    bool should_reload_on_file_change() const override { return true; }

    Identifier type_id() const override { return "MaterialResource"; }

    std::string serialize() const;

protected:
    LoadResourceResult load_resource_impl(ResourceLoadingInput& input) override;

private:
    std::vector<material::Parameter> m_parameters;
    std::vector<material::Sampler> m_samplers;
    std::vector<material::Option> m_options;
    ResourceHandle<ShaderResource> m_shader_resource;
};

} // namespace Mg
