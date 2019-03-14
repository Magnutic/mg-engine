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

/** @file mg_text_file.h
 * More convenient interface for iostreams.
 */

#pragma once

#include "mg/utils/mg_optional.h"

#include <fstream>
#include <string>
#include <string_view>

namespace Mg::io {

Opt<std::ofstream> make_output_filestream(std::string_view filepath, bool overwrite);

Opt<std::ifstream> make_input_filestream(std::string_view filepath);

std::string all_text(std::istream& stream);

std::string get_line(std::istream& stream);

std::string get_token(std::istream& stream, std::string_view delims);

char peek_char(std::istream& stream);

char get_char(std::istream& stream);

/** Write the supplied string to file, appending a newline if string does
 * not already end with one.
 */
void write_line(std::ostream& stream, std::string_view string);

} // namespace Mg::io
