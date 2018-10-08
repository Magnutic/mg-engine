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

/** @file mg_stl_helpers.h
 * Convenience helpers for the STL; reduce boilerplate.
 * This does not intend to replace the direct use of STL algorithms; it is intended to make common
 * usage patterns more convenient.
 */

#pragma once

#include <algorithm>
#include <optional>

namespace Mg {

// These functions use forwarding references (Cont&& container) instead of lvalue references
// (Cont& container) as parameters. This is useful when the argument is a non-owning range (e.g.
// gsl::span) which was returned by value from a function.

/** Find element in container by iterating over elements. */
template<typename ContT, typename ElemT = typename ContT::value_type>
auto find(ContT&& container, const ElemT& element)
{
    return std::find(container.begin(), container.end(), element);
}

/** Find element matching predicate in container by iterating over elements. */
template<typename ContT, typename F> auto find_if(ContT&& container, F&& predicate)
{
    return std::find_if(container.begin(), container.end(), std::forward<F>(predicate));
}

/** Find mapped element in map by key. Returns nullptr if no such key exists. */
template<typename MapT,
         typename KeyT  = typename MapT::key_type,
         typename ElemT = typename MapT::mapped_type>
ElemT* find_in_map(MapT& map, const KeyT& key)
{
    auto it = map.find(key);

    if (it == map.end()) {
        return nullptr;
    }

    return std::addressof(it->second);
}

/** Find element mapped by key in map and apply func to it, if found.
 * @return true if element was found.
 */
template<typename MapT, typename F, typename KeyT = typename MapT::key_type>
bool apply_in_map(MapT& map, const KeyT& key, F&& func)
{
    if (auto it = map.find(key); it != map.end()) {
        func(it->second);
        return true;
    }

    return false;
}

/** Return type of index_where and index_of. */
struct index_where_result {
    /* Whether matching element was found. */
    bool found{};
    /* Index of matching element. */
    size_t index{};
};

/** Return index of first element matching given predicate. */
template<typename ContT, typename F>
index_where_result index_where(ContT&& container, F&& predicate)
{
    size_t index = 0;

    for (auto&& elem : container) {
        if (predicate(elem)) {
            break;
        }
        ++index;
    }

    const bool found = index < container.size();
    return { found, found ? index : 0 };
}

/** Return index of first result equal to `elem`. */
template<typename ContT, typename ElemT = typename ContT::value_type>
index_where_result index_of(ContT&& container, const ElemT& elem)
{
    return index_where(std::forward<ContT>(container),
                       [&elem](const ElemT& e) { return e == elem; });
}

// find_and_erase_if:
// The first implementation does not work on maps, because remove_if requires elements to be
// move assignable; maps store key as const. Fall back to second implementation, if so.

/** Find and erase elements matching predicate in container by iterating over elements. */
template<typename ContT,
         typename F,
         typename ElemT = typename std::decay_t<ContT>::value_type,
         typename std::enable_if_t<std::is_move_assignable_v<ElemT>, int> = 0>
bool find_and_erase_if(ContT&& container, F&& predicate)
{
    auto it = std::remove_if(container.begin(), container.end(), std::forward<F>(predicate));

    if (it != container.end()) {
        container.erase(it, container.end());
        return true;
    }

    return false;
}

/** Find and erase elements matching predicate in container by iterating over elements. */
template<typename ContT, typename F, typename = typename std::decay_t<ContT>::mapped_type>
bool find_and_erase_if(ContT&& container, F&& predicate)
{
    int num_removed = 0;

    auto it = container.begin();
    while (it != container.end()) {
        if (predicate(*it)) {
            it = container.erase(it);
            ++num_removed;
        }
        else {
            ++it;
        }
    }

    return num_removed > 0;
}

/** Find and erase element in container by iterating over elements. */
template<typename ContT, typename ElemT = typename ContT::value_type>
bool find_and_erase(ContT&& container, const ElemT& element)
{
    return find_and_erase_if(std::forward<ContT>(container),
                             [&element](const ElemT& e) { e == element; });
}


/** Sort elements in container. */
template<typename ContT> void sort(ContT&& container)
{
    std::sort(container.begin(), container.end());
}

/** Sort elements in container using supplied comparison function. */
template<typename ContT, typename Cmp> void sort(ContT&& container, Cmp&& cmp)
{
    std::sort(container.begin(), container.end(), std::forward<Cmp>(cmp));
}

} // namespace Mg
