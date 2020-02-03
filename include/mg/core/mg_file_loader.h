//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_file_loader.h
 * Functionality for loading resource files into memory.
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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


/** Record representing a single file available in a IFileLoader. */
struct FileRecord {
    Identifier name{ "" };
    std::time_t time_stamp{};
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
    virtual std::time_t file_time_stamp(Identifier file) = 0;

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

    std::time_t file_time_stamp(Identifier file) override;

    void load_file(Identifier file, span<std::byte> target_buffer) override;

    std::string_view name() const override { return m_directory; }

private:
    std::string m_directory;
};

/** Loads files from a zip archive. */
class ZipFileLoader final : public IFileLoader {
public:
    explicit ZipFileLoader(std::string_view archive) : m_archive_name(archive) {}

    MG_MAKE_DEFAULT_COPYABLE(ZipFileLoader);
    MG_MAKE_DEFAULT_MOVABLE(ZipFileLoader);

    ~ZipFileLoader() { close_zip_archive(); }

    Array<FileRecord> available_files() override;

    bool file_exists(Identifier file) override;

    uintmax_t file_size(Identifier file) override;

    std::time_t file_time_stamp(Identifier file) override;

    void load_file(Identifier file, span<std::byte> target_buffer) override;

    std::string_view name() const override { return m_archive_name; }

private:
    void open_zip_archive();
    void close_zip_archive() noexcept;

    std::string m_archive_name;
    zip_t* m_archive_file = nullptr;
};

} // namespace Mg
