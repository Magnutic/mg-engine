//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
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
