//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_file_io.h
 * More convenient and intuitive interface for iostreams.
 */

#pragma once

#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_optional.h"

#include <fstream>
#include <string>
#include <string_view>
#include <type_traits>

namespace Mg::io {

enum class Mode { text, binary };

/** Portably creates ofstream from UTF-8 filepath, with stream exceptions enabled. Returns nullopt
 * if creating the filestream fails.
 */
Opt<std::ofstream> make_output_filestream(std::string_view filepath, bool overwrite, Mode mode);

/** Portably creates ifstream from UTF-8 filepath, with stream exceptions enabled. Returns nullopt
 * if creating the filestream fails.
 */
Opt<std::ifstream> make_input_filestream(std::string_view filepath, Mode mode);

std::string get_all_text(std::istream& stream);

/** Read all lines from stream, ignoring empty lines. Ensures all lines end with '\n'. */
std::string get_all_lines(std::istream& stream);

/** Get the next line (characters from current read position until next '\n'). */
std::string get_line(std::istream& stream);

/** Get the next token (from current read position until first occurrence of any of the
 * characters in delims).
 */
std::string get_token(std::istream& stream, std::string_view delims);

char peek_char(std::istream& stream);

char get_char(std::istream& stream);

/** Write the supplied string to file, appending a newline if string does not already end with one.
 */
void write_line(std::ostream& stream, std::string_view string);

/** Get the size of the file contents in bytes (i.e. the index of the last byte). */
size_t file_size(std::istream& stream);

inline size_t position(std::istream& stream)
{
    return static_cast<size_t>(stream.tellg());
}

inline void set_position(std::istream& stream, size_t new_position)
{
    stream.seekg(new_position);
}

/** Read a value from the stream. It is the user's responsibility to avoid problems with alignment,
 * endianness, and padding bytes.
 * @param value_out Reference to target value.
 * @return Whether value was successfully read.
 */
template<typename T> void read_binary(std::istream& stream, T& value_out)
{
    static_assert(std::is_trivially_copyable<T>::value, "Target type is not trivially copyable.");
    // Note: reading directly into value_out is undefined behaviour according to the standard.
    // "Correct" method would be to first read to buffer of char, then memcpy to value_out.
    stream.read(reinterpret_cast<char*>(&value_out), sizeof(T));
}

/** Read an array of values from the file stream. It is the user's responsibility to avoid problems
 * with alignment, endianness, and padding bytes.
 * @param out span of values to fill.
 * @return Number of bytes that where successfully read.
 */
template<typename T> size_t read_binary_array(std::istream& stream, span<T> out)
{
    if (out.size() == 0) {
        return 0;
    }
    static_assert(std::is_trivially_copyable<T>::value, "Target type is not trivially copyable.");

    const auto before_position = position(stream);
    // Note: reading directly into out is undefined behaviour according to the standard.
    // "Correct" method would be to first read to buffer of char, then memcpy to value_out.
    stream.read(reinterpret_cast<char*>(out.data()), out.size_bytes());
    const auto after_position = position(stream);
    return (after_position - before_position);
}

/** Writes a value to the file stream. It is the user's responsibility to avoid problems with
 * alignment, endianness, and padding bytes.
 * @param value Reference to value to write.
 */
template<typename T> void write_binary(std::ostream& stream, const T& value)
{
    static_assert(std::is_trivially_copyable<T>::value, "Source type is not trivially copyable.");
    stream.write(reinterpret_cast<const char*>(value), sizeof(value));
}

/** Writes an array of values to the file stream. It is the user's responsibility to avoid
 * problems with alignment, endianness, and padding bytes.
 * @param values span of values to write.
 * @return Number of bytes that where successfully written.
 */
template<typename T> size_t write_binary_array(std::ostream& stream, const span<T> values)
{
    static_assert(std::is_trivially_copyable<T>::value, "Source type is not trivially copyable.");

    const auto before_position = stream.tellp();
    stream.write(reinterpret_cast<const char*>(values.data()), values.size_bytes());
    const auto after_position = stream.tellp();
    return static_cast<size_t>(after_position - before_position);
}

} // namespace Mg::io
