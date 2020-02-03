//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_shader_parser_token.h"

#include <vector>

namespace Mg::shader {

std::vector<Token> lex_shader_definition(std::string_view shader_resource_definition);

} // namespace Mg::shader
