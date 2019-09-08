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

    span<const shader::Sampler>   samplers() const noexcept { return m_samplers; }
    span<const shader::Parameter> parameters() const noexcept { return m_parameters; }
    span<const shader::Option>    options() const noexcept { return m_options; }

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
    std::vector<shader::Sampler>   m_samplers;
    std::vector<shader::Option>    m_options;

    std::string m_vertex_code;
    std::string m_fragment_code;

    shader::Tag::Value m_tags = 0;
};

} // namespace Mg
