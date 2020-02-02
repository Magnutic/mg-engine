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

#include "mg/core/mg_identifier.h"

#include "mg/core/mg_log.h"
#include "mg/utils/mg_assert.h"

#include <fmt/core.h>

#include <forward_list>
#include <mutex>
#include <type_traits>
#include <unordered_map>

namespace Mg {

namespace {

//--------------------------------------------------------------------------------------------------
// Nifty counter-based lifetime management for dynamic-string-copy map.
// Guarantees initialisation before first use, even during static initialisation of other objects.
// (This is the same pattern used to initialise std::cout, etc.)
//--------------------------------------------------------------------------------------------------

// Strings are stored in linked list, with a map from hash to iterator pair of strings matching that
// hash. This means that the strings will never move, and thus pointers to string data will not be
// invalidated, while also supporting multiple strings with the same hash.
//
// It is also a fairly inefficient storage method, and should perhaps be replaced with something
// leaner, if need arises. Probably store string data using a bump allocator along with an auxiliary
// hashmap to look up strings by hash.

using StringList = std::forward_list<std::string>;

// Stores the string(s) matching a given hash. In the vast majority of cases, there should be only
// one element, but in the case of a hash collision, there may be more.
class PerHashStrings {
public:
    PerHashStrings(StringList::iterator begin, StringList::iterator end) noexcept
        : m_begin(begin), m_end(end)
    {}

    StringList::iterator begin() const noexcept { return m_begin; }
    StringList::iterator end() const noexcept { return m_end; }

private:
    StringList::iterator m_begin;
    StringList::iterator m_end;
};

// This map stores copies of dynamic strings that have been used to initialise Identifiers.
struct DynamicStrMap {
    StringList string_list;

    std::unordered_map<uint32_t, PerHashStrings> map;
    std::mutex mutex;
};

int nifty_counter;
std::aligned_storage_t<sizeof(DynamicStrMap)> map_buf;
DynamicStrMap* p_dynamic_str_map = nullptr;

} // namespace

detail::StrMapInitialiser::StrMapInitialiser() noexcept
{
    if (nifty_counter++ == 0) {
        p_dynamic_str_map = new (&map_buf) DynamicStrMap{};
    }
}

detail::StrMapInitialiser::~StrMapInitialiser()
{
    if (--nifty_counter == 0) {
        p_dynamic_str_map->~DynamicStrMap();
    }
}

//--------------------------------------------------------------------------------------------------

void detail::report_hash_collision(std::string_view first, std::string_view second)
{
    auto details = fmt::format("'{}' and '{}' have the same hash.", first, second);
    g_log.write_warning("Detected Identifier hash collision: " + details);
}

void Identifier::set_full_string(std::string_view str)
{
    std::unique_lock<std::mutex> lock{ p_dynamic_str_map->mutex };

    auto& map = p_dynamic_str_map->map;
    auto& string_list = p_dynamic_str_map->string_list;

    // Find element corresponding to the hash in the dynamic string map.
    auto it = map.find(m_hash);

    if (it == map.end()) {
        // Hash not found in map; insert the string.
        string_list.push_front(std::string(str));
        const StringList::iterator it_begin = string_list.begin();
        const StringList::iterator it_end = std::next(it_begin);

        std::tie(it, std::ignore) = map.insert({ m_hash, { it_begin, it_end } });
        m_str = it_begin->c_str();
        return;
    }

    PerHashStrings& per_hash_strings = it->second;

    if (*per_hash_strings.begin() == str) {
        // The first string in the string store matches the sought one.
        m_str = per_hash_strings.begin()->c_str();
    }
    else {
        // There was a hash collision.
#if MG_IDENTIFIER_REPORT_HASH_COLLISION
        detail::report_hash_collision(*per_hash_strings.begin(), str);
#endif

        // See if this string is stored elsewhere in the same StringStore.
        for (auto&& stored_str : per_hash_strings) {
            if (stored_str == str) {
                m_str = stored_str.c_str();
                return;
            }
        }

        // If not, insert it.
        auto new_string_it = string_list.insert_after(per_hash_strings.begin(), std::string(str));
        m_str = new_string_it->c_str();
    }
}

} // namespace Mg
