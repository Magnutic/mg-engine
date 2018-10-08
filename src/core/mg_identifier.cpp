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

#include <mg/core/mg_identifier.h>

#include <iostream>
#include <mutex>
#include <new>
#include <type_traits>
#include <unordered_map>

#include <mg/utils/mg_assert.h>

namespace Mg {
//--------------------------------------------------------------------------------------------------
// Nifty counter-based lifetime management for dynamic string copy map.
// Guarantees initialisation before first use, even during static initialisation of other
// objects.
// (This is the same pattern used to initialise std::cout, etc.)
//--------------------------------------------------------------------------------------------------

// This map stores copies of dynamic strings that have been used to initialise Identifiers.
using DynamicStrMap = std::unordered_map<uint32_t, const std::string>;

static int nifty_counter;

std::aligned_storage_t<sizeof(DynamicStrMap)> map_buf;
std::aligned_storage_t<sizeof(std::mutex)>    mutex_buf;

static DynamicStrMap* p_dynamic_str_map = nullptr;
static std::mutex*    p_str_map_mutex   = nullptr;

detail::StrMapInitialiser::StrMapInitialiser()
{
    if (nifty_counter++ == 0) {
        p_dynamic_str_map = new (&map_buf) DynamicStrMap{}; // NOLINT
        p_str_map_mutex   = new (&mutex_buf) std::mutex{};  // NOLINT
    }
}

detail::StrMapInitialiser::~StrMapInitialiser()
{
    if (--nifty_counter == 0) {
        p_str_map_mutex->~mutex();
        p_dynamic_str_map->~DynamicStrMap();
    }
}

//--------------------------------------------------------------------------------------------------

void Identifier::set_full_string(std::string_view str)
{
    std::unique_lock<std::mutex> lock{ *p_str_map_mutex };

    auto& map = *p_dynamic_str_map;

    if (auto it = map.find(m_hash); it == map.end()) {
        bool inserted;
        std::tie(it, inserted) = map.insert({ m_hash, std::string{ str } });
        m_str                  = it->second.c_str();
    }
    else {
        // Sanity check, make sure strings match (detect hash collisions)
        MG_ASSERT_DEBUG(it->second == str);
        m_str = it->second.c_str();
    }
}

std::ostream& operator<<(std::ostream& out, const Mg::Identifier& rhs)
{
    return out << rhs.str_view();
}

} // namespace Mg
