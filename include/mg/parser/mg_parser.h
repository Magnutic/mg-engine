//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_parser.h
 * Parsers for resource declarations and files.
 */

#pragma once

#include "mg/resources/mg_shader_resource.h"
#include "mg/utils/mg_gsl.h"

#include <glm/fwd.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace Mg::parser {

glm::vec2 parse_vec2(std::string_view definition);

glm::vec3 parse_vec3(std::string_view definition);

glm::vec4 parse_vec4(std::string_view definition);

struct ShaderParseResult {
    std::string vertex_code;
    std::string fragment_code;

    std::vector<shader::Sampler> samplers;
    std::vector<shader::Parameter> parameters;
    std::vector<shader::Option> options;

    shader::Tag::Value tags = {};
};

ShaderParseResult parse_shader(std::string_view shader_resource_definition);

} // namespace Mg::parser
