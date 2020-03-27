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

#include <fmt/core.h>

#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    include "Windows.h"
#endif // _WIN32

namespace Mg {

// TODO C++20: Replace with using std::filesystem::last_write_time and std::chrono::clock_cast.

// Get last write time of file as time_t (more portable than the unspecified time type guaranteed by
// C++17 standard).
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
        const auto msg = fmt::format("Could not read time stamp of file '{}'",
                                     file.generic_u8string());
        g_log.write_error(msg);
        throw RuntimeError{};
    }

    SYSTEMTIME st;
    if (FileTimeToSystemTime(&ft, &st) == 0) {
        g_log.write_error("Failed to convert file time stamp to 'time_t'.");
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
    // Not guaranteed to be portable, but works with libstdc++ (GCC).
    const auto filetime = std::filesystem::last_write_time(file);
    return std::chrono::system_clock::to_time_t(filetime);
#endif // _WIN32
}

} // namespace Mg
