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

/** @file mg_max.h
 * std::max()-equivalent.
 */

#pragma once

namespace Mg {

/** Equivalent to std::max. Re-implemented here because std::max resides in the very heavy
 * <algorithm> header, which is too much to include for such a small function.
 */
template<typename T> constexpr const T& max(const T& l, const T& r) noexcept
{
    return l > r ? l : r;
}

} // namespace Mg
