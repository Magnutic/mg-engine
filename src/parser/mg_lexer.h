//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_parser_token.h"

#include <vector>
#include <string_view>

namespace Mg::parser {

std::vector<Token> lex_definition(std::string_view definition);

} // namespace Mg::parser
