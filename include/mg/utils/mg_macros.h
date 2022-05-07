//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_macros.h
 * Useful pre-processor macros.
 * Usually, we try to avoid using macros. Sometimes, however, they are the best tool.
 */

#pragma once

#include <type_traits>

/** 'Force' (more like strongly suggest) compiler to inline function.
 * Useful in some relatively obscure cases (particularly compile time string hashing, see
 * mg_identifier.h).
 */
#if defined _MSC_VER
#    define MG_INLINE __forceinline
#elif defined __GNUC__ || defined __clang__
#    define MG_INLINE __attribute__((always_inline))
#else
#    define MG_INLINE inline
#endif

/** Unsigned integer overflow is not in itself an error, but it often indicates a bug, so we want to
 * be able to catch it with debugging tools. However, sometimes unsigned overflow is intentional.
 * Use MG_USES_UNSIGNED_OVERFLOW to mark a function as such, making it exempt from the checks.
 */
#if defined __clang__
#    define MG_USES_UNSIGNED_OVERFLOW __attribute__((no_sanitize("unsigned-integer-overflow")))
#else
#    define MG_USES_UNSIGNED_OVERFLOW
#endif

#ifndef _MSC_VER
/** Generates deleted move constructors and move assignment operators. */
#    define MG_MAKE_NON_MOVABLE(class_name) \
        class_name(class_name&&) = delete;  \
        class_name& operator=(class_name&&) = delete;
#else
// Due to poor support for guaranteed copy elision in Visual Studio 15.7.5,
// we let the type still be movable there. Compiling on any other compiler
// will catch non-elided moves, still.
// TODO: remove this when MSVC is no longer broken.
#    define MG_MAKE_NON_MOVABLE(class_name) MG_MAKE_DEFAULT_MOVABLE(class_name)
#endif

/** Generates deleted and copy constructors and copy assignment operators. */
#define MG_MAKE_NON_COPYABLE(class_name)    \
    class_name(const class_name&) = delete; \
    class_name& operator=(const class_name&) = delete;

/** Generate default move constructors and move assignment operators. */
#define MG_MAKE_DEFAULT_MOVABLE(class_name)      \
    class_name(class_name&&) noexcept = default; \
    class_name& operator=(class_name&&) noexcept = default;

/** Generate default copy constructors and copy assignment operators. */
#define MG_MAKE_DEFAULT_COPYABLE(class_name) \
    class_name(const class_name&) = default; \
    class_name& operator=(const class_name&) = default;

/** Define special member functions for virtual interfaces */
#define MG_INTERFACE_BOILERPLATE(class_name)           \
    class_name() = default;                            \
    class_name(const class_name&) = delete;            \
    class_name& operator=(const class_name&) = delete; \
    class_name(class_name&&) = delete;                 \
    class_name& operator=(class_name&&) = delete;      \
    virtual ~class_name() {}

/** Generate the operators needed to use a scoped enum as a set of bit flags. */
#define MG_DEFINE_BITMASK_OPERATORS(EnumT)                                                 \
    inline EnumT operator&(EnumT l, EnumT r)                                               \
    {                                                                                      \
        return static_cast<EnumT>(std::underlying_type_t<EnumT>(l) &                       \
                                  std::underlying_type_t<EnumT>(r));                       \
    }                                                                                      \
    inline EnumT operator|(EnumT l, EnumT r)                                               \
    {                                                                                      \
        return static_cast<EnumT>(std::underlying_type_t<EnumT>(l) |                       \
                                  std::underlying_type_t<EnumT>(r));                       \
    }                                                                                      \
    inline EnumT operator^(EnumT l, EnumT r)                                               \
    {                                                                                      \
        return static_cast<EnumT>(std::underlying_type_t<EnumT>(l) ^                       \
                                  std::underlying_type_t<EnumT>(r));                       \
    }                                                                                      \
    inline EnumT operator~(EnumT v)                                                        \
    {                                                                                      \
        return static_cast<EnumT>(~std::underlying_type_t<EnumT>(v));                      \
    }                                                                                      \
    inline EnumT& operator&=(EnumT& l, EnumT r)                                            \
    {                                                                                      \
        l = l & r;                                                                         \
        return l;                                                                          \
    }                                                                                      \
    inline EnumT& operator|=(EnumT& l, EnumT r)                                            \
    {                                                                                      \
        l = l | r;                                                                         \
        return l;                                                                          \
    }                                                                                      \
    inline EnumT& operator^=(EnumT& l, EnumT r)                                            \
    {                                                                                      \
        l = l ^ r;                                                                         \
        return l;                                                                          \
    }                                                                                      \
    inline bool operator==(EnumT l, std::underlying_type_t<EnumT> r)                       \
    {                                                                                      \
        return std::underlying_type_t<EnumT>(l) == r;                                      \
    }                                                                                      \
    inline bool operator!=(EnumT l, std::underlying_type_t<EnumT> r) { return !(l == r); } \
    inline bool operator==(std::underlying_type_t<EnumT> l, EnumT r) { return r == l; }    \
    inline bool operator!=(std::underlying_type_t<EnumT> l, EnumT r) { return !(r == l); }
