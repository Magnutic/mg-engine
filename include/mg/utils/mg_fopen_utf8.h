//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_fopen_utf8.h
 * Cross-platform UTF-8 fopen equivalent.
 */

#pragma once

#include <cstdio>
#include <string>
#include <string_view>

namespace Mg {

/** Cross-platform (Windows, Linux, probably OS X) fopen for UTF-8 file paths.  */
std::FILE* fopen_utf8(const char* filepath_utf8, const char* mode);

#ifdef _WIN32
#    ifdef _MSC_VER
std::wstring widen_if_msvc(std::string_view str);
#    else // Windows, non-MSVC compiler

/* Unsolved issue here - how to open files with unicode file paths on Windows? MSVC supports a
 * non-standard wstring (UTF-16) overload, but AFAIK other Windows compilers like MinGW does not.
 * After searching around a bit and reading some old mailing lists, it seems they are not interested
 * in solving this problem either.
 *
 * Until all Windows compilers support fopen with UTF-16 wstrings, or until Windows supports
 * UTF-8 strings in its API (even better), this problem seems to be largely unsolvable.
 *
 * UPDATE 2017: Now that filesystem is in C++17, fstream now takes filesystem::path() as a
 * parameter. I imagine MSVC will implement this to work correctly with UTF-8 paths. However, as of
 * writing, it seems libstdc++ has not yet implemented filesystem, so we will keep this solution
 * until then.
 */
#        error "No support for unicode file paths in fopen on the current platform."
#    endif // _MSC_VER
#else      // Not Windows
inline std::string widen_if_msvc(std::string_view str)
{
    return std::string(str);
}
#endif     // _WIN32

} // namespace Mg
