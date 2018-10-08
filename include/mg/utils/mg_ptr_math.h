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

/** @file mg_ptr_math.h
 * Pointer arithmetic helpers.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace Mg {
namespace ptr_math {

/** Get the pointer value as integer */
inline intptr_t as_int(void* ptr)
{
    return reinterpret_cast<intptr_t>(ptr);
}

/** Get the pointer value as integer */
inline intptr_t as_int(const void* ptr)
{
    return reinterpret_cast<intptr_t>(ptr);
}

/** Get the pointer value as unsigned integer */
inline uintptr_t as_uint(void* ptr)
{
    return reinterpret_cast<uintptr_t>(ptr);
}

/** Get the pointer value as unsigned integer */
inline uintptr_t as_uint(const void* ptr)
{
    return reinterpret_cast<uintptr_t>(ptr);
}

/** Get pointer with offset of term bytes after ptr */
template<typename IntT> void* add(void* ptr, IntT term)
{
    return static_cast<char*>(ptr) + static_cast<intptr_t>(term);
}

/** Get pointer with offset of term bytes before ptr */
template<typename IntT> inline void* subtract(void* ptr, IntT term)
{
    return static_cast<char*>(ptr) - static_cast<intptr_t>(term);
}

/** Get pointer to the address with the given alignment that is closest to ptr,
 * but greater than or equal to ptr. */
inline void* align(void* ptr, size_t alignment)
{
    return reinterpret_cast<void*>((as_uint(ptr) + alignment - 1) & ~(alignment - 1));
}

/** Get pointer to the address with the given alignment that is closest to ptr,
 * but smaller than or equal to ptr. */
inline void* align_backward(void* ptr, size_t alignment)
{
    return reinterpret_cast<void*>(as_uint(ptr) & ~(alignment - 1));
}

/** Get pointer to address which has both the specified alignment, and can fit
 * a prefix of type PrefixT in between ptr and the return value. */
template<typename PrefixT> void* align_and_fit_prefix(void* ptr, size_t alignment)
{
    alignment = (alignment > alignof(PrefixT)) ? alignment : alignof(PrefixT);
    return align(static_cast<char*>(ptr) + sizeof(PrefixT), alignment);
}

/** Get the difference between two pointers in number of bytes. */
inline ptrdiff_t byte_difference(const void* lhs, const void* rhs)
{
    return reinterpret_cast<const char*>(lhs) - reinterpret_cast<const char*>(rhs);
}

inline bool is_aligned(void* ptr, size_t alignment)
{
    return (as_uint(ptr) & ~(alignment - 1)) == as_uint(ptr);
}
} // namespace ptr_math
} // namespace Mg
