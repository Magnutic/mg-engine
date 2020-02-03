//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
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

#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"

#include <utility>

namespace Mg {

/** Default deleter for Mg::Array and Mg::ArrayUnknownSize. */
template<typename T> class DefaultArrayDelete {
public:
    void operator()(T* ptr) noexcept { delete[] ptr; }
};

namespace detail {
// Base type common to Mg::Array and Mg::ArrayUnkownSize
template<typename T, typename DeleterT = DefaultArrayDelete<T>> class ArrayBase {
public:
    using value_type = T;
    using size_type = size_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using deleter = DeleterT;

    ArrayBase() = default;

    explicit ArrayBase(T* raw_ptr) noexcept : m_ptr(raw_ptr) {}

    ~ArrayBase() { reset(); }

    MG_MAKE_NON_COPYABLE(ArrayBase);
    MG_MAKE_DEFAULT_MOVABLE(ArrayBase);

    T* data() noexcept { return m_ptr; }
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
        m_ptr = nullptr;
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
    static ArrayUnknownSize make(size_t size) { return ArrayUnknownSize(new T[size]()); }

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

    MG_MAKE_NON_COPYABLE(ArrayUnknownSize);

    ~ArrayUnknownSize() = default;

    void swap(ArrayUnknownSize& p) noexcept { std::swap(this->m_ptr, p.m_ptr); }

    T& operator[](size_t i) noexcept { return this->m_ptr[i]; }
    const T& operator[](size_t i) const noexcept { return this->m_ptr[i]; }
};

/** Minimalistic container for heap-allocated arrays. Tracks the size of the array and provides
 * out-of-bounds access checks. A light-weight alternative to std::vector, for when dynamic growth
 * is not required.
 */
template<typename T> class Array : public detail::ArrayBase<T> {
    using Base = detail::ArrayBase<T>;

public:
    using iterator = T*;
    using const_iterator = const T*;

    static Array make(size_t size) { return Array(new T[size](), size); }

    static Array<T> make_copy(span<const T> data)
    {
        struct alignas(T) block {
            std::byte storage[sizeof(T)];
        };

        block* p_storage = nullptr;
        size_t i = 0;
        const size_t num_elems = data.size();

        try {
            // Allocate storage.
            p_storage = new block[num_elems];

            // Construct copies in storage.
            for (; i < num_elems; ++i) {
                new (&p_storage[i]) T(data[i]);
            }
        }
        catch (...) {
            // If an exception was thrown, destroy all the so-far constructed objects in reverse
            // order of construction.
            for (size_t u = 0; u < i; ++u) {
                const auto index = i - u - 1;
                reinterpret_cast<T*>(p_storage)[index].~T(); // NOLINT
            }

            throw;
        }

        return Array<T>(reinterpret_cast<T*>(p_storage), num_elems); // NOLINT
    }

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

    MG_MAKE_NON_COPYABLE(Array);

    ~Array() = default;

    void swap(Array& p) noexcept
    {
        std::swap(this->m_ptr, p.m_ptr);
        std::swap(m_size, p.m_size);
    }

    iterator begin() noexcept { return this->m_ptr; }
    iterator end() noexcept { return this->m_ptr + m_size; } // NOLINT
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

    typename Base::size_type size() const noexcept { return m_size; }

private:
    typename Base::size_type m_size = 0;
};

} // namespace Mg
