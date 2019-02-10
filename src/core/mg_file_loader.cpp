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

// A note on path formats:
// On POSIX systems like Linux, paths are represented in UTF-8 with forward slash as path separator.
// On Windows, paths are represented in UTF-16 with backslash as path separator.
//
// Mg Engine uses file paths as hashed identifiers for resource files. This means that the path
// representation used internally must be strictly consistent. For this reason, Mg Engine uses paths
// following the POSIX convention, what the C++ filesystem library calls 'generic format'.
//
// File paths found from directory using the standard filesystem library are converted to UTF-8
// strings with the std::filesystem::path::generic_u8string() function, which should return the path
// in 'generic' POSIX format regardless of platform.
//
// As for files loaded from zip archives, the zip standard states that path separators shall be
// forward slashes and that file names should be encoded according to 'IBM Code Page 437' or UTF-8,
// see: https://pkware.cachefly.net/webdocs/APPNOTE/APPNOTE-6.3.5.TXT
//
// From what I hear, though, it seems as though many programs insert filenames in whichever encoding
// they wish into zip archives.
// libzip's 'zip_get_name' function returns UTF-8 by default, converted from a guessed encoding.
//
// For the sake of predictability, try to ensure that the tools that create the zip archives store
// the data in UTF-8.

#include "mg/core/mg_file_loader.h"

#include <filesystem>
#include <stdexcept>

#include <zip.h>

#include <fmt/core.h>

#include "mg/core/mg_log.h"
#include "mg/utils/mg_binary_io.h"

namespace Mg {

namespace fs = std::filesystem;

//--------------------------------------------------------------------------------------------------
// Direct file loading
//--------------------------------------------------------------------------------------------------

std::vector<FileRecord> BasicFileLoader::available_files()
{
    std::vector<FileRecord> index;

    fs::path root_dir{ m_directory };
    auto     search_options = fs::directory_options::follow_directory_symlink;

    for (auto& file : fs::recursive_directory_iterator{ root_dir, search_options }) {
        if (fs::is_directory(file)) { continue; }

        time_point last_write_time = fs::last_write_time(file.path());

        // Get path relative to this loader's root directory.
        // Using generic path format: this is to ensure that files are given string-identical
        // identifiers on Windows, irrespective of whether they are read from zip-file or from
        // directory.
        // N.B. MSVC implementation of generic_u8string is documented as converting backslash path
        // separators to forward slashes.
        std::string filepath = file.path().generic_u8string();
        filepath.erase(0, root_dir.generic_u8string().length() + 1);

        index.push_back({ Identifier::from_runtime_string(filepath), last_write_time });
    }

    return index;
}

bool BasicFileLoader::file_exists(Identifier file)
{
    auto     fname     = file.str_view();
    fs::path file_path = fs::u8path(m_directory) / fs::u8path(fname.begin(), fname.end());
    return fs::exists(file_path);
}

uintmax_t BasicFileLoader::file_size(Identifier file)
{
    MG_ASSERT(file_exists(file));

    auto     fname     = file.str_view();
    fs::path file_path = fs::u8path(m_directory) / fs::u8path(fname.begin(), fname.end());
    return fs::file_size(file_path);
}

time_point BasicFileLoader::file_time_stamp(Identifier file)
{
    MG_ASSERT(file_exists(file));

    auto     fname     = file.str_view();
    fs::path file_path = fs::u8path(m_directory) / fs::u8path(fname.begin(), fname.end());
    return fs::last_write_time(file_path);
}

void BasicFileLoader::load_file(Identifier file, span<std::byte> target_buffer)
{
    MG_ASSERT(target_buffer.size() >= file_size(file));

    g_log.write_verbose(fmt::format("BasicFileLoader::load_file(): loading '{}'...", file.c_str()));

    auto fname = file.str_view();
    auto path  = fs::u8path(m_directory) / fs::u8path(fname.begin(), fname.end());

    BinaryFileReader reader{ path.u8string() };

    if (!reader.good()) {
        throw std::runtime_error(fmt::format("Could not read file '{}'", path.c_str()));
    }

    auto size = reader.size();

    // Point index_data to buffer
    auto bytes_read = reader.read_array(target_buffer);

    MG_ASSERT(bytes_read == size);
    MG_ASSERT(bytes_read <= target_buffer.size());
}

//--------------------------------------------------------------------------------------------------
// Zip archive loading
//--------------------------------------------------------------------------------------------------

/** (Re-) open the zip archive for this reader. */
void ZipFileLoader::open_zip_archive()
{
    if (m_archive_file != nullptr) {
        auto error = zip_get_error(m_archive_file);
        if (error->sys_err == 0 && error->zip_err == 0) { return; }

        // If something is wrong, try to close and re-open archive
        close_zip_archive();
    }

    int zip_error  = 0;
    m_archive_file = zip_open(m_archive_name.data(), ZIP_RDONLY, &zip_error);

    if (m_archive_file == nullptr) {
        zip_error_t error{};
        zip_error_init_with_code(&error, zip_error);

        std::string msg = fmt::format(
            "Failed to open archive '{}': {}", m_archive_name, zip_error_strerror(&error));

        zip_error_fini(&error);

        throw std::runtime_error(msg);
    }
}

void ZipFileLoader::close_zip_archive()
{
    if (m_archive_file != nullptr) {
        zip_close(m_archive_file);
        m_archive_file = nullptr;
    }
}

std::vector<FileRecord> ZipFileLoader::available_files()
{
    open_zip_archive();
    auto num_files = zip_get_num_entries(m_archive_file, 0);

    std::vector<FileRecord> index;
    index.reserve(narrow<size_t>(num_files));

    for (uint32_t i = 0; i < num_files; ++i) {
        const char* filename = zip_get_name(m_archive_file, i, 0);

        time_point last_write_time;

        struct zip_stat stat = {};
        zip_stat(m_archive_file, filename, 0, &stat);

        if ((stat.valid & ZIP_STAT_MTIME) != 0) {
            last_write_time = std::chrono::system_clock::from_time_t(stat.mtime);
        }

        index.push_back({ Identifier::from_runtime_string(filename), last_write_time });
    }

    return index;
}

bool ZipFileLoader::file_exists(Identifier file)
{
    open_zip_archive();
    struct zip_stat sb {};
    int             result = zip_stat(m_archive_file, file.c_str(), 0, &sb);

    return (result != -1);
}

struct zip_stat zip_stat_helper(zip_t* archive, Identifier file_path)
{
    struct zip_stat sb {};

