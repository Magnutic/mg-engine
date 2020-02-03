//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_simple_pimpl.h
 * Utility to simplify usage of the PIMPL (pointer to implementation) idiom.
 */

#pragma once

#include <memory>
#include <utility>

namespace Mg {

/** Mixin utility type, reduces boilerplate of writing PIMPL (pointer to implementation) classes.
 * Provides ownership of `ImplT` implementation instance, which may be accessed within the subclass
 * via `impl()` member functions.
 * PImplMixin's constructor forwards its arguments to ImplT's constructor.
 *
 * @remark PImplMixin is intended to be used as a base class, for example:
 *     class MyClass : PImplMixin<MyClassImpl> { ... }
 *
 * @remark As always when using the PIMPL idiom, the public class's constructors and destructor must
 * be defined in the .cpp file, after `ImplT` is defined (i.e. `ImplT` must be a complete type).
 *
 * @tparam ImplT Implementation type.
 */
template<typename ImplT> class PImplMixin {
protected:
    template<typename... Args>
    PImplMixin(Args&&... args) : m_impl(std::make_unique<ImplT>(std::forward<Args>(args)...))
    {}

    PImplMixin(const PImplMixin& rhs) : m_impl(std::make_unique<ImplT>(*rhs.m_impl)) {}

    PImplMixin(PImplMixin&& rhs) = default;

    PImplMixin& operator=(const PImplMixin& rhs)
    {
        PImplMixin tmp(rhs);
        swap(tmp);
        return *this;
    }

    PImplMixin& operator=(PImplMixin&& rhs) noexcept
    {
        PImplMixin tmp(std::move(rhs));
        swap(tmp);
        return *this;
    }

    void swap(PImplMixin& rhs) noexcept
    {
        using std::swap;
        swap(impl(), rhs.impl());
    }

    friend void swap(PImplMixin& lhs, PImplMixin& rhs) noexcept { lhs.swap(rhs); }

    ImplT& impl() noexcept { return *m_impl; }
    const ImplT& impl() const noexcept { return *m_impl; }

private:
    std::unique_ptr<ImplT> m_impl;
};

/** Mixin utility type, acts like the PIMPL-pattern but stores the private implementation data
 * inline instead of through a pointer. This means that it needs to know the maximum size of the
 * implementation (the `max_impl_size` template paramater).
 *
 * Usage is identical to `PImplMixin`.
 *
 * @tparam ImplT Implementation type.
 * @tparam max_impl_size Maximum size of private implementation class. A buffer of this size is
 * allocated within the class. Changing the private implementation class does not affect the
 * public class's ABI as long as `max_impl_size` stays the same; changing `max_impl_size` will
 * break ABI.
 */
template<typename ImplT, size_t max_impl_size> class InPlacePImplMixin {
protected:
    template<typename... Args> InPlacePImplMixin(Args&&... args)
    {
        static_assert(sizeof(ImplT) <= max_impl_size);
        new (&m_impl_buffer) ImplT(std::forward<Args>(args)...);
    }

    InPlacePImplMixin(const InPlacePImplMixin& rhs) { new (&m_impl_buffer) ImplT(rhs.impl()); }

    InPlacePImplMixin(InPlacePImplMixin&& rhs) noexcept
    {
        new (&m_impl_buffer) ImplT(std::move_if_noexcept(rhs.impl()));
    }

    InPlacePImplMixin& operator=(const InPlacePImplMixin& rhs)
    {
        InPlacePImplMixin tmp(rhs);
        swap(tmp);
        return *this;
    }

    InPlacePImplMixin& operator=(InPlacePImplMixin&& rhs) noexcept
    {
        InPlacePImplMixin tmp(std::move(rhs));
        swap(tmp);
        return *this;
    }

    ~InPlacePImplMixin() { impl().~ImplT(); }

    void swap(InPlacePImplMixin& rhs) noexcept
    {
        using std::swap;
        swap(impl(), rhs.impl());
    }

    friend void swap(InPlacePImplMixin& lhs, InPlacePImplMixin& rhs) noexcept { lhs.swap(rhs); }

    ImplT& impl() { return *reinterpret_cast<ImplT*>(&m_impl_buffer); }
    const ImplT& impl() const { return *reinterpret_cast<const ImplT*>(&m_impl_buffer); }

private:
    std::aligned_storage_t<max_impl_size, alignof(max_align_t)> m_impl_buffer;
};

} // namespace Mg
