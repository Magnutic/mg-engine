//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
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
