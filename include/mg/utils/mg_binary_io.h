//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_binary_io.h
 * Binary file io utilities.
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <type_traits>

#include "mg/utils/mg_fopen_utf8.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"

namespace Mg {

namespace detail {
class BinaryFileHandler {
public:
    MG_MAKE_NON_COPYABLE(BinaryFileHandler);
    MG_MAKE_NON_MOVABLE(BinaryFileHandler);

    BinaryFileHandler() = default;
    ~BinaryFileHandler()
    {
        if (m_file) {
            std::fclose(m_file);
        }
    }

    /** Get whether we have reached the end of the file stream. */
    bool eof() const noexcept { return std::feof(m_file) != 0; }

    /** Returns the error code associated with the filestream. */
    int error_code() const noexcept { return std::ferror(m_file); }

    /** Get whether the file stream is open and in usable state. */
    operator bool() const noexcept { return good(); }

    /** Get whether the file stream is open and in usable state. */
    bool good() const noexcept { return m_file && error_code() == 0; }

    /** Get the current position in the file stream as byte index. */
    long pos() const noexcept { return std::ftell(m_file); }

    /** Set position in binary file stream.
     * @param pos The byte offset from file start to set as current position.
     * @return Whether position was successfully set.
     */
    bool set_offset(long pos) noexcept { return std::fseek(m_file, pos, SEEK_SET) != 0; }

protected:
    bool _open(std::string_view filepath, const char* openmode);

    std::FILE* m_file = nullptr;
};
} // namespace detail

//--------------------------------------------------------------------------------------------------

class BinaryFileReader : public detail::BinaryFileHandler {
public:
    BinaryFileReader() = default;

    BinaryFileReader(std::string_view filepath) { open(filepath); }

    bool open(std::string_view filepath) { return _open(filepath, "rb"); }

    /** Get the size of the file contents in bytes (i.e. the index of the
     * last byte.
     */
    size_t size() const noexcept;

    /** Read a value from the file stream. It is the user's responsibility to
     * avoid problems with alignment and endianness.
     * @param value_out Reference to target value.
     * @return Whether value was successfully read.
     */
    template<typename T> bool read(T& value_out) noexcept;

    /** Read an array of values from the file stream. It is the user's
     * responsibility to avoid problems with alignment and endianness.
     * @param out span of values to fill.
     * @return Number of values that where successfully read.
     */
    template<typename T> size_t read_array(span<T> out) noexcept;
};

//--------------------------------------------------------------------------------------------------

class BinaryFileWriter : public detail::BinaryFileHandler {
public:
    BinaryFileWriter() = default;

    BinaryFileWriter(std::string_view filepath, bool overwrite = true)
    {
        open(filepath, overwrite);
    }

    bool open(std::string_view filepath, bool overwrite = true);

    /** Writes a value to the file stream. It is the user's responsibility to
     * avoid problems with alignment and endianness.
     * @param value Reference to value to write.
     * @return Whether value was successfully written.
     */
    template<typename T> bool write(const T& value) noexcept;

    /** Writes an array of values to the file stream. It is the user's
     * responsibility to avoid problems with alignment and endianness.
     * @param values span of values to write.
     * @return Number of values that where successfully written.
     */
    template<typename T> size_t write_array(const span<T> values) noexcept;
};

//--------------------------------------------------------------------------------------------------
// Implementation
//--------------------------------------------------------------------------------------------------

inline bool detail::BinaryFileHandler::_open(std::string_view filepath, const char* openmode)
{
    m_file = fopen_utf8(std::string(filepath).c_str(), openmode);
    return m_file != nullptr;
}

//--------------------------------------------------------------------------------------------------

template<typename T> bool BinaryFileReader::read(T& value_out) noexcept
{
    static_assert(std::is_trivially_copyable<T>::value, "Target type is not trivially copyable.");
    return std::fread(&value_out, sizeof(T), 1, m_file) == 1;
}

template<typename T> size_t BinaryFileReader::read_array(span<T> out) noexcept
{
    if (out.size() == 0) {
        return 0;
    }
    static_assert(std::is_trivially_copyable<T>::value, "Target type is not trivially copyable.");
    const auto size_read = std::fread(&out[0], sizeof(T), out.size(), m_file);
    return size_read;
}

inline size_t BinaryFileReader::size() const noexcept
{
    const auto cur_pos = pos();
    std::fseek(m_file, 0, SEEK_END);
    const auto ret_val = pos();
    std::fseek(m_file, cur_pos, SEEK_SET);
    return narrow<size_t>(ret_val);
}

//--------------------------------------------------------------------------------------------------

inline bool BinaryFileWriter::open(std::string_view filepath, bool overwrite)
{
    if (overwrite) {
        return _open(filepath, "wb");
    }
    return _open(filepath, "wb+");
}

template<typename T> bool BinaryFileWriter::write(const T& value) noexcept
{
    static_assert(std::is_trivially_copyable<T>::value, "Source type is not trivially copyable.");
    return std::fwrite(&value, sizeof(T), 1, m_file) == 1;
}

template<typename T> size_t BinaryFileWriter::write_array(const span<T> values) noexcept
{
    static_assert(std::is_trivially_copyable<T>::value, "Source type is not trivially copyable.");
    auto size_written = std::fwrite(&values[0], sizeof(T), values.size(), m_file);
    return size_written;
}

} // namespace Mg
