//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Magnus Bergsten
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

/** @file mg_file_time_helper.cpp
 * Helper functions to deal with portability issues with file time stamps.
 */

#include "mg/utils/mg_file_time_helper.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"

#ifdef _MSC_VER
#    define WIN32_LEAN_AND_MEAN
#    include "Windows.h"
#endif // _MSC_VER

namespace Mg {

// TODO: implement portably (probably only possible with C++20)

// Get last write time of file as time_t (more portable than the unspecified time type guaranteed by
// C++17 standard).
std::time_t last_write_time_t(const std::filesystem::path& file)
{
    const auto filetime = std::filesystem::last_write_time(file);

#ifdef _MSC_VER
    // For MSVC, manually convert to time_t.
    FILETIME   ft;
    SYSTEMTIME st;

    // filesystem time stamps are supposed to be binary compatible with Windows FILETIME objects in
    // MSVC stdlib.
    memcpy(&ft, &filetime, sizeof(FILETIME));

    if (FileTimeToSystemTime(&ft, &st) == 0) {
        g_log.write_error("Failed to convert file time stamp to 'time_t'.");
        throw RuntimeError{};
    }

    std::tm tm;
    tm.tm_sec   = st.wSecond;
    tm.tm_min   = st.wMinute;
    tm.tm_hour  = st.wHour;
    tm.tm_mday  = st.wDay;
    tm.tm_mon   = st.wMonth - 1;
    tm.tm_year  = st.wYear - 1900;
    tm.tm_isdst = -1;

    return std::mktime(&tm);
#else
    // Not guaranteed to be portable, but works with libstdc++ (GCC).
    return std::chrono::system_clock::to_time_t(filetime);
#endif // _MSC_VER
}

} // namespace Mg
