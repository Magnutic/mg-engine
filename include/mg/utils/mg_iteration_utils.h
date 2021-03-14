//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_iteration_utils.h
 * Utilities for common iteration patterns.
 * TODO zip, enumerate?
 */

#pragma once

#include <type_traits>
#include <utility>

namespace Mg {

/** Iterator range that iterates two adjacent items at a time.
 * Usage example: given e.g. a vector of integers `std::vector<int> vec;`:
 *     for (auto&& [i1, i2] : Mg::IterateAdjacent{vec}) {
 *         // do something with i1, i2...
 *     }
 */
template<typename It> class IterateAdjacent {
public:
    using T = std::decay_t<decltype(*std::declval<It>())>;
    using Tref = std::add_lvalue_reference_t<T>;

    template<typename ContT>
    explicit IterateAdjacent(ContT& container)
        : m_begin(std::begin(container)), m_end(std::end(container))
    {
        if (m_end != m_begin) {
            --m_end;
        }
    }

    class iterator {
    public:
        using value_type = std::pair<Tref, Tref>;

        iterator(It it) : m_it(it) {}

        value_type operator*() const { return { *m_it, *second_iterator() }; }

        iterator& operator++()
        {
            ++m_it;
            return *this;
        }

        iterator operator++(int)
        {
            iterator tmp(m_it);
            ++m_it;
            return tmp;
        }

        friend bool operator==(const iterator& lhs, const iterator& rhs) noexcept
        {
            return lhs.m_it == rhs.m_it;
        }
        friend bool operator!=(const iterator& lhs, const iterator& rhs) noexcept
        {
            return lhs.m_it != rhs.m_it;
        }

    private:
        It second_iterator() const
        {
            It it = m_it;
            ++it;
            return it;
        }

        It m_it;
    };

    iterator begin() const noexcept { return iterator(m_begin); }
    iterator end() const noexcept { return iterator(m_end); }

private:
    It m_begin;
    It m_end;
};

template<typename ContT> IterateAdjacent(ContT&) -> IterateAdjacent<typename ContT::iterator>;

} // namespace Mg
