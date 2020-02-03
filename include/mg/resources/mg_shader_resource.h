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
#include "mg/resources/mg_shader_types.h"
#include "mg/utils/mg_gsl.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

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
