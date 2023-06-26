//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_material_resource.h
 *
 */

#pragma once

#include "mg/core/mg_value.h"
#include "mg/gfx/mg_shader_related_types.h"
#include "mg/resource_cache/mg_base_resource.h"
#include "mg/resource_cache/mg_resource_handle.h"
#include "mg/utils/mg_gsl.h"

#include "mg/parser/mg_parser.h"

#include <memory>
#include <string>
#include <vector>

namespace Mg {

class ShaderResource;
class TextureResource;

class MaterialResource : public BaseResource {
public:
    using BaseResource::BaseResource;

    gfx::BlendMode blend_mode() const noexcept { return m_blend_mode; }

    std::span<const parser::SamplerDeclaration> samplers() const noexcept { return m_samplers; }
    std::span<const parser::ParameterDeclaration> parameters() const noexcept { return m_parameters; }
    std::span<const parser::OptionDeclaration> options() const noexcept { return m_options; }
    ResourceHandle<ShaderResource> shader_resource() const noexcept { return m_shader_resource; }

    bool should_reload_on_file_change() const override { return true; }

    Identifier type_id() const override { return "MaterialResource"; }

protected:
    LoadResourceResult load_resource_impl(ResourceLoadingInput& input) override;

private:
    gfx::BlendMode m_blend_mode;
    std::vector<parser::ParameterDeclaration> m_parameters;
    std::vector<parser::SamplerDeclaration> m_samplers;
    std::vector<parser::OptionDeclaration> m_options;
    ResourceHandle<ShaderResource> m_shader_resource;
};

} // namespace Mg
