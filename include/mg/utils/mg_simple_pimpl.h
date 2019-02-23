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

/** @file mg_simple_pimpl.h
 * Utility to simplify usage of the PIMPL (pointer to implementation) idiom.
 */

#pragma once

#include "mg/utils/mg_pointer.h"

#include <utility>

namespace Mg {

/** Mixin utility type, reduces boilerplate of writing PIMPL (pointer to implementation) classes.
 * Provides ownership of `ImplT` implementation instance, which may be accessed within the subclass
 * via `data()` member functions.
 * PimplMixin's constructor forwards its arguments to ImplT's constructor.
 *
 * @remark PimplMixin is intended to be used as a base class, for example:
 *     class MyClass : PimplMixin<MyClassImpl> { ... }
 *
 * @remark As always when using the PIMPL idiom, the public class's constructors and destructor must
 * be defined in the .cpp file, after `ImplT` is defined (i.e. `ImplT` must be a complete type).
 *
 * @tparam ImplT Implementation type.
 */
template<typename ImplT> class PimplMixin {
protected:
    template<typename... Args>
    PimplMixin(Args&&... args) : m_impl(Ptr<ImplT>::make(std::forward<Args>(args)...))
    {}

    PimplMixin(const PimplMixin& rhs) : m_impl(Ptr<ImplT>::make(*rhs.m_impl)) {}

    PimplMixin(PimplMixin&& rhs) = default;

    PimplMixin& operator=(PimplMixin rhs) noexcept { swap(rhs); }

    void swap(PimplMixin& rhs) noexcept
    {
        using std::swap;
        swap(data(), rhs.data());
    }

    friend void swap(PimplMixin& lhs, PimplMixin& rhs) noexcept { lhs.swap(rhs); }

    ImplT&       data() { return *m_impl; }
    const ImplT& data() const { return *m_impl; }

private:
    Ptr<ImplT> m_impl;
};

/** Mixin utility type, acts like the PIMPL-pattern but stores the private implementation data
 * inline instead of through a pointer. This means that it needs to know the maximum size of the
 * implementation (the `max_impl_size` template paramater).
 *
 * Usage is identical to `PimplMixin`.
 *
 * @tparam ImplT Implementation type.
 * @tparam max_impl_size Maximum size of private implementation class. A buffer of this size is
 * allocated within the class. Changing the private implementation class does not affect the
 * public class's ABI as long as `max_impl_size` stays the same; changing `max_impl_size` will
 * break ABI.
 */
template<typename ImplT, size_t max_impl_size> class InPlacePimplMixin {
protected:
    template<typename... Args> InPlacePimplMixin(Args&&... args)
    {
        static_assert(sizeof(ImplT) <= max_impl_size);
        new (&m_impl_buffer) ImplT(std::forward<Args>(args)...);
    }

    InPlacePimplMixin(const InPlacePimplMixin& rhs) { new (&m_impl_buffer) ImplT(rhs.data()); }

    InPlacePimplMixin(InPlacePimplMixin&& rhs) noexcept
    {
        new (&m_impl_buffer) ImplT(std::move_if_noexcept(rhs.data()));
    }

    InPlacePimplMixin& operator=(InPlacePimplMixin rhs) noexcept { swap(rhs); }

    ~InPlacePimplMixin() { data().~ImplT(); }

    void swap(InPlacePimplMixin& rhs) noexcept
    {
        using std::swap;
        swap(data(), rhs.data());
    }

    friend void swap(InPlacePimplMixin& lhs, InPlacePimplMixin& rhs) noexcept { lhs.swap(rhs); }

    ImplT&       data() { return *reinterpret_cast<ImplT*>(&m_impl_buffer); }
    const ImplT& data() const { return *reinterpret_cast<const ImplT*>(&m_impl_buffer); }

private:
    std::aligned_storage_t<max_impl_size, alignof(max_align_t)> m_impl_buffer;
};

} // namespace Mg
