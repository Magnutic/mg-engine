//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Magnus Bergsten
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

/** @file mg_shader_parser.h
 * Parser for shader resource files.
 */

#pragma once

#include "mg/resources/mg_shader_types.h"
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
