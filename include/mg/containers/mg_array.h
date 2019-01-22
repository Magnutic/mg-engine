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

/** @file mg_array.h
 * Heap-allocated array with ownership semantics, like a lightweight, non-resizable, non-copyable
 * std::vector.
 *
 * Mg::Array is similar to std::unique_ptr<T[]>, but keeps track of the size of the array and
 * provides iterator interface.
 *
 * Mg::ArrayUnknownSize is largely equivalent to std::unique_ptr<T[]>, but avoids dependency on the
 * fairly large <memory> header.
 */

#pragma once

#include <utility>

namespace Mg {

/** Default deleter for Mg::Array and Mg::ArrayUnknownSize. */
template<typename T> class DefaultArrayDelete {
public:
    void operator()(T* ptr) { delete[] ptr; }
};

namespace detail {
// Base type common to Mg::Array and Mg::ArrayUnkownSize
template<typename T, typename DeleterT = DefaultArrayDelete<T>> class ArrayBase {
public:
    using value_type      = T;
    using size_type       = size_t;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using deleter         = DeleterT;

    ArrayBase() = default;

    explicit ArrayBase(T* raw_ptr) noexcept : m_ptr(raw_ptr) {}

    ~ArrayBase() { reset(); }

    ArrayBase(const ArrayBase&) = delete;
    ArrayBase& operator=(const ArrayBase&) = delete;

    T*       data() noexcept { return m_ptr; }
    const T* data() const noexcept { return m_ptr; }

protected:
    void reset() noexcept
    {
        if (m_ptr != nullptr) {
            deleter{}(m_ptr);
            m_ptr = nullptr;
        }
    }

    T* release() noexcept
    {
        T* ret = m_ptr;
        m_ptr  = nullptr;
        return ret;
    }

    T* m_ptr = nullptr;
};
} // namespace detail

/** Minimalistic container for heap-allocated arrays. Does not keep track of the array's size --
 * and, consequently, cannot offer out-of-bounds access checks -- and is as such best suited as a
 * building block for higher-level containers.
 */
template<typename T> class ArrayUnknownSize : public detail::ArrayBase<T> {
    using Base = detail::ArrayBase<T>;

public:
    template<typename... Args> static ArrayUnknownSize make(size_t size)
    {
        return ArrayUnknownSize(new T[size]());
    }

    ArrayUnknownSize() = default;

    explicit ArrayUnknownSize(T* ptr) noexcept : Base(ptr) {}

    ArrayUnknownSize(ArrayUnknownSize&& p) noexcept : Base(p.m_ptr) { p.release(); }

    ArrayUnknownSize& operator=(ArrayUnknownSize&& p) noexcept
    {
        ArrayUnknownSize temp(std::move(p));
        this->reset();
        swap(temp);
        return *this;
    }

    void swap(ArrayUnknownSize& p) noexcept { std::swap(this->m_ptr, p.m_ptr); }

    T&       operator[](size_t i) noexcept { return this->m_ptr[i]; }
    const T& operator[](size_t i) const noexcept { return this->m_ptr[i]; }
};

/** Minimalistic container for heap-allocated arrays. Tracks the size of the array and provides
 * out-of-bounds access checks. A light-weight alternative to std::vector, for when dynamic growing
 * is not required.
 */
template<typename T> class Array : public detail::ArrayBase<T> {
    using Base = detail::ArrayBase<T>;

public:
    using iterator       = T*;
    using const_iterator = const T*;

    template<typename... Args> static Array make(size_t size) { return Array(new T[size](), size); }

    Array() = default;

    explicit Array(T* ptr, size_t size) noexcept : Base(ptr), m_size(size) {}

    Array(Array&& p) noexcept : Base(p.m_ptr), m_size(p.m_size)
    {
        p.release();
        p.m_size = 0;
    }

    Array& operator=(Array&& p) noexcept
    {
        Array temp(std::move(p));
        this->reset();
        swap(temp);
        return *this;
    }

    void swap(Array& p) noexcept
    {
        std::swap(this->m_ptr, p.m_ptr);
        std::swap(m_size, p.m_size);
    }

    iterator       begin() noexcept { return this->m_ptr; }
    iterator       end() noexcept { return this->m_ptr + m_size; } // NOLINT
    const_iterator begin() const noexcept { return this->m_ptr; }
    const_iterator end() const noexcept { return this->m_ptr + m_size; } // NOLINT
    const_iterator cbegin() const noexcept { return this->m_ptr; }
    const_iterator cend() const noexcept { return this->m_ptr + m_size; } // NOLINT

    T& operator[](size_t i) noexcept
    {
        MG_ASSERT(i < m_size);
        return this->m_ptr[i];
    }

    const T& operator[](size_t i) const noexcept
    {
        MG_ASSERT(i < m_size);
        return this->m_ptr[i];
    }

    typename Base::size_type size() const noexcept { return size; }

private:
    typename Base::size_type m_size = 0;
};

} // namespace Mg
