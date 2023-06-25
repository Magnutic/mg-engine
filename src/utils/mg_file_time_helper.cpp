//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_file_time_helper.cpp
 * Helper functions to deal with portability issues with file time stamps.
 */

#include "mg/utils/mg_file_time_helper.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/utils/mg_u8string_casts.h"

#include <fmt/core.h>

#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    include <Windows.h>
#else
#    include <sys/stat.h>
#    include <sys/types.h>
#    include <unistd.h>
#endif // _WIN32

namespace Mg {

namespace {
constexpr auto msg_failed_to_read = "Could not read time stamp of file '{}'";
[[maybe_unused]] constexpr auto msg_failed_to_convert = "Could not read time stamp of file '{}'";
} // namespace

// TODO C++20: Replace with using std::filesystem::last_write_time and std::chrono::clock_cast.

std::time_t last_write_time_t(const std::filesystem::path& file)
{
#ifdef _WIN32
    // For Windows, manually convert to time_t.
    HANDLE handle = CreateFileW(file.wstring().c_str(),
                                GENERIC_READ,
                                FILE_SHARE_READ,
                                nullptr,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    FILETIME ft;
    const bool gotTime = GetFileTime(handle, nullptr, nullptr, &ft);

    if (!gotTime) {
        throw RuntimeError{ msg_failed_to_read,
                            u8string_view_to_string_view(file.generic_u8string()) };
    }

    SYSTEMTIME st;
    if (FileTimeToSystemTime(&ft, &st) == 0) {
        log.error(msg_failed_to_convert);
        throw RuntimeError{};
    }

    std::tm tm;
    tm.tm_sec = st.wSecond;
    tm.tm_min = st.wMinute;
    tm.tm_hour = st.wHour;
    tm.tm_mday = st.wDay;
    tm.tm_mon = st.wMonth - 1;
    tm.tm_year = st.wYear - 1900;
    tm.tm_isdst = -1;

    return std::mktime(&tm);

#else
    // Try unix approach.
    struct stat result = {};
    if (stat(file.c_str(), &result) == 0) {
        return result.st_mtime;
    }

    throw RuntimeError{ msg_failed_to_read, cast_u8_to_char(file.generic_u8string()) };
#endif // _WIN32
}

} // namespace Mg
