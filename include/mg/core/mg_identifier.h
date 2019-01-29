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

/** @file mg_string_hash.h
 * Identifier based on FNV-1a String hashing.
 * Usage: Mg::Identifier id("String to hash"); or if you just want the hash
 * uint32_t hash = Mg::hash_fnv1a("String to hash");
 */

#pragma once

#include <cstdint>
#include <iosfwd>
#include <string_view>

#include "mg/utils/mg_macros.h"

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
 * N.B. the string hashing does not guarantee the absence of collisions.
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
    uint32_t hash() const noexcept { return m_hash; }

    /** Returns the full string from which this Identifier was created. */
    const char* c_str() const noexcept { return m_str; };

    /** Returns the full string from which this Identifier was created. */
    std::string_view str_view() const noexcept { return m_str; };

private:
    // Construct from dynamic string (comparatively costly). Invoked by `from_runtime_string()`.
    Identifier(std::string_view str) : m_hash{ hash_fnv1a(str) } { set_full_string(str); }

    void set_full_string(std::string_view str);

    const char* m_str;
    uint32_t    m_hash; // Hash value
};

// Identifier should be trivially copyable (for performance reasons and to allow memcpy-aliasing).
static_assert(std::is_trivially_copyable_v<Identifier>);

//--------------------------------------------------------------------------------------------------
// Static storage for dynamic string copies
//--------------------------------------------------------------------------------------------------

namespace detail {

static struct StrMapInitialiser {
    StrMapInitialiser();
    ~StrMapInitialiser();
} str_map_initialiser;
} // namespace detail

//--------------------------------------------------------------------------------------------------

inline bool operator==(const Identifier& lhs, const Identifier& rhs)
{
    return lhs.hash() == rhs.hash();
}

inline bool operator!=(const Identifier& lhs, const Identifier& rhs)
{
    return !(lhs == rhs);
}

//--------------------------------------------------------------------------------------------------
// Standard library support
//--------------------------------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& out, const Identifier&);

} // namespace Mg

namespace std {

// We need to specialise std::hash in order to use Mg::Identifier in e.g.
// std::unordered_map. This just returns the pre-calculated hash value.
template<> struct hash<Mg::Identifier> {
    std::size_t operator()(const Mg::Identifier& rhs) const noexcept { return rhs.hash(); }
};

} // namespace std
