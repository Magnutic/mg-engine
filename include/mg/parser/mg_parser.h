//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_parser.h
 * Parsers for resource declarations and files.
 */

#pragma once

#include "mg/core/mg_value.h"
#include "mg/gfx/mg_blend_modes.h"
#include "mg/gfx/mg_shader_related_types.h"
#include "mg/utils/mg_gsl.h"

#include <glm/fwd.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace Mg::parser {

struct SamplerDeclaration {
    Identifier name;
    shader::SamplerType type{};
    Opt<Identifier> texture_resource_id;
};

struct ParameterDeclaration {
    Identifier name;
    shader::ParameterType type{};
    Mg::Value default_value;
};

struct OptionDeclaration {
    Identifier name;
    bool value{};
};

struct MaterialParseResult {
    gfx::BlendMode blend_mode = gfx::blend_mode_constants::bm_default;

    std::vector<SamplerDeclaration> samplers;
    std::vector<ParameterDeclaration> parameters;
    std::vector<OptionDeclaration> options;

    Identifier shader_resource_id;
};

MaterialParseResult parse_material(std::string_view material_resource_definition);

struct ShaderParseResult {
    std::string vertex_code;
    std::string fragment_code;

    std::vector<SamplerDeclaration> samplers;
    std::vector<ParameterDeclaration> parameters;
    std::vector<OptionDeclaration> options;

    shader::Tag tags = {};
};

ShaderParseResult parse_shader(std::string_view shader_resource_definition);

} // namespace Mg::parser
