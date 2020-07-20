//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_shader_parser.h
 * Parser for shader resource files.
 */

#pragma once

#include "mg/resources/mg_shader_resource.h"
#include "mg/utils/mg_gsl.h"

#include <string>
#include <string_view>
#include <vector>

namespace Mg::shader {

struct ParseResult {
    std::string vertex_code;
    std::string fragment_code;

    std::vector<shader::Sampler> samplers;
    std::vector<shader::Parameter> parameters;
    std::vector<shader::Option> options;

    shader::Tag::Value tags = {};
};

ParseResult parse_shader(std::string_view shader_resource_definition);

} // namespace Mg::shader
