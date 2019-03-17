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

// Strings are stored in an unordered_map of linked lists. This means that the strings can never
// move, and thus pointers to string data will not be invalidated, while also supporting multiple
// strings with the same hash.
//
// It is also a remarkably inefficient storage method, and should probably be replaced with
// something leaner, if need arises. Probably store string data using a bump allocator along with an
// auxiliary hashmap to look up strings by hash.

// Stores the string(s) matching a given hash. In the vast majority of cases, there should be only
// one element, but in the case of a hash collision, there may be more.
using StringStore = std::forward_list<std::string>;

// This map stores copies of dynamic strings that have been used to initialise Identifiers.
struct DynamicStrMap {
    std::unordered_map<uint32_t, StringStore> map;
    std::mutex                                mutex;
};

int                                           nifty_counter;
std::aligned_storage_t<sizeof(DynamicStrMap)> map_buf;
DynamicStrMap*                                p_dynamic_str_map = nullptr;

} // namespace

detail::StrMapInitialiser::StrMapInitialiser()
{
    if (nifty_counter++ == 0) { p_dynamic_str_map = new (&map_buf) DynamicStrMap{}; }
}

detail::StrMapInitialiser::~StrMapInitialiser()
{
    if (--nifty_counter == 0) { p_dynamic_str_map->~DynamicStrMap(); }
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

    // Find element corresponding to the hash in the dynamic string map.
    auto it = map.find(m_hash);

    if (it == map.end()) {
        // Hash not found in map; insert the string.
        std::tie(it, std::ignore) = map.insert({ m_hash, { std::string(str) } });
        StringStore& string_store = it->second;
        m_str                     = string_store.front().c_str();
        return;
    }

    StringStore& string_store = it->second;

    if (string_store.front() == str) {
        // The first string in the string store matches the sought one.
        m_str = string_store.front().c_str();
    }
    else {
        // There was a hash collision.
#if MG_IDENTIFIER_REPORT_HASH_COLLISION
        detail::report_hash_collision(string_store.front(), str);
#endif

        // See if this string is stored elsewhere in the same StringStore.
        for (auto&& stored_str : string_store) {
            if (stored_str == str) {
                m_str = stored_str.c_str();
                return;
            }
        }

        // If not, insert it.
        m_str = string_store.emplace_front(str).c_str();
    }
}

} // namespace Mg
