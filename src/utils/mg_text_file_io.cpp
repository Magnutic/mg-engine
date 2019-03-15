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

#include "mg/utils/mg_text_file_io.h"

#include "mg/utils/mg_fopen_utf8.h"
#include "mg/utils/mg_stl_helpers.h"
#include "mg/utils/mg_string_utils.h"

namespace Mg::io {

// The fstream open functions below use  widen_if_msvc() to enable MSVC's non-standard
// std::wstring-taking constructor. Attempting to open a UTF-8 filepath via the standard
// std::string-taking constructor fails on Windows; using UTF-16 wstring is the only solution (until
// all standard library implementations provide fstream constructors taking filesystem::path).

Opt<std::ofstream> make_output_filestream(std::string_view filepath, bool overwrite)
{
    std::ios::openmode mode = std::ios::out;
    mode |= overwrite ? std::ios::trunc : std::ios::app;

    auto writer = make_opt<std::ofstream>(widen_if_msvc(filepath), mode);

    if (!writer->good()) { return {}; }

    return writer;
}

Opt<std::ifstream> make_input_filestream(std::string_view filepath)
{
    auto writer = make_opt<std::ifstream>(widen_if_msvc(filepath));

    if (!writer->good()) { return {}; }

    return writer;
}

std::string all_text(std::istream& stream)
{
    std::string file_text;
    file_text.reserve(512);

    while (!stream.eof()) { file_text += get_line(stream) + "\n"; }

    return file_text;
}

std::string get_line(std::istream& stream)
{
    std::string line;
    line.reserve(80);

    while (peek_char(stream) != 0) {
        auto c = get_char(stream);
        if (c == '\n') { break; }
        line += c;
    }

    return line;
}

std::string get_token(std::istream& stream, std::string_view delims)
{
    if (!stream.good()) { return {}; }

    auto is_delim = [&](char c) { return find(delims, c) != delims.end(); };

    std::string token;

    // Discard preceding delimiters
    while (is_delim(peek_char(stream)) && !stream.eof()) { get_char(stream); }

    // Read token until we hit a delimiter
    while (is_delim(peek_char(stream)) && !stream.eof()) { token += get_char(stream); }

    return token;
}

char peek_char(std::istream& stream)
{
    int value = stream.peek();

    if (value == std::char_traits<char>::eof()) { return '\0'; }

    return static_cast<char>(value);
}

char get_char(std::istream& stream)
{
    char c = '\0';
    stream.get(c);
    return c;
}

void write_line(std::ostream& stream, std::string_view string)
{
    stream << string;

    if (string.empty() || string[string.size() - 1] != '\n') { stream << "\n"; }
}

} // namespace Mg::io
