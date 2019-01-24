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

/** @file mg_format_string.h
 * Type-safe printf-style formatting.
 */

#pragma once

#include <string>

#include <fmt/printf.h>

namespace Mg {

/** Constructs a string from a printf() style call in a type safe manner.
 * Handles most common printf() formatting.
 *
 * Compatibility wrapper, use fmt from now on.
 */
template<typename... Ts>
std::string format_string(const char* fmt, Ts&&... args) {
    return fmt::sprintf(fmt, std::forward<Ts>(args)...);
}

} // namespace Mg
