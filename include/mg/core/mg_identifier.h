//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_identifier.h
 * Identifier based on FNV-1a string hashing.
 * Usage: `Mg::Identifier id("String to hash");` or, if you just want the hash:
 * `uint32_t hash = Mg::hash_fnv1a("String to hash");`
 */

#pragma once

#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_u8string_casts.h"

#include <cstdint>
#include <string_view>
#include <type_traits>

/** Whether hash collisions should be logged whenever they are detected. Note that Mg::Identifier
 * works correctly even in the presence of hash collisions.
 */
#ifndef MG_IDENTIFIER_REPORT_HASH_COLLISION
#    define MG_IDENTIFIER_REPORT_HASH_COLLISION 1
#endif

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

class Identifier;

namespace literals {
MG_INLINE constexpr Identifier operator""_id(const char* str, size_t len);
MG_INLINE constexpr uint32_t operator""_hash(const char* str, size_t len)
{
    return hash_fnv1a({ str, len });
}
} // namespace literals

using namespace literals;

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
    MG_INLINE constexpr Identifier(const char (&str)[N]) // NOLINT(cppcoreguidelines-avoid-c-arrays)
        : Identifier(&str[0], hash_fnv1a({ &str[0], N - 1 }))
    {}

    /** Constructs an Identifier from a dynamic string: this is slower as it requires run-time
     * hashing and potentially storing a copy of the dynamic string.
     */
    static Identifier from_runtime_string(std::string_view str) { return { str }; }
    static Identifier from_runtime_string(std::u8string_view str)
    {
        return { cast_u8_to_char(str) };
    }

    /** Default constructor, empty string. */
    Identifier() : Identifier("") {}

    ~Identifier() = default;

    Identifier(const Identifier&) noexcept = default;

    Identifier& operator=(const Identifier&) noexcept = default;

    Identifier(Identifier&&) noexcept = default;

    Identifier& operator=(Identifier&&) noexcept = default;

    /** Returns the calculated hash value. */
    constexpr uint32_t hash() const noexcept { return m_hash; }

    /** Returns the full string from which this Identifier was created. */
    constexpr const char* c_str() const noexcept { return m_str; };

    /** Returns the full string from which this Identifier was created. */
    constexpr std::string_view str_view() const noexcept { return m_str; };

    /** Comparison functor for ordering by hash value. */
    class HashCompare {
    public:
        bool operator()(Identifier lhs, Identifier rhs) const noexcept
        {
            return lhs.hash() < rhs.hash();
        }
    };

    /** Comparison functor for ordering by hash value. */
    class LexicalCompare {
    public:
        bool operator()(Identifier lhs, Identifier rhs) const noexcept
        {
            return lhs.str_view() < rhs.str_view();
        }
    };

private:
    // Allow operator""_id to access private constructor.
    friend constexpr Identifier literals::operator""_id(const char* str, size_t len);

    // Construct from dynamic string (comparatively costly). Invoked by `from_runtime_string()`.
    Identifier(std::string_view str) : m_hash{ hash_fnv1a(str) } { set_full_string(str); }

    // Internal constructor.
    constexpr Identifier(const char* str, uint32_t hash) : m_str(str), m_hash(hash) {}

    void set_full_string(std::string_view str);

    const char* m_str{};
    uint32_t m_hash{};
};

namespace literals {
MG_INLINE constexpr Identifier operator""_id(const char* str, size_t len)
{
    return { str, hash_fnv1a({ str, len }) };
}
} // namespace literals

// Identifier should be trivially copyable (for performance reasons and to allow memcpy-aliasing).
static_assert(std::is_trivially_copyable_v<Identifier>);

namespace detail {

// Static storage for dynamic string copies
static struct StrMapInitializer {
    StrMapInitializer() noexcept;
    MG_MAKE_NON_MOVABLE(StrMapInitializer);
    MG_MAKE_NON_COPYABLE(StrMapInitializer);
    ~StrMapInitializer();
} str_map_initializer;

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
    const bool string_equal =
        (hash_equal ? (lhs.c_str() == rhs.c_str() || lhs.str_view() == rhs.str_view()) : false);

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

// We need to specialize std::hash in order to use Mg::Identifier in e.g. std::unordered_map. This
// just returns the pre-calculated hash value.
template<> struct hash<Mg::Identifier> {
    std::size_t operator()(const Mg::Identifier& rhs) const noexcept { return rhs.hash(); }
};

} // namespace std
