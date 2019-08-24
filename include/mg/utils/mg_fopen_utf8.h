//******************************************************************************
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
#ifdef _MSC_VER
std::wstring widen_if_msvc(std::string_view str);
#else // Windows, non-MSVC compiler

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
#error "No support for unicode file paths in fopen on the current platform."
#endif // _MSC_VER
#else  // Not Windows
inline std::string widen_if_msvc(std::string_view str)
{
    return std::string(str);
}
#endif // _WIN32

} // namespace Mg
