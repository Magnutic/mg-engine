//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Magnus Bergsten
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

/** @file mg_pointer.h
 * Minimalistic owning pointer, a simpler alternative to std::unique_ptr.
 */

#pragma once

#include <utility>

namespace Mg {

template<typename T> class DefaultDelete {
public:
    void operator()(T* ptr) { delete ptr; }
};

/* Minimalistic owning pointer, a simpler alternative to std::unique_ptr.
 * Most importantly, it avoids the need to include the <memory> header, which is surprisingly large.
 * Main differences from std::unique_ptr:
 *   - Does not support arrays (use Mg::Array or Mg::ArrayUnknownSize instead).
 *   - Does not support stateful deleters
 */
template<typename T, typename DeleterT = DefaultDelete<T>> class Ptr {
public:
    using pointer      = T*;
    using element_type = T;
    using deleter_type = DeleterT;

    template<typename... Args> static Ptr make(Args&&... args)
    {
        Ptr p;
        p.m_ptr = new T(std::forward<Args>(args)...);
        return p;
    }

    explicit Ptr(T* raw_ptr) noexcept : m_ptr(raw_ptr) {}

    ~Ptr() { deleter_type{}(m_ptr); }

    // Converting constructor
    template<typename U, typename D> Ptr(Ptr<U, D>&& rhs) : m_ptr(rhs.m_ptr) { rhs.release(); }

    Ptr()           = default;
    Ptr(const Ptr&) = delete;

    Ptr(Ptr&& p) noexcept : m_ptr(p.m_ptr) { p.release(); }

    Ptr& operator=(Ptr&& p)
    {
        Ptr temp(std::move(p));
        swap(temp);
        return *this;
    }

    void swap(Ptr& p) noexcept { std::swap(m_ptr, p.m_ptr); }

    T* get() const noexcept { return m_ptr; }

    T& operator*() const noexcept { return *m_ptr; }
    T* operator->() const noexcept { return m_ptr; }

    pointer release() noexcept
    {
        pointer ret = m_ptr;
        m_ptr       = nullptr;
        return ret;
    }

    void reset(pointer ptr = pointer())
    {
        Ptr temp;
        swap(temp);
        m_ptr = ptr;
    }

    friend bool operator==(const Ptr& l, const Ptr& r) noexcept { return l.m_ptr == r.m_ptr; }
    friend bool operator!=(const Ptr& l, const Ptr& r) noexcept { return l.m_ptr != r.m_ptr; }

    friend bool operator==(std::nullptr_t, const Ptr& r) noexcept { return r.m_ptr == nullptr; }
    friend bool operator!=(std::nullptr_t, const Ptr& r) noexcept { return r.m_ptr != nullptr; }
    friend bool operator==(const Ptr& l, std::nullptr_t) noexcept { return l.m_ptr == nullptr; }
    friend bool operator!=(const Ptr& l, std::nullptr_t) noexcept { return l.m_ptr != nullptr; }

private:
    template<typename U, typename D> friend class Ptr;

    pointer m_ptr = nullptr;
};

} // namespace Mg
