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

/** @file mg_macros.h
 * Useful pre-processor macros.
 * Usually, we try to avoid using macros. Sometimes, however, they are the best tool.
 */

#pragma once

/** 'Force' (more like strongly suggest) compiler to inline function.
 * Useful in some relatively obscure cases (particularly compile time string hashing, see
 * mg_identifier.h).
 */
#if defined _MSC_VER
#define MG_INLINE __forceinline
#elif defined __GNUC__ || defined __clang__
#define MG_INLINE __attribute__((always_inline))
#else
#define MG_INLINE inline
#endif

/** Unsigned integer overflow is not in itself an error, but it often indicates a bug, so we want to
 * be able to catch it with debugging tools. However, sometimes unsigned overflow is intentional.
 * Use MG_USES_UNSIGNED_OVERFLOW to mark a function as such, making it exempt from the checks.
 */
#if defined __clang__
#define MG_USES_UNSIGNED_OVERFLOW __attribute__((no_sanitize("unsigned-integer-overflow")))
#else
#define MG_USES_UNSIGNED_OVERFLOW
#endif

#ifndef _MSC_VER
/** Generates deleted move constructors and move assignment operators. */
#define MG_MAKE_NON_MOVABLE(class_name) \
    class_name(class_name&&) = delete;  \
    class_name& operator=(class_name&&) = delete;
#else
// Due to poor support for guaranteed copy elision in Visual Studio 15.7.5,
// we let the type still be movable there. Compiling on any other compiler
// will catch non-elided moves, still.
// TODO: remove this when MSVC is no longer broken.
#define MG_MAKE_NON_MOVABLE(class_name) MG_MAKE_DEFAULT_MOVABLE(class_name)
#endif

/** Generates deleted and copy constructors and copy assignment operators. */
#define MG_MAKE_NON_COPYABLE(class_name)    \
    class_name(const class_name&) = delete; \
    class_name& operator=(const class_name&) = delete;

/** Generate default move constructors and move assignment operators. */
#define MG_MAKE_DEFAULT_MOVABLE(class_name) \
    class_name(class_name&&) = default;     \
    class_name& operator=(class_name&&) = default;

/** Generate default copy constructors and copy assignment operators. */
#define MG_MAKE_DEFAULT_COPYABLE(class_name) \
    class_name(const class_name&) = default; \
    class_name& operator=(const class_name&) = default;

/** Define special member functions for virtual interfaces (defaulted + virtual destructor) */
#define MG_INTERFACE_BOILERPLATE(class_name)            \
    class_name()                  = default;            \
    class_name(const class_name&) = default;            \
    class_name& operator=(const class_name&) = default; \
    class_name(class_name&&)                 = default; \
    class_name& operator=(class_name&&) = default;      \
    virtual ~class_name() {}
