//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_iteration_utils.h
 * Utilities for common iteration patterns.
 */

#pragma once

#include <type_traits>
#include <utility>

namespace Mg {

namespace detail {

// Pick the const_iterator or iterator as appropriate based on the container's constness.
template<typename ContT>
using ContainerIt = std::
    conditional_t<std::is_const_v<ContT>, typename ContT::const_iterator, typename ContT::iterator>;

} // namespace detail

/** Iterator range that iterates two adjacent items at a time.
 *
 * Usage example: given e.g. a vector of integers `std::vector<int> vec;`:
 *     for (auto&& [i1, i2] : Mg::IterateAdjacent{vec}) {
 *         // do something with i1, i2...
 *     }
 */
template<typename It> class IterateAdjacent {
public:
    using T = std::remove_reference_t<decltype(*std::declval<It>())>;
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

template<typename ContT> IterateAdjacent(ContT&) -> IterateAdjacent<detail::ContainerIt<ContT>>;


/** Iterator range that increments a counter along with the iteration.
 *
 * Usage example: given e.g. a vector of T `std::vector<T> vec;`:
 *     for (auto&& [i, value] : Mg::Enumerate{vec, size_t(0)}) {
 *         // First iteration: i == 0; value = vec[0];
 *         // Second iteration: i == 1; value = vec[1];
 *         // etc.
 *     }
 */
template<typename NumT, typename It> class Enumerate {
public:
    using T = std::remove_reference_t<decltype(*std::declval<It>())>;
    using Tref = std::add_lvalue_reference_t<T>;

    template<typename ContT>
    explicit Enumerate(ContT& container, const NumT num = NumT{})
        : m_begin(std::begin(container)), m_end(std::end(container)), m_num(num)
    {}

    class iterator {
    public:
        using value_type = std::pair<NumT, Tref>;

        iterator(It it, NumT num) : m_it(it), m_num(num) {}

        value_type operator*() const { return { m_num, *m_it }; }

        iterator& operator++()
        {
            ++m_it;
            ++m_num;
            return *this;
        }

        iterator operator++(int)
        {
            iterator tmp(m_it, m_num);
            ++m_it;
            ++m_num;
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
        It m_it;
        NumT m_num;
    };

    iterator begin() const noexcept { return iterator(m_begin, m_num); }
    iterator end() const noexcept { return iterator(m_end, NumT{}); }

private:
    It m_begin;
    It m_end;
    NumT m_num;
};

template<typename ContT, typename NumT>
Enumerate(ContT&, NumT) -> Enumerate<NumT, detail::ContainerIt<ContT>>;

/** Iterator range that iterates over two ranges simultaneously. Stops at the end of the shorter
 * range.
 *
 * Usage example: given e.g. two vectors of T and U `std::vector<T> vec1; std::vector<U> vec2`:
 *     for (auto&& [tv, uv] : Mg::Zip{vec1, vec2}) {
 *         // Do something with tv and uv...
 *     }
 */
template<typename ItT, typename ItU> class Zip {
public:
    using T = std::remove_reference_t<decltype(*std::declval<ItT>())>;
    using U = std::remove_reference_t<decltype(*std::declval<ItU>())>;
    using Tref = std::add_lvalue_reference_t<T>;
    using Uref = std::add_lvalue_reference_t<U>;

    template<typename ContT_T, typename ContT_U>
    explicit Zip(ContT_T& container_t, ContT_U& container_u)
        : m_begin_t(std::begin(container_t))
        , m_end_t(std::end(container_t))
        , m_begin_u(std::begin(container_u))
        , m_end_u(std::end(container_u))
    {}

    class iterator {
    public:
        using value_type = std::pair<Tref, Uref>;

        iterator(ItT it_t, ItU it_u) : m_it_t(it_t), m_it_u(it_u) {}

        value_type operator*() const { return { *m_it_t, *m_it_u }; }

        iterator& operator++()
        {
            ++m_it_t;
            ++m_it_u;
            return *this;
        }

        iterator operator++(int)
        {
            iterator tmp(m_it_t, m_it_u);
            ++m_it_t;
            ++m_it_u;
            return tmp;
        }

        friend bool operator==(const iterator& lhs, const iterator& rhs) noexcept
        {
            // Return equal if either are equal, to ensure equality with the end iterator when
            // either have reached the end.
            return lhs.m_it_t == rhs.m_it_t || lhs.m_it_u == rhs.m_it_u;
        }
        friend bool operator!=(const iterator& lhs, const iterator& rhs) noexcept
        {
            return !(lhs == rhs);
        }

    private:
        ItT m_it_t;
        ItU m_it_u;
    };

    iterator begin() const noexcept { return iterator(m_begin_t, m_begin_u); }
    iterator end() const noexcept { return iterator(m_end_t, m_end_u); }

private:
    ItT m_begin_t;
    ItT m_end_t;
    ItU m_begin_u;
    ItU m_end_u;
};

template<typename ContT_T, typename ContT_U>
Zip(ContT_T&, ContT_U&) -> Zip<detail::ContainerIt<ContT_T>, detail::ContainerIt<ContT_U>>;

} // namespace Mg