    if (zip_stat(archive, file_path.c_str(), 0, &sb) != -1) { return sb; }

    throw std::runtime_error(
        fmt::format("ZipFileLoader::file_size(): Could not find file '{}'", file_path.c_str()));
}

uintmax_t ZipFileLoader::file_size(Identifier file)
{
    MG_ASSERT(file_exists(file));
    open_zip_archive();

    auto sb = zip_stat_helper(m_archive_file, file);

    if ((sb.valid & ZIP_STAT_SIZE) == 0) {
        throw std::runtime_error(
            fmt::format("ZipFileLoader::file_size(): "
                        "Could not read size of file '{}' in zip archive '{}'",
                        file.c_str(),
                        m_archive_name));
    }

    return sb.size;
}

time_point ZipFileLoader::file_time_stamp(Identifier file)
{
    MG_ASSERT(file_exists(file));
    open_zip_archive();

    auto sb = zip_stat_helper(m_archive_file, file);
    if ((sb.valid & ZIP_STAT_MTIME) == 0) {
        throw std::runtime_error(
            fmt::format("ZipFileLoader::file_time_stamp(): "
                        "Could not read time stamp of file '{}' in zip archive '{}'",
                        file.c_str(),
                        m_archive_name));
    }

    return std::chrono::system_clock::from_time_t(sb.mtime);
}

// RAII wrapper, automatically closes zipfile on scope exit
static auto make_zip_handle(zip_file_t* p)
{
    return std::unique_ptr<zip_file_t, decltype(&zip_fclose)>{ p, &zip_fclose };
}

void ZipFileLoader::load_file(Identifier file, span<std::byte> target_buffer)
{
    MG_ASSERT(file_size(file) <= target_buffer.size());

    if (target_buffer.empty()) {
        return; // Ignore empty files.
    }

    // Make sure archive is open for reading
    open_zip_archive();

    auto index = zip_name_locate(m_archive_file, file.c_str(), ZIP_FL_NOCASE);

    auto error_throw = [&](std::string reason) {
        throw std::runtime_error(fmt::format("Could not read file '{}' from archive '{}': {}",
                                             file.c_str(),
                                             m_archive_name,
                                             reason));
    };

    if (index == -1) { error_throw("could not find file in archive"); }

    auto uindex = narrow<uint64_t>(index);

    // Get file size
    struct zip_stat file_info = {};
    zip_stat_index(m_archive_file, uindex, 0, &file_info);

    // Check for errors
    if ((file_info.valid & ZIP_STAT_SIZE) == 0) { error_throw("could not read file size"); }

    // Open file within archive
    auto zip_file = make_zip_handle(zip_fopen_index(m_archive_file, uindex, 0));

    // Check for errors
    if (zip_file == nullptr) {
        zip_error_t error{};
        zip_error_init(&error);
        std::string msg = zip_error_strerror(&error);
        zip_error_fini(&error);

        error_throw(msg);
    }

    // Read data from file
    auto bytes_read = zip_fread(zip_file.get(), target_buffer.data(), file_info.size);

    if (bytes_read == -1) { error_throw("could not read data"); }

    MG_ASSERT(size_t(bytes_read) <= target_buffer.size());
}

} // namespace Mg
