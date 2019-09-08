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

/** @file mg_identifier.h
 * Identifier based on FNV-1a string hashing.
 * Usage: `Mg::Identifier id("String to hash");` or, if you just want the hash:
 * `uint32_t hash = Mg::hash_fnv1a("String to hash");`
 */

#pragma once

#include "mg/utils/mg_macros.h"

#include <cstdint>
#include <string_view>
#include <type_traits>

/** Whether hash collisions should be logged whenever they are detected. Note that Mg::Identifier
 * works correctly even in the presence of hash collisions.
 */
#ifndef MG_IDENTIFIER_REPORT_HASH_COLLISION
#    define MG_IDENTIFIER_REPORT_HASH_COLLISION 1
#endif

namespace Mg::detail {

// Providing both template and constexpr function hashing implementations since the template
// solution seems more likely to actually result in compile-time hashing.

/** Hash string literal using FNV1a algorithm. Template implementation to enable compile-time
 * hashing.
 * @tparam N length of string
 */
template<uint32_t N, uint32_t I = N - 1> struct FnvHash {
    MG_INLINE MG_USES_UNSIGNED_OVERFLOW static constexpr uint32_t invoke(const char (&str)[N])
    {
        return (FnvHash<N, I - 1>::invoke(str) ^ static_cast<uint32_t>(str[I - 1])) * 16777619u;
    }
};

template<uint32_t N> struct FnvHash<N, 0> {
    MG_INLINE MG_USES_UNSIGNED_OVERFLOW static constexpr uint32_t invoke(const char (&)[N])
    {
        return 2166136261u;
    }
};

} // namespace Mg::detail

namespace Mg {

/** Hash string using FNV1a algorithm. */
MG_INLINE MG_USES_UNSIGNED_OVERFLOW constexpr uint32_t hash_fnv1a(std::string_view str)
{
    uint32_t hash = 2166136261u;

    for (const char c : str) {
        hash ^= static_cast<uint32_t>(c);
        hash *= 16777619u;
    }

    return hash;
}

/** Identifier class that is more efficient than using strings for certain purposes (e.g. hashmap
 * key). Identifier objects contain only a 32-bit hash of the string from which they were created
 * and a char pointer to the original string.
 *
 * The string hashing does not guarantee the absence of collisions, but collisions are correctly
 * handled in the sense that comparisons will not consider Identifiers with the same hash but
 * created from different strings to be the same (at the cost of some overhead), and in that the
 * c_str() and str_view() member functions will return the correct strings.
 */
class Identifier {
public:
    /** Construct an Identifier from a string literal. */
    template<unsigned int N>
    MG_INLINE constexpr Identifier(const char (&str)[N])
        : m_str(str), m_hash{ detail::FnvHash<N>::invoke(str) }
    {}

    /** Constructs an Identifier from a dynamic string: this is slower as it requires run-time
     * hashing and potentially storing a copy of the dynamic string.
     */
    static Identifier from_runtime_string(std::string_view str) { return Identifier(str); }

    Identifier(const Identifier& rhs) noexcept = default;

    Identifier& operator=(const Identifier& rhs) noexcept = default;

    /** Returns the calculated hash value. */
    constexpr uint32_t hash() const noexcept { return m_hash; }

    /** Returns the full string from which this Identifier was created. */
    constexpr const char* c_str() const noexcept { return m_str; };

    /** Returns the full string from which this Identifier was created. */
    constexpr std::string_view str_view() const noexcept { return m_str; };

private:
    // Construct from dynamic string (comparatively costly). Invoked by `from_runtime_string()`.
    Identifier(std::string_view str) : m_hash{ hash_fnv1a(str) } { set_full_string(str); }

    void set_full_string(std::string_view str);

    const char* m_str;
    uint32_t    m_hash;
};

// Identifier should be trivially copyable (for performance reasons and to allow memcpy-aliasing).
static_assert(std::is_trivially_copyable_v<Identifier>);

namespace detail {

// Static storage for dynamic string copies
static struct StrMapInitialiser {
    StrMapInitialiser() noexcept;
    MG_MAKE_NON_MOVABLE(StrMapInitialiser);
    MG_MAKE_NON_COPYABLE(StrMapInitialiser);
    ~StrMapInitialiser();
} str_map_initialiser;

void report_hash_collision(std::string_view first, std::string_view second);

} // namespace detail

//--------------------------------------------------------------------------------------------------

inline bool operator==(const Identifier& lhs, const Identifier& rhs)
{
    const bool hash_equal = lhs.hash() == rhs.hash();

    // Note the comparison of C-string pointers: the pointers themselves are being compared. This is
    // intentional: if the Identifiers were created from the same string literal or were both
    // created at run-time, then the pointers would refer to the same address.
    // Thus, the second half of the comparison (actual string comparison) only has to be run in the
    // relatively uncommon case of two identical compile-time strings which were not merged by the
    // compiler or linker.
    const bool string_equal = (hash_equal ? (lhs.c_str() == rhs.c_str() ||
                                             lhs.str_view() == rhs.str_view())
                                          : false);

#if MG_IDENTIFIER_REPORT_HASH_COLLISION
    if (hash_equal != string_equal) {
        detail::report_hash_collision(lhs.str_view(), rhs.str_view());
    }
#endif

    return string_equal;
}

inline bool operator!=(const Identifier& lhs, const Identifier& rhs)
{
    return !(lhs == rhs);
}

} // namespace Mg

namespace std {

// We need to specialise std::hash in order to use Mg::Identifier in e.g. std::unordered_map. This
// just returns the pre-calculated hash value.
template<> struct hash<Mg::Identifier> {
    std::size_t operator()(const Mg::Identifier& rhs) const noexcept { return rhs.hash(); }
};

} // namespace std
