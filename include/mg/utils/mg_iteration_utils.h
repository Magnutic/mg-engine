//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_iteration_utils.h
 * Utilities for common iteration patterns.
 */

#pragma once

#include <initializer_list>
#include <type_traits>
#include <utility>

namespace Mg {

namespace detail {

// Pick the const_iterator or iterator as appropriate, based on the container's constness.
template<typename ContT>
using ContainerIt = std::
    conditional_t<std::is_const_v<ContT>, typename ContT::const_iterator, typename ContT::iterator>;

} // namespace detail

template<typename It> class IterateAdjacentRange;
template<typename NumT, typename It> class EnumerateRange;
template<typename ItT, typename ItU> class ZipRange;

//--------------------------------------------------------------------------------------------------

/** Create an iterator range that iterates two adjacent items in the given container at a time.
 *
 * Usage example: given e.g. a vector of integers `std::vector<int> vec;`:
 *     for (auto&& [i1, i2] : Mg::iterate_adjacent(vec)) {
 *         // do something with i1, i2...
 *     }
 */
template<typename ContT>
auto iterate_adjacent(ContT&) -> IterateAdjacentRange<detail::ContainerIt<ContT>>;

/** Create an iterator range that iterates two adjacent items in the given initializer list at a
 * time.
 *
 * Usage example:
 *     for (auto&& [i1, i2] : Mg::iterate_adjacent({1,2,3,4})) {
 *         // do something with i1, i2...
 *     }
 */
template<typename ElemT>
auto iterate_adjacent(std::initializer_list<ElemT>)
    -> IterateAdjacentRange<typename std::initializer_list<ElemT>::iterator>;

/** Disallows iterate_adjacent over rvalue containers to reduce the risk of lifetime hazards. */
template<typename ContT>
auto iterate_adjacent(ContT&&) -> IterateAdjacentRange<detail::ContainerIt<ContT>>
{
    static_assert((sizeof(ContT), false),
                  "iterate_adjacent may not be used with a container rvalue. Instead, please "
                  "declare a variable to hold the container first, then call iterate_adjacent with "
                  "that variable. This avoids risks of lifetime issues with temporary values.");
}

//--------------------------------------------------------------------------------------------------

/** Returns an iterator range over the given container that increments a counter along with the
 * iteration.
 *
 * Usage example: given e.g. a vector of T `std::vector<T> vec;`:
 *     for (auto&& [i, value] : Mg::enumerate<size_t>(vec, 0) {
 *         // First iteration: i == 0; value = vec[0];
 *         // Second iteration: i == 1; value = vec[1];
 *         // etc.
 *     }
 */
template<typename NumT, typename ContT>
auto enumerate(ContT&, NumT counter_start = 0) -> EnumerateRange<NumT, detail::ContainerIt<ContT>>;

/** Returns an iterator range over the given initializer list that increments a counter along with
 * the iteration.
 *
 * Usage example:
 *     for (auto&& [i, value] : Mg::enumerate<size_t>({"a","b","c"}, 0)) {
 *         // First iteration: i == 0; value = "a";
 *         // Second iteration: i == 1; value = "b";
 *         // etc.
 *     }
 */
template<typename NumT, typename ElemT>
auto enumerate(std::initializer_list<ElemT> ilist, NumT counter_start = 0)
    -> EnumerateRange<NumT, typename std::initializer_list<ElemT>::iterator>;

/** Disallows enumerate over rvalue containers to reduce the risk of lifetime hazards. */
template<typename NumT, typename ContT>
auto enumerate(ContT&&, NumT = 0) -> EnumerateRange<NumT, detail::ContainerIt<ContT>>
{
    static_assert((sizeof(ContT), false),
                  "enumerate may not be used with a container rvalue. Instead, please declare a "
                  "variable to hold the container first, then call enumerate with that variable. "
                  "This avoids risks of lifetime issues with temporary values.");
}

//--------------------------------------------------------------------------------------------------

/** Constructs an iterator range that iterates over two ranges simultaneously. Stops at the end of
 * the shorter range.
 *
 * Usage example: given e.g. two vectors of T and U `std::vector<T> vec1; std::vector<U> vec2`:
 *     for (auto&& [tv, uv] : Mg::zip(vec1, vec2)) {
 *         // Do something with tv and uv...
 *     }
 */
template<typename ContT_T, typename ContT_U>
auto zip(ContT_T&, ContT_U&)
    -> ZipRange<detail::ContainerIt<ContT_T>, detail::ContainerIt<ContT_U>>;

/** Constructs an iterator range that iterates over two ranges simultaneously. Stops at the end of
 * the shorter range.
 *
 * Usage example: given e.g. a vectors of T `std::vector<T> vec`:
 *     for (auto&& [tv, uv] : Mg::zip({1,2,3}, vec)) {
 *         // Do something with tv and uv...
 *     }
 */
template<typename ElemT, typename ContT_U>
auto zip(std::initializer_list<ElemT>, ContT_U&)
    -> ZipRange<typename std::initializer_list<ElemT>::iterator, detail::ContainerIt<ContT_U>>;

/** Constructs an iterator range that iterates over two ranges simultaneously. Stops at the end of
 * the shorter range.
 *
 * Usage example: given e.g. a vectors of T `std::vector<T> vec`:
 *     for (auto&& [tv, uv] : Mg::zip(vec, {1,2,3})) {
 *         // Do something with tv and uv...
 *     }
 */
template<typename ContT_T, typename ElemU>
auto zip(ContT_T&, std::initializer_list<ElemU>)
    -> ZipRange<detail::ContainerIt<ContT_T>, typename std::initializer_list<ElemU>::iterator>;

/** Constructs an iterator range that iterates over two ranges simultaneously. Stops at the end of
 * the shorter range.
 *
 * Usage example:
 *     for (auto&& [tv, uv] : Mg::zip({1,2,3}, {"1","2","3"})) {
 *         // Do something with tv and uv...
 *     }
 */
template<typename ElemT, typename ElemU>
auto zip(std::initializer_list<ElemT>, std::initializer_list<ElemU>)
    -> ZipRange<typename std::initializer_list<ElemT>::iterator,
                typename std::initializer_list<ElemU>::iterator>;

/** Disallows zip over rvalue containers to reduce the risk of lifetime hazards. */
template<typename ContT_T, typename ContT_U>
auto zip(ContT_T&&, ContT_U&)
    -> ZipRange<detail::ContainerIt<ContT_T>, detail::ContainerIt<ContT_U>>
{
    static_assert((sizeof(ContT_T), false),
                  "zip may not be used with a container rvalue. Instead, please declare a "
                  "variable to hold the container first, then call zip with that variable. "
                  "This avoids risks of lifetime issues with temporary values.");
}

/** Disallows zip over rvalue containers to reduce the risk of lifetime hazards. */
template<typename ContT_T, typename ContT_U>
auto zip(ContT_T&, ContT_U&&)
    -> ZipRange<detail::ContainerIt<ContT_T>, detail::ContainerIt<ContT_U>>
{
    static_assert((sizeof(ContT_U), false),
                  "zip may not be used with a container rvalue. Instead, please declare a "
                  "variable to hold the container first, then call zip with that variable. "
                  "This avoids risks of lifetime issues with temporary values.");
}

/** Disallows zip over rvalue containers to reduce the risk of lifetime hazards. */
template<typename ContT_T, typename ContT_U>
auto zip(ContT_T&&, ContT_U&&)
    -> ZipRange<detail::ContainerIt<ContT_T>, detail::ContainerIt<ContT_U>>
{
    static_assert((sizeof(ContT_U), false),
                  "zip may not be used with a container rvalue. Instead, please declare a "
                  "variable to hold the container first, then call zip with that variable. "
                  "This avoids risks of lifetime issues with temporary values.");
}

//--------------------------------------------------------------------------------------------------
// iterate_adjacent implementation
//-------------------------------------------------------------------------------------------------

/** Implementation of iterate_adjacent. */
template<typename It> class IterateAdjacentRange {
public:
    using T = std::remove_reference_t<decltype(*std::declval<It>())>;
    using Tref = std::add_lvalue_reference_t<T>;

    template<typename ContT>
    explicit IterateAdjacentRange(ContT& container)
        : m_begin(std::begin(container)), m_end(std::end(container))
    {
        if (m_end != m_begin) {
            --m_end;
        }
    }

    ~IterateAdjacentRange() = default;

    IterateAdjacentRange(const IterateAdjacentRange&) = delete;
    IterateAdjacentRange(IterateAdjacentRange&&) = delete;
    IterateAdjacentRange& operator=(const IterateAdjacentRange&) = delete;
    IterateAdjacentRange& operator=(IterateAdjacentRange&&) = delete;

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

template<typename ContT>
auto iterate_adjacent(ContT& container) -> IterateAdjacentRange<detail::ContainerIt<ContT>>
{
    return IterateAdjacentRange<detail::ContainerIt<ContT>>{ container };
}

template<typename ElemT>
auto iterate_adjacent(std::initializer_list<ElemT> ilist)
    -> IterateAdjacentRange<typename std::initializer_list<ElemT>::iterator>
{
    return IterateAdjacentRange<typename std::initializer_list<ElemT>::iterator>{ ilist };
}

//--------------------------------------------------------------------------------------------------
// enumerate implementation
//-------------------------------------------------------------------------------------------------

template<typename NumT, typename It> class EnumerateRange {
public:
    using T = std::remove_reference_t<decltype(*std::declval<It>())>;
    using Tref = std::add_lvalue_reference_t<T>;

    template<typename ContT>
    explicit EnumerateRange(ContT& container, const NumT num)
        : m_begin(std::begin(container)), m_end(std::end(container)), m_num(num)
    {}

    ~EnumerateRange() = default;

    // Explicitly disallow construction from rvalues, to reduce risk of lifetime hazards.
    template<typename ContT> explicit EnumerateRange(ContT&& container, NumT) = delete;

    EnumerateRange(const EnumerateRange&) = delete;
    EnumerateRange(EnumerateRange&&) = delete;
    EnumerateRange& operator=(const EnumerateRange&) = delete;
    EnumerateRange& operator=(EnumerateRange&&) = delete;

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

    It m_begin;
    It m_end;
    NumT m_num;
};

template<typename NumT, typename ContT>
auto enumerate(ContT& container, const NumT counter_start)
    -> EnumerateRange<NumT, detail::ContainerIt<ContT>>
{
    return EnumerateRange<NumT, detail::ContainerIt<ContT>>{ container, counter_start };
}

template<typename NumT, typename ElemT>
auto enumerate(std::initializer_list<ElemT> ilist, const NumT counter_start)
    -> EnumerateRange<NumT, typename std::initializer_list<ElemT>::iterator>
{
    return EnumerateRange<NumT, typename std::initializer_list<ElemT>::iterator>{ ilist,
                                                                                  counter_start };
}

//--------------------------------------------------------------------------------------------------
// zip implementation
//-------------------------------------------------------------------------------------------------

template<typename ItT, typename ItU> class ZipRange {
public:
    using T = std::remove_reference_t<decltype(*std::declval<ItT>())>;
    using U = std::remove_reference_t<decltype(*std::declval<ItU>())>;
    using Tref = std::add_lvalue_reference_t<T>;
    using Uref = std::add_lvalue_reference_t<U>;

    template<typename ContT_T, typename ContT_U>
    explicit ZipRange(ContT_T& container_t, ContT_U& container_u)
        : m_begin_t(std::begin(container_t))
        , m_end_t(std::end(container_t))
        , m_begin_u(std::begin(container_u))
        , m_end_u(std::end(container_u))
    {}

    ~ZipRange() = default;

    ZipRange(const ZipRange&) = delete;
    ZipRange(ZipRange&&) = delete;
    ZipRange& operator=(const ZipRange&) = delete;
    ZipRange& operator=(ZipRange&&) = delete;

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
auto zip(ContT_T& container_t, ContT_U& container_u)
    -> ZipRange<detail::ContainerIt<ContT_T>, detail::ContainerIt<ContT_U>>
{
    return ZipRange<detail::ContainerIt<ContT_T>, detail::ContainerIt<ContT_U>>{ container_t,
                                                                                 container_u };
}

template<typename ElemT, typename ContT_U>
auto zip(std::initializer_list<ElemT> ilist, ContT_U& container_u)
    -> ZipRange<typename std::initializer_list<ElemT>::iterator, detail::ContainerIt<ContT_U>>
{
    return ZipRange<typename std::initializer_list<ElemT>::iterator, detail::ContainerIt<ContT_U>>{
        ilist, container_u
    };
}

template<typename ContT_T, typename ElemU>
auto zip(ContT_T& container_t, std::initializer_list<ElemU> ilist_u)
    -> ZipRange<detail::ContainerIt<ContT_T>, typename std::initializer_list<ElemU>::iterator>
{
    return ZipRange<detail::ContainerIt<ContT_T>, typename std::initializer_list<ElemU>::iterator>{
        container_t, ilist_u
    };
}

template<typename ElemT, typename ElemU>
auto zip(std::initializer_list<ElemT> ilist_t, std::initializer_list<ElemU> ilist_u)
    -> ZipRange<typename std::initializer_list<ElemT>::iterator,
                typename std::initializer_list<ElemU>::iterator>
{
    return ZipRange<typename std::initializer_list<ElemT>::iterator,
                    typename std::initializer_list<ElemU>::iterator>{ ilist_t, ilist_u };
}

} // namespace Mg
