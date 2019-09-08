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

#include "mg/utils/mg_fopen_utf8.h"

#include "mg/utils/mg_gsl.h"

// TODO: investigate replacements for codecvt, which is deprecated in C++17.
#include <codecvt>
#include <locale>

namespace Mg {

#ifdef _WIN32

// Convert UTF-8 string to wstring.
static std::wstring utf8_to_wstring(std::string_view str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv("");
    const span<const char>                           bytes{ str.data(), str.size() };
    return myconv.from_bytes(bytes.begin(), bytes.end());
}

// Widens a UTF-8 encoded std::string to std::wstring for UTF-16 file open MSVC extension.
std::wstring widen_if_msvc(std::string_view str)
{
    return utf8_to_wstring(str);
}

// Widens a UTF-8 encoded string to std::wstring and uses the _wfopen() MSVC extension.
std::FILE* fopen_utf8(const char* filepath_utf8, const char* mode)
{
    return _wfopen(utf8_to_wstring(filepath_utf8).data(), utf8_to_wstring(mode).data());
}

#else // Not Windows

// Regular fopen (UTF-8 should be supported)
std::FILE* fopen_utf8(const char* filepath_utf8, const char* mode)
{
    return fopen(filepath_utf8, mode);
}

#endif // _WIN32

} // namespace Mg
