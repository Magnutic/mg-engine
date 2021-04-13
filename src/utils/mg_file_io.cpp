//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/utils/mg_file_io.h"

#include <filesystem>

namespace Mg::io {

Opt<std::ofstream> make_output_filestream(std::string_view filepath, bool overwrite, Mode mode)
{
    std::ios::openmode open_mode = std::ios::out;
    open_mode |= overwrite ? std::ios::trunc : std::ios::app;
    if (mode == Mode::binary) {
        open_mode |= std::ios::binary;
    }

    auto writer = make_opt<std::ofstream>(std::filesystem::u8path(filepath), open_mode);

    if (!writer->good()) {
        return {};
    }

    writer->exceptions(std::ios::badbit | std::ios::failbit);
    return writer;
}

Opt<std::ifstream> make_input_filestream(std::string_view filepath, Mode mode)
{
    std::ios::openmode open_mode = std::ios::in;
    if (mode == Mode::binary) {
        open_mode |= std::ios::binary;
    }

    auto writer = make_opt<std::ifstream>(std::filesystem::u8path(filepath), open_mode);

    if (!writer->good()) {
        return {};
    }

    writer->exceptions(std::ios::badbit | std::ios::failbit);
    return writer;
}

std::string get_all_text(std::istream& stream)
{
    std::string file_text;

    while (peek_char(stream) != 0) {
        const auto c = get_char(stream);
        file_text += c;
    }

    return file_text;
}

std::string get_all_lines(std::istream& stream)
{
    std::string file_text;

    while (!stream.eof()) {
        file_text += get_line(stream) + "\n";
    }

    return file_text;
}

std::string get_line(std::istream& stream)
{
    std::string line;
    line.reserve(80);

    while (peek_char(stream) != 0) {
        const auto c = get_char(stream);
        if (c == '\n') {
            break;
        }
        line += c;
    }

    return line;
}

std::string get_token(std::istream& stream, std::string_view delims)
{
    if (!stream.good()) {
        return {};
    }

    const auto is_delim = [&](const char c) { return delims.find(c) != std::string_view::npos; };

    std::string token;

    // Discard preceding delimiters
    while (is_delim(peek_char(stream)) && !stream.eof()) {
        get_char(stream);
    }

    // Read token until we hit a delimiter
    while (is_delim(peek_char(stream)) && !stream.eof()) {
        token += get_char(stream);
    }

    return token;
}

char peek_char(std::istream& stream)
{
    const int value = stream.peek();

    if (value == std::char_traits<char>::eof()) {
        return '\0';
    }

    return narrow_cast<char>(value);
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

    if (string.empty() || string[string.size() - 1] != '\n') {
        stream << "\n";
    }
}

size_t file_size(std::istream& stream)
{
    const auto current_pos = stream.tellg();
    stream.seekg(0, std::ios::end);
    const auto result = stream.tellg();
    stream.seekg(current_pos);
    return static_cast<size_t>(result);
}

} // namespace Mg::io
