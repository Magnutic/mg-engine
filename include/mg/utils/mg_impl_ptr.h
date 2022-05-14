//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_impl_ptr.h
 * Smart pointer for the pointer-to-implementation (pimpl) pattern. This pointer only requires that
 * its pointee type is complete in its constructor and dereferencing operators.
 */

#pragma once

#include <utility>

namespace Mg {

template<typename ImplT> class ImplPtr {
public:
    template<typename... CtorArgs>
    explicit ImplPtr(CtorArgs&&... ctor_args)
        : m_ptr(new ImplT(std::forward<CtorArgs>(ctor_args)...))
        , m_delete_function(make_delete_function())
    {}

    ~ImplPtr()
    {
        if (m_ptr) m_delete_function(m_ptr);
    }

    // Copying not implemented since the need has not arisen. If it needs to be implemented, it can
    // be done using a CopyFunction, similar to the DeleteFunction. However, this requires some way
    // of disabling the copy constructor depending on ImplT, which is easier to do in C++20.
    ImplPtr(const ImplPtr&) = delete;
    ImplPtr& operator=(const ImplPtr&) = delete;

    ImplPtr(ImplPtr&& rhs) noexcept
        : m_ptr(std::exchange(rhs.m_ptr, nullptr))
        , m_delete_function(std::exchange(rhs.m_delete_function, nullptr))
    {}

    ImplPtr& operator=(ImplPtr&& rhs) noexcept
    {
        swap(ImplPtr(std::move(rhs)));
        return *this;
    }

    void swap(ImplPtr& other) noexcept
    {
        std::swap(m_ptr, other.m_ptr);
        std::swap(m_delete_function, other.m_delete_function);
    }

    ImplT& operator*() { return *m_ptr; }
    const ImplT& operator*() const { return *m_ptr; }

    ImplT* operator->() { return m_ptr; }
    const ImplT* operator->() const { return m_ptr; }

    ImplT* get() { return m_ptr; }
    const ImplT* get() const { return m_ptr; }

    explicit operator bool() const { return m_ptr != nullptr; }

private:
    using DeleteFunction = void (*)(const ImplT*);

    static DeleteFunction make_delete_function()
    {
        return [](const ImplT* impl) { delete impl; };
    }

    ImplT* m_ptr = nullptr;
    DeleteFunction m_delete_function = nullptr;
};

} // namespace Mg
