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

/** @file mg_instance_counter.h
 * InstanceCounter, type that counts the number of times it has been constructed/moved and detects
 * invalid uses of countee type. Primarily intended for use in test code.
 */

#pragma once

#include <exception>
#include <sstream>

namespace Mg {

namespace detail {
// Store counter here to make sure only one set of counter variables is created for each type T --
// other template parameters of InstanceCounter should not affect it.
template<typename T> struct CounterValues {
    static inline int s_counter      = 0;
    static inline int s_counter_move = 0;
};
} // namespace detail

/** Type that counts the number of times it has been constructed/moved. It is useful when testing
 * that containers and allocators properly construct/destroy objects. This is primarily intended as
 * a utility type for test code, but it could also be used (with some overhead) to add error
 * checking to application or library code.
 *
 * It also tracks the state of the object -- is it initialised, moved from, and/or destroyed -- and
 * throws when used incorrectly.
 *
 * Use by including Counter<T> as a member of T or as a base of T (CRTP-use), for example:
 *     class SomeClassA { Mg::Counter<SomeClassA> m_counter; public: ... };
 * or:
 *     class SomeClassB : public Mg::Counter<SomeClassB> { ... };
 *
 * @tparam T type to be counted
 * @tparam allow_copy_from_moved Whether it is valid to assign or initialise an object of type T by
 * copying/moving a moved-from T.
 */
template<typename T, bool allow_copy_from_moved = false, bool allow_self_assignment = false>
class InstanceCounter {
public:
    InstanceCounter()
    {
        m_initialised = true;
        ++detail::CounterValues<T>::s_counter;
        ++detail::CounterValues<T>::s_counter_move;
    }

    InstanceCounter(const InstanceCounter& rhs)
    {
        check_rhs("Copy constructing", rhs);

        m_initialised = true;
        m_moved_from  = rhs.m_moved_from;

        if (!rhs.is_moved_from()) { ++detail::CounterValues<T>::s_counter; }

        ++detail::CounterValues<T>::s_counter_move;
    }

    InstanceCounter(InstanceCounter&& rhs) noexcept
    {
        check_rhs("Move constructing", rhs);

        m_initialised = true;
        m_moved_from  = rhs.m_moved_from;
        ++detail::CounterValues<T>::s_counter_move;
        rhs.m_moved_from = true;
    }

    ~InstanceCounter()
    {
        m_initialised = false;
        m_destroyed   = true;
        --detail::CounterValues<T>::s_counter_move;
        if (!is_moved_from()) { --detail::CounterValues<T>::s_counter; }
    }

    InstanceCounter& operator=(const InstanceCounter& rhs)
    {
        check_rhs("Copy assigning", rhs);

        if (allow_self_assignment && this == &rhs) { return *this; }

        if (!is_moved_from() && rhs.is_moved_from()) { --detail::CounterValues<T>::s_counter; }

        if (is_moved_from() && !rhs.is_moved_from()) { ++detail::CounterValues<T>::s_counter; }

        m_moved_from = rhs.m_moved_from;
        return *this;
    }

    InstanceCounter& operator=(InstanceCounter&& rhs) noexcept
    {
        check_rhs("Move assigning", rhs);

        if (allow_self_assignment && this == &rhs) { return *this; }

        if (!is_moved_from()) { --detail::CounterValues<T>::s_counter; }

        m_moved_from     = rhs.m_moved_from;
        rhs.m_moved_from = true;
        return *this;
    }

    /** Get the number of objects of type T that currently exist, excluding moved-from objects. */
    static int get_counter() { return detail::CounterValues<T>::s_counter; }

    /** Get the number of objects of type T that currently exist, including moved-from objects. */
    static int get_counter_move() { return detail::CounterValues<T>::s_counter_move; }

    bool is_initialised() const { return m_initialised; }

    bool is_destroyed() const { return m_destroyed; }

    bool is_moved_from() const { return m_moved_from; }

private:
    void check_rhs(const char* action, const InstanceCounter& rhs)
    {
        std::ostringstream ss;
        ss << action << ": ";
        bool error = false;

        auto notify_error = [&](const char* what) {
            ss << what << ". ";
            error = true;
        };

        if (!allow_self_assignment && this == &rhs) { notify_error("self-assignment"); }

        if (!rhs.is_initialised()) { notify_error("rhs is unitialised"); }

        if (!allow_copy_from_moved && rhs.is_moved_from()) { notify_error("rhs is moved-from"); }

        if (rhs.is_destroyed()) { notify_error("rhs is destroyed"); }

        if (error) {
            ss << "(this: " << this << ", rhs: " << &rhs << ")";
            auto string = ss.str();
            throw std::logic_error{ string.c_str() };
        }
    }

    bool m_initialised = false;
    bool m_destroyed   = false;
    bool m_moved_from  = false;
};

} // namespace Mg
