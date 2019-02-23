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

/** @file mg_file_loader.h
 * Functionality for loading resource files into memory.
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "mg/containers/mg_array.h"
#include "mg/core/mg_identifier.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"

// Forward declaration of libzip struct
struct zip;
using zip_t = struct zip;

namespace Mg {

// N.B. this assumes that std::filesystem::file_time_type is a typedef of
// std::chrono::system_clock::time_point. I believe this is the case on most platforms -
// probably on all desktop OS platforms, I am guessing that the under-specification is to
// support arbitrary file systems on e.g. embedded systems.
using time_point = std::chrono::system_clock::time_point;

/** Record representing a single file available in a IFileLoader. */
struct FileRecord {
    Identifier name{ "" };
    time_point time_stamp;
};

/** Interface for loading files from some source (e.g. directory, zip-archive, ...). */
class IFileLoader {
public:
    MG_INTERFACE_BOILERPLATE(IFileLoader);

    // TODO: TOCTOU?
    virtual Array<FileRecord> available_files() = 0;

    virtual bool file_exists(Identifier file) = 0;

    /** Returns file size in bytes of the file. */
    virtual uintmax_t file_size(Identifier file) = 0;

    /** Get the last-modified time stamp of the file. */
    virtual time_point file_time_stamp(Identifier file) = 0;

    /** Load file. Throws if file is not available. */
    virtual void load_file(Identifier file, span<std::byte> target_buffer) = 0;

    /** Returns a human-readable identifier for this file loader, e.g. path of directory or name of
     * zip archive. Mainly intended for logging.
     */
    virtual std::string_view name() const = 0;
};

/** Loads files directly from directory. */
class BasicFileLoader final : public IFileLoader {
public:
    explicit BasicFileLoader(std::string_view directory) : m_directory(directory) {}

    Array<FileRecord> available_files() override;

    bool file_exists(Identifier file) override;

    uintmax_t file_size(Identifier file) override;

    time_point file_time_stamp(Identifier file) override;

    void load_file(Identifier file, span<std::byte> target_buffer) override;

    std::string_view name() const override { return m_directory; }

private:
    std::string m_directory;
};

/** Loads files from a zip archive. */
class ZipFileLoader final : public IFileLoader {
public:
    explicit ZipFileLoader(std::string_view archive) : m_archive_name(archive) {}

    ~ZipFileLoader() { close_zip_archive(); }

    Array<FileRecord> available_files() override;

    bool file_exists(Identifier file) override;

    uintmax_t file_size(Identifier file) override;

    time_point file_time_stamp(Identifier file) override;

    void load_file(Identifier file, span<std::byte> target_buffer) override;

    std::string_view name() const override { return m_archive_name; }

private:
    void open_zip_archive();
    void close_zip_archive();

    std::string m_archive_name;
    zip_t*      m_archive_file = nullptr;
};

} // namespace Mg
