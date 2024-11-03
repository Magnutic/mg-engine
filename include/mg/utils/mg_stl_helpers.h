//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_stl_helpers.h
 * Convenience helpers for the STL; reduce boilerplate.
 * This does not intend to replace the direct use of STL algorithms; it is intended to make common
 * usage patterns more convenient.
 */

#pragma once

#include <algorithm>
#include <type_traits>

namespace Mg {

// These functions use forwarding references (Cont&& container) instead of lvalue references
// (Cont& container) as parameters. This is useful when the argument is a non-owning range (e.g.
// std::span) which was returned by value from a function, but risks causing dangling references if
// used on rvalue containers.

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
         typename KeyT = typename MapT::key_type,
         typename ElemT = typename MapT::mapped_type>
auto find_in_map(MapT& map, const KeyT& key) -> decltype(std::addressof(map.find(key)->second))
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

template<typename ContT, typename ElemT = typename ContT::value_type>
bool contains(ContT&& container, const ElemT& elem)
{
    return index_of(container, elem).found;
}

template<typename ContT, typename F> bool any_of(ContT&& container, F&& predicate)
{
    return std::any_of(container.begin(), container.end(), predicate);
}

template<typename ContT, typename F> bool all_of(ContT&& container, F&& predicate)
{
    return std::all_of(container.begin(), container.end(), predicate);
}

template<typename ContT, typename F> bool none_of(ContT&& container, F&& predicate)
{
    return std::none_of(container.begin(), container.end(), predicate);
}

template<typename ContT, typename F> size_t count_if(ContT&& container, F&& predicate)
{
    return static_cast<size_t>(std::count_if(container.begin(), container.end(), predicate));
}

template<typename ContT, typename ElemT = typename ContT::value_type>
size_t count(ContT&& container, const ElemT& elem)
{
    return static_cast<size_t>(std::count(container.begin(), container.end(), elem));
}

template<typename ContT, typename ElemT = typename ContT::value_type>
auto lower_bound(ContT&& container, const ElemT& value)
{
    return std::lower_bound(container.begin(), container.end(), value);
}

template<typename ContT, typename Cmp, typename ElemT = typename ContT::value_type>
auto lower_bound(ContT&& container, const ElemT& value, Cmp&& compare)
{
    return std::lower_bound(container.begin(), container.end(), value, std::forward<Cmp>(compare));
}

template<typename ContT, typename ElemT = typename ContT::value_type>
auto upper_bound(ContT&& container, const ElemT& value)
{
    return std::upper_bound(container.begin(), container.end(), value);
}

template<typename ContT, typename Cmp, typename ElemT = typename ContT::value_type>
auto upper_bound(ContT&& container, const ElemT& value, Cmp&& compare)
{
    return std::upper_bound(container.begin(), container.end(), value, std::forward<Cmp>(compare));
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

/** Find and erase elements matching predicate in container by iterating over elements.
 * Implementation for std::map-like types.
 */
template<typename ContT,
         typename F,
         typename ElemT = typename std::decay_t<ContT>::value_type,
         // Requires value_type not move-assignable
         typename std::enable_if_t<!std::is_move_assignable_v<ElemT>, int> = 1,
         // Requires has typedef mapped_type
         typename = typename std::decay_t<ContT>::mapped_type>
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

/** Sort container and erase duplicate elements. */
template<typename ContT> void sort_unique(ContT&& container)
{
    std::sort(container.begin(), container.end());
    container.erase(std::unique(container.begin(), container.end()), container.end());
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
