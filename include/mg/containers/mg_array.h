//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
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

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <type_traits>

namespace Mg {

namespace detail {

enum class InitTag { GeneratorInit, DefaultInit, ValueInit };

template<typename T, InitTag init = InitTag::GeneratorInit, typename Generator = void>
static T* make_array(const size_t num_elems, Generator* generator = nullptr)
{
    if (num_elems == 0) {
        return nullptr;
    }

    void* buffer = nullptr;
    char* array_begin = nullptr;
    size_t i = 0;

    try {
        // Allocate storage.
        buffer = std::malloc(num_elems * sizeof(T) + sizeof(size_t)); // NOLINT

        // Write length of array to start of storage.
        new (buffer) size_t(num_elems);

        // Get address where array contents will go.
        // Note: to be perfectly portable, we would need to advance the correct amount to reach
        // an aligned address for alignof(T), but here I assume that sizeof(size_t) corresponds to
        // maximum alignment.
        array_begin = static_cast<char*>(buffer) + sizeof(size_t); // NOLINT

        // Construct copies in storage.
        for (; i < num_elems; ++i) {
            auto* ptr = array_begin + sizeof(T) * i; // NOLINT

            if constexpr (init == InitTag::DefaultInit) {
                new (ptr) T;
            }
            else if constexpr (init == InitTag::ValueInit) {
                new (ptr) T();
            }
            else {
                new (ptr) T((*generator)());
            }
        }
    }
    catch (...) {
        // If an exception was thrown, destroy all the so-far constructed objects in reverse
        // order of construction.
        for (size_t u = 0; u < i; ++u) {
            reinterpret_cast<T*>(array_begin)[i - u - 1].~T(); // NOLINT
        }

        throw;
    }

    return reinterpret_cast<T*>(array_begin); // NOLINT
}

template<typename T> static void destroy_array(T* array_begin)
{
    auto* buffer = reinterpret_cast<char*>(array_begin) - sizeof(size_t); // NOLINT
    const size_t num_elems = *reinterpret_cast<size_t*>(buffer);

    for (size_t i = 0; i < num_elems; ++i) {
        array_begin[num_elems - i - 1].~T(); // NOLINT
    }

    std::free(buffer); // NOLINT
}

template<typename T> class ArrayCopyGenerator {
public:
    // NOLINTNEXTLINE false positive
    explicit ArrayCopyGenerator(const span<const T> source) : m_source(source) {}
    const T& operator()() { return m_source[m_index++]; }

private:
    span<const T> m_source;
    size_t m_index = 0;
};

template<typename T> class InitializerListGenerator {
public:
    explicit InitializerListGenerator(const std::initializer_list<T>& ilist) : m_it(ilist.begin())
    {}

    const T& operator()() { return *(m_it++); }

private:
    typename std::initializer_list<T>::const_iterator m_it;
};

template<typename T> class ValueCopyGenerator {
public:
    explicit ValueCopyGenerator(const T& value) : m_value(value) {}
    const T& operator()() { return m_value; }

private:
    const T& m_value;
};

// Base type common to Mg::Array and Mg::ArrayUnkownSize
template<typename T> class ArrayBase {
public:
    using value_type = T;
    using size_type = size_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;

    ArrayBase() = default;

    explicit ArrayBase(T* raw_ptr) noexcept : m_ptr(raw_ptr) {}

    ~ArrayBase() { reset(); }

    MG_MAKE_NON_COPYABLE(ArrayBase);
    MG_MAKE_DEFAULT_MOVABLE(ArrayBase);

    T* data() noexcept { return m_ptr; }
    const T* data() const noexcept { return m_ptr; }

    bool empty() const noexcept { return m_ptr == nullptr; }

protected:
    void reset() noexcept
    {
        static_assert(sizeof(T) != 0); // Ensure type is complete before deleting.
        if (m_ptr != nullptr) {
            destroy_array(m_ptr);
            m_ptr = nullptr;
        }
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
    static ArrayUnknownSize make(size_t size)
    {
        return size == 0
                   ? ArrayUnknownSize{}
                   : ArrayUnknownSize(detail::make_array<T, detail::InitTag::ValueInit>(size));
    }

    static ArrayUnknownSize make_for_overwrite(size_t size)
    {
        return size == 0
                   ? ArrayUnknownSize{}
                   : ArrayUnknownSize(detail::make_array<T, detail::InitTag::DefaultInit>(size));
    }

    static ArrayUnknownSize make(size_t size, const T& value)
    {
        detail::ValueCopyGenerator gen(value);
        if (size == 0) {
            return ArrayUnknownSize{};
        }
        return ArrayUnknownSize(detail::make_array<T>(size, &gen));
    }

    static ArrayUnknownSize make(std::initializer_list<T> ilist)
    {
        const auto size = narrow<size_t>(ilist.end() - ilist.begin());
        if (size == 0) {
            return ArrayUnknownSize{};
        }
        detail::InitializerListGenerator gen(ilist);
        return ArrayUnknownSize(detail::make_array<T>(size, &gen), size);
    }

    static ArrayUnknownSize make_copy(span<const T> data)
    {
        if (data.empty()) {
            return ArrayUnknownSize{};
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            ArrayUnknownSize result = make_for_overwrite(data.size());
            std::memcpy(result.data(), data.data(), data.size_bytes());
            return result;
        }
        else {
            detail::ArrayCopyGenerator gen(data);
            return ArrayUnknownSize<T>(detail::make_array<T>(data.size(), &gen));
        }
    }

    ArrayUnknownSize() = default;

    ArrayUnknownSize(std::nullptr_t) : Base(nullptr) {}

    ArrayUnknownSize(ArrayUnknownSize&& rhs) noexcept : Base(rhs.m_ptr) { rhs.m_ptr = nullptr; }

    ArrayUnknownSize& operator=(ArrayUnknownSize&& rhs) noexcept
    {
        ArrayUnknownSize temp(static_cast<ArrayUnknownSize&&>(rhs));
        this->reset();
        swap(temp);
        return *this;
    }

    MG_MAKE_NON_COPYABLE(ArrayUnknownSize);

    ~ArrayUnknownSize() = default;

    void clear() noexcept { this->reset(); }

    void swap(ArrayUnknownSize& rhs) noexcept { std::swap(this->m_ptr, rhs.m_ptr); }

    T& operator[](size_t i) noexcept
    {
        return this->m_ptr[i]; // NOLINT
    }

    const T& operator[](size_t i) const noexcept
    {
        return this->m_ptr[i]; // NOLINT
    }

private:
    explicit ArrayUnknownSize(T* ptr) noexcept : Base(ptr) {}
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

    static Array make(size_t size)
    {
        return size == 0 ? Array{}
                         : Array(detail::make_array<T, detail::InitTag::ValueInit>(size), size);
    }

    static Array make_for_overwrite(size_t size)
    {
        return size == 0 ? Array{}
                         : Array(detail::make_array<T, detail::InitTag::DefaultInit>(size), size);
    }

    static Array make(size_t size, const T& value)
    {
        if (size == 0) {
            return Array{};
        }
        detail::ValueCopyGenerator gen(value);
        return Array(detail::make_array<T>(size, &gen), size);
    }

    static Array make(std::initializer_list<T> ilist)
    {
        const auto size = narrow<size_t>(ilist.end() - ilist.begin());
        if (size == 0) {
            return Array{};
        }
        detail::InitializerListGenerator gen(ilist);
        return Array(detail::make_array<T>(size, &gen), size);
    }

    static Array make_copy(span<const T> data)
    {
        if (data.empty()) {
            return Array{};
        }

        if constexpr (std::is_trivially_copyable_v<T>) {
            Array result = make_for_overwrite(data.size());
            std::memcpy(result.data(), data.data(), data.size_bytes());
            return result;
        }
        else {
            detail::ArrayCopyGenerator gen(data);
            return Array(detail::make_array<T>(data.size(), &gen), data.size());
        }
    }

    /** Default constructor creates an empty array. */
    Array() = default;

    Array(std::nullptr_t) : Base(nullptr), m_size(0) {}

    Array(const Array& rhs) : Array(make_copy(rhs)) {}

    Array& operator=(const Array& rhs)
    {
        Array temp(rhs);
        this->reset();
        swap(temp);
        return *this;
    }

    Array(Array&& rhs) noexcept : Base(rhs.m_ptr), m_size(rhs.m_size)
    {
        rhs.m_ptr = nullptr;
        rhs.m_size = 0;
    }

    Array& operator=(Array&& rhs) noexcept
    {
        Array temp(static_cast<Array&&>(rhs));
        this->reset();
        swap(temp);
        return *this;
    }

    ~Array() { m_size = 0; }

    void clear() noexcept
    {
        this->reset();
        m_size = 0;
    }

    void swap(Array& rhs) noexcept
    {
        std::swap(this->m_ptr, rhs.m_ptr);
        std::swap(m_size, rhs.m_size);
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
        return this->m_ptr[i]; // NOLINT
    }

    const T& operator[](size_t i) const noexcept
    {
        MG_ASSERT(i < m_size);
        return this->m_ptr[i]; // NOLINT
    }

    T& back() noexcept
    {
        MG_ASSERT(m_size != 0);
        return this->m_ptr[m_size - 1]; // NOLINT
    }

    const T& back() const noexcept
    {
        MG_ASSERT(m_size != 0);
        return this->m_ptr[m_size - 1]; // NOLINT
    }

    T& front() noexcept
    {
        MG_ASSERT(m_size != 0);
        return this->m_ptr[0]; // NOLINT
    }

    const T& front() const noexcept
    {
        MG_ASSERT(m_size != 0);
        return this->m_ptr[0]; // NOLINT
    }

    typename Base::size_type size() const noexcept { return m_size; }

    bool empty() const noexcept { return m_size == 0; }

private:
    explicit Array(T* ptr, size_t size) noexcept : Base(ptr), m_size(size) {}

    typename Base::size_type m_size = 0;
};

} // namespace Mg
