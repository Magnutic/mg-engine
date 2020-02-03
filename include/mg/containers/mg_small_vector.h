//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#pragma once

// Only include smaller stdlib headers -- keeps preprocessed code size fairly small.
#include <cstddef>
#include <type_traits>
#include <utility>

// What assertion function or macro to use.
#ifndef MG_SMALL_VECTOR_ASSERT
#    include <cassert>
#    define MG_SMALL_VECTOR_ASSERT(exp) assert(exp)
#endif

// Disable warning "[...] m_capacity maybe uninitialized in this function.".
// (m_capacity is a union member which is managed manually).
#if defined(__GNUC__) && !defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

namespace Mg {

namespace detail {

// RAII-type for managing external vector-data buffers (i.e. the vector data when it does not fit
// inside the internal buffer).
// This could also be done using std::unique_ptr, but this type is slightly more convenient to use
// and -- more importantly -- avoids the #include-dependency on <memory>, which is suprisingly
// heavy.
template<typename T> class ExternalBuffer {
public:
    ExternalBuffer() = default;
    ~ExternalBuffer() { delete[] m_p_data; }

    ExternalBuffer(const ExternalBuffer&) = delete;
    const ExternalBuffer& operator=(const ExternalBuffer& rhs) = delete;

    ExternalBuffer(ExternalBuffer&& rhs) noexcept : m_p_data(rhs.m_p_data) { rhs.m_p_data = {}; }

    ExternalBuffer& operator=(ExternalBuffer&& rhs) noexcept
    {
        ExternalBuffer tmp(std::move(rhs));
        swap(tmp);
        return *this;
    }

    static ExternalBuffer allocate(std::size_t num)
    {
        ExternalBuffer eb;
        eb.m_p_data = new T[num];
        return eb;
    }

    void swap(ExternalBuffer& rhs) noexcept { std::swap(m_p_data, rhs.m_p_data); }

    T* get() const noexcept { return m_p_data; }
    T& operator[](std::size_t i) const noexcept { return m_p_data[i]; }

private:
    T* m_p_data = nullptr;
};

// Equivalent to std::rotate, implemented here to avoid dependency on the <algorithm> header.
// Implementation adapted from cppreference.com, license:
// https://creativecommons.org/licenses/by-sa/3.0/
template<class ForwardIt> ForwardIt rotate(ForwardIt first, ForwardIt n_first, ForwardIt last)
{
    if (first == n_first) {
        return last;
    }
    if (n_first == last) {
        return first;
    }

    ForwardIt read = n_first;
    ForwardIt write = first;
    ForwardIt next_read = first; // Read position for when "read" hits "last".

    while (read != last) {
        if (write == next_read) {
            next_read = read; // Track where "first" went.
        }
        std::swap(*(write++), *(read++));
    }

    // Rotate the remaining sequence into place.
    rotate(write, next_read, last);
    return write;
}

// Compare two ranges lexicographically; returns -1 if range1 is smaller, returns 1 if range1 is
// larger, returns 0 if they are equal.
template<class InputIt1, class InputIt2>
int range_compare(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2)
{
    for (; (first1 != last1) && (first2 != last2); ++first1, ++first2) {
        if (*first1 < *first2) {
            return -1;
        }
        if (*first2 < *first1) {
            return 1;
        }
    }

    if (first1 == last1 && first2 == last2) {
        // Equal-length ranges.
        return 0;
    }

    if (first1 == last1 && first2 != last2) {
        // Range 2 is longer.
        return -1;
    }

    // Range 1 is longer.
    return 1;
}

} // namespace detail

/** Mg::small_vector is a dynamically sized array, similar to std::vector, with the difference that
 * a certain space is reserved in the small_vector object for local storage of small vectors.
 *
 * The small_vector container can be ideal for arrays that are small in the average case, as it
 * avoids dynamic memory allocations and the associated memory fragmentation.
 *
 * If the number of elements grows beyond the size of the local storage, the container falls back to
 * dynamic allocation, behaving like std::vector.
 *
 * For simplicity and smaller code size, Mg::small_vector only implements a subset of std::vector's
 * interface. Range-based construction and assignment are not supported, and neither are
 * reverse-iterator getters (rbegin(), rend(), etc.).
 *
 * @tparam T Element type
 * @tparam num_local_elems Number of elements for which local storage is reserved.
 */
template<typename T, std::size_t num_local_elems> class small_vector {
    // Implementation-affecting qualities of the element type.
    static constexpr bool trivial_copy = std::is_trivially_copyable<T>::value;
    static constexpr bool nothrow_move = std::is_nothrow_move_constructible<T>::value;
    static constexpr bool nothrow_swap = trivial_copy || nothrow_move;

    struct alignas(T) elem_data_t {
        unsigned char data[sizeof(T)];
    };

    using ExternalBuffer = detail::ExternalBuffer<elem_data_t>;

    // According to the following source, 1.5 is a good choice for a vector's growth factor:
    // https://github.com/facebook/folly/blob/master/folly/docs/FBVector.md
    static constexpr double k_growth_factor = 1.5;

public:
    using value_type = T;

    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = T*;
    using const_pointer = const T*;

    using iterator = pointer;
    using const_iterator = const_pointer;


    // Constructors
    // ---------------------------------------------------------------------------------------------

    /** Default-constructor: creates empty small_vector. */
    small_vector() noexcept : m_size(0), m_uses_external_storage(0) {}

    ~small_vector() { clear(); }

    small_vector(const small_vector& rhs) : small_vector()
    {
        reserve(rhs.size());
        for (const T& elem : rhs) {
            push_back(elem);
        }
    }

    small_vector(small_vector&& rhs) noexcept : small_vector()
    {
        if (rhs.uses_local_storage()) {
            for (T& elem : rhs) {
                push_back(std::move_if_noexcept(elem));
            }
        }
        else {
            _switch_to_external_storage(std::move(rhs.m_external_buffer), rhs.capacity());
            m_size = rhs.size();
            rhs._switch_to_local_storage(0);
        }
    }

    /** Construct small_vector with `count` number of copies of `value`. */
    explicit small_vector(size_type count, const T& value) : small_vector()
    {
        while (count--) {
            push_back(value);
        }
    }

    /** Construct small_vector with `count` number of default-constructed elements.
     * Requires that the element_type is default-constructible.
     */
    explicit small_vector(size_type count) : small_vector(count, T()) {}

    /** Construct by copying elements from initializer-list. */
    small_vector(std::initializer_list<T> init) : small_vector()
    {
        reserve(init.size());
        for (const T& elem : init) {
            push_back(elem);
        }
    }

    // Assignment
    // ---------------------------------------------------------------------------------------------
    small_vector& operator=(const small_vector& rhs)
    {
        small_vector tmp(rhs);
        swap(tmp);
        return *this;
    }

    small_vector& operator=(small_vector&& rhs) noexcept(nothrow_swap)
    {
        small_vector tmp(std::move(rhs));
        swap(tmp);
        return *this;
    }

    small_vector& operator=(std::initializer_list<T> init)
    {
        small_vector tmp(init);
        swap(tmp);
        return *this;
    }

    void assign(size_type count, const T& value)
    {
        small_vector tmp(count, value);
        swap(tmp);
    }

    void assign(std::initializer_list<T> init)
    {
        small_vector tmp(init);
        swap(tmp);
    }

    // Element access
    // ---------------------------------------------------------------------------------------------

    /** Bounds-checked element access. */
    reference at(size_type index)
    {
        MG_SMALL_VECTOR_ASSERT(index < size());
        return data()[index];
    }

    /** Bounds-checked element access. */
    const_reference at(size_type index) const
    {
        MG_SMALL_VECTOR_ASSERT(index < size());
        return data()[index];
    }

    reference operator[](size_type index) noexcept { return at(index); }
    const_reference operator[](size_type index) const noexcept { return at(index); };

    reference front() { return at(0); }
    const_reference front() const { return at(0); }

    reference back() { return at(size() - 1); }
    const_reference back() const { return at(size() - 1); }

    pointer data() noexcept { return reinterpret_cast<pointer>(_storage_ptr()); }

    const_pointer data() const noexcept { return reinterpret_cast<const_pointer>(_storage_ptr()); }

    // Iterators
    // ---------------------------------------------------------------------------------------------

    iterator begin() noexcept { return data(); }
    const_iterator begin() const noexcept { return data(); }
    const_iterator cbegin() const noexcept { return data(); }

    iterator end() noexcept { return data() + size(); }
    const_iterator end() const noexcept { return data() + size(); }
    const_iterator cend() const noexcept { return data() + size(); }

    // Capacity
    // ---------------------------------------------------------------------------------------------

    /** Returns whether the container is empty. */
    bool empty() const noexcept { return size() == 0; }

    /** Returns the number of elements in the container. */
    size_type size() const noexcept { return m_size; }

    /** Returns maximum number of elements that this container could theoretically store -- in the
     * absence of memory limitations.
     */
    size_type max_size() const noexcept { return size_type{ 1 } << (sizeof(size_type) * 8 - 1); }

    /** Returns the size of the local element storage -- the number of elements that this container
     * can hold without falling back to dynamic memory allocation.
     */
    size_type local_size() const noexcept { return num_local_elems; }

    /** Returns whether the small_vector is currently using the reserved local storage. */
    bool uses_local_storage() const noexcept { return m_uses_external_storage == 0; }

    /** Reserve storage for the number of elements given by `new_capacity`.
     * If `new_capacity` exceeds the `num_local_elems` template parameter, this will result in a
     * dynamic memory allocation.
     */
    void reserve(size_type new_capacity)
    {
        if (new_capacity <= capacity()) {
            return;
        }

        MG_SMALL_VECTOR_ASSERT(new_capacity < max_size());
        _switch_to_external_storage(ExternalBuffer::allocate(new_capacity), new_capacity);
    }

    void shrink_to_fit()
    {
        if (uses_local_storage()) {
            return;
        }

        auto buf = ExternalBuffer::allocate(size());
        _move_elems_between_buffers(m_external_buffer.get(), buf.get(), size());
        m_external_buffer = std::move(buf);
        m_capacity = size();
    }

    /** Returns the number of elements that this container is able to store without further memory
     * allocations.
     */
    size_type capacity() const noexcept { return uses_local_storage() ? local_size() : m_capacity; }

    // Modifiers
    // ---------------------------------------------------------------------------------------------

    /** Destroy all elements and reset the containers size to 0. The resulting state is equivalent
     * to a default-constructed small_vector.
     */
    void clear() noexcept
    {
        size_type index = size();
        while (index != 0) {
            _destroy_at(--index);
        }

        if (!uses_local_storage()) {
            _switch_to_local_storage(0);
        }

        m_size = 0;
    }

    iterator insert(const_iterator pos, const T& value) { return emplace(pos, value); }

    iterator insert(const_iterator pos, T&& value) { return emplace(pos, std::move(value)); }

    iterator insert(const_iterator pos, size_type count, const T& value)
    {
        auto index = _distance(cbegin(), pos);
        reserve(size() + count);
        while (count--) {
            emplace_back(value);
        }
        _move_last_n_to_pos(index, 1);
        return begin() + index;
    }

    template<typename... Args> iterator emplace(const_iterator pos, Args&&... args)
    {
        size_type index = _distance(cbegin(), pos);
        emplace_back(std::forward<Args>(args)...);
        _move_last_n_to_pos(index, 1);
        return begin() + index;
    }

    /** Insert a range of elements at a specified position.
     *
     * @param pos Iterator indicating the position in which the range is to be inserted.
     * @param first Start iterator of the range of elements to insert.
     * @param last End iterator of the range of elements to insert.
     *
     * @note The iterator range [first, last) may not be iterators into this container.
     * @remark This function provides weak exception safety.
     */
    template<typename InputIt> iterator insert(const_iterator pos, InputIt first, InputIt last)
    {
        // Order is important here: call to reserve() might invalidate pos.
        size_type index = _distance(cbegin(), pos);
        return _insert_range(index, first, last);
    }

    iterator insert(const_iterator pos, std::initializer_list<T> init)
    {
        insert(pos, init.begin(), init.end());
        size_type index = _distance(cbegin(), pos);
        reserve(size() + init.size());

        for (const T& value : init) {
            emplace_back(value);
        }

        _move_last_n_to_pos(index, init.size());
        return begin() + index;
    }

    /** Erase the element indicated by the given iterator. */
    iterator erase(const_iterator pos)
    {
        iterator ret = begin() + _distance(cbegin(), pos);
        _move_to_end(pos, 1);
        _destroy_at(size() - 1);
        --m_size;
        return ret;
    }

    /** Erase a range of elements. */
    iterator erase(const_iterator first, const_iterator last)
    {
        iterator ret = begin() + _distance(cbegin(), first);
        auto new_end = _move_to_end(first, _distance(first, last));
        auto it = end();
        while (it-- != new_end) {
            it->~T();
            --m_size;
        }

        return ret;
    }

    void push_back(const T& value) { emplace_back(value); }

    void push_back(T&& value) { emplace_back(std::move(value)); }

    template<typename... Args> reference emplace_back(Args&&... args)
    {
        static_assert(std::is_constructible<T, Args...>::value,
                      "small_vector::emplace_back(): T cannot be "
                      "constructed with the supplied parameters.");

        if (size() < capacity()) {
            // Construct new elem at end-position.
            new (&_storage_ptr()[size()]) T(std::forward<Args>(args)...);
        }
        else {
            // Make sure to allocate new buffer and construct new element there _before_ moving old
            // elements. This prevents dangling reference errors when args depend on an element in
            // this vector (e.g. v.emplace_back(v[0]); )

            size_type new_capacity = static_cast<size_type>(capacity() * k_growth_factor);

            // If k_growth_factor is not large enough, new_capacity might not be any larger.
            // This is especially likely to happen if initial capacity was very small.
            if (new_capacity == capacity()) {
                new_capacity += 1;
            }
            auto new_buffer = ExternalBuffer::allocate(new_capacity);

            // Construct new elem at end-position in newly allocated buffer.
            new (&new_buffer[size()]) T(std::forward<Args>(args)...);

            // Move all old elements to new buffer.
            _switch_to_external_storage(std::move(new_buffer), new_capacity);
        }

        return (*this)[m_size++];
    }

    void pop_back() noexcept
    {
        _destroy_at(size() - 1);
        --m_size;
    }

    void resize(size_type count)
    {
        static_assert(std::is_default_constructible<T>::value,
                      "small_vector::resize() with no value argument: "
                      "T is not default constructible.");

        _resize_impl(count);
    }

    void resize(size_type count, const T& value) { _resize_impl(count, value); }

    void swap(small_vector& rhs) noexcept(nothrow_swap)
    {
        if (uses_local_storage() && rhs.uses_local_storage()) {
            _swap_local(rhs);
            return;
        }
        if (!uses_local_storage() && !rhs.uses_local_storage()) {
            _swap_external(rhs);
            return;
        }
        if (uses_local_storage() && !rhs.uses_local_storage()) {
            _swap_this_local_rhs_external(rhs);
            return;
        }
        if (!uses_local_storage() && rhs.uses_local_storage()) {
            rhs._swap_this_local_rhs_external(*this);
            return;
        }
    }

private:
    static size_type _distance(const_iterator begin, const_iterator end)
    {
        MG_SMALL_VECTOR_ASSERT(begin <= end);
        return static_cast<size_type>(end - begin);
    }
    // Element handling helpers

    elem_data_t* _storage_ptr() noexcept
    {
        return uses_local_storage() ? &m_buffer[0] : m_external_buffer.get();
    }

    const elem_data_t* _storage_ptr() const noexcept
    {
        return uses_local_storage() ? &m_buffer[0] : m_external_buffer.get();
    }

    template<typename... Args>
    pointer _construct_in_buffer_at(elem_data_t* buffer, size_type index, Args&&... args)
    {
        return new (&buffer[index]) value_type(std::forward<Args>(args)...);
    }

    template<typename... Args> pointer _construct_at(size_type index, Args&&... args)
    {
        return _construct_in_buffer_at(_storage_ptr(), index, std::forward<Args>(args)...);
    }

    void _destroy_in_buffer_at(const elem_data_t* buffer, size_type index) noexcept
    {
        MG_SMALL_VECTOR_ASSERT(buffer != nullptr);
        reinterpret_cast<const_pointer>(buffer + index)->~T();
    }

    void _destroy_at(size_type index) noexcept { _destroy_in_buffer_at(_storage_ptr(), index); }

    // Swap helpers

    // Swap for the case where both operands use local storage.
    void _swap_local(small_vector& rhs) noexcept(nothrow_swap)
    {
        MG_SMALL_VECTOR_ASSERT(uses_local_storage());
        MG_SMALL_VECTOR_ASSERT(rhs.uses_local_storage());

        if (trivial_copy) {
            _swap_local_trivial(rhs);
        }
        else {
            _swap_local_non_trivial(rhs);
        }
    }

    // Swap implementation for the case where both operands use local storage and the element
    // type is trivially copyable (POD).
    void _swap_local_trivial(small_vector& rhs) noexcept
    {
        using std::swap;
        swap(m_buffer, rhs.m_buffer);

        // Cannot swap bit fields
        const auto tmp = m_size;
        m_size = rhs.m_size;
        rhs.m_size = tmp;
    }

    // Swap implementation for the case where both operands use local storage and the element
    // type is not trivally copyable (non-POD). (noexcept commented due to warning.)
    void _swap_local_non_trivial(small_vector& rhs) /* noexcept(nothrow_move) */
    {
        using std::swap;
        small_vector& small_vec = (size() < rhs.size()) ? *this : rhs;
        small_vector& large_vec = (size() >= rhs.size()) ? *this : rhs;

        const size_type small_size = small_vec.size();
        const size_type large_size = large_vec.size();

        try {
            // Move elements from larger vector into empty locations in smaller vector
            for (size_type i = small_size; i < large_size; ++i) {
                small_vec._construct_at(i, std::move_if_noexcept(large_vec[i]));
                ++small_vec.m_size;
            }
        }
        catch (...) {
            // Strong exception safety:
            // Storage is guaranteed to be local both before and after, so resize should not throw.
            small_vec._shrink_to(small_size);
            throw;
        }

        // Swap those elements which exist at the same index in both vectors
        for (size_type i = 0; i < small_size; ++i) {
            swap((*this)[i], rhs[i]);
        }

        // Remove the (moved-from) values at the end of the larger vector
        large_vec._shrink_to(small_size);
    }

    // Swap for the case where both operands use external storage.
    void _swap_external(small_vector& rhs) noexcept
    {
        MG_SMALL_VECTOR_ASSERT(!uses_local_storage());
        MG_SMALL_VECTOR_ASSERT(!rhs.uses_local_storage());

        using std::swap;
        swap(m_external_buffer, rhs.m_external_buffer);
        swap(m_capacity, rhs.m_capacity);

        // Cannot swap bit fields
        const auto tmp = m_size;
        m_size = rhs.m_size;
        rhs.m_size = tmp;
    }

    // Swap for the case where this is using local storage, and rhs is using external storage.
    // noexcept commented out due to warning.
    void _swap_this_local_rhs_external(small_vector& rhs) /* noexcept(nothrow_swap) */
    {
        MG_SMALL_VECTOR_ASSERT(uses_local_storage());
        MG_SMALL_VECTOR_ASSERT(!rhs.uses_local_storage());

        // tmp_vec gets rhs' data; rhs becomes empty.
        auto tmp_vec(std::move(rhs));

        try {
            // Now rhs gets this' data; this becomes empty.
            _swap_local(rhs);
        }
        catch (...) {
            // Restore rhs
            rhs = std::move(tmp_vec);
            throw;
        }

        // This puts the vector in an invalid state, but it is going to be swapped with tmp_vec
        // which will be immediately destroyed.
        _switch_to_external_storage(ExternalBuffer{}, 0);

        // Now this gets rhs' data.
        _swap_external(tmp_vec);
    }

    template<typename... Args> void _resize_impl(size_type count, Args&&... args)
    {
        if (count > size()) {
            // This temporary stack copy is strictly speaking unnecessary; see emplace_back().
            // This is, however, simpler, and resize() is probably not the most performance
            // sensitive function.
            T tmp(std::forward<Args>(args)...);
            reserve(count);

            while (size() < count) {
                _construct_at(m_size++, tmp);
            }
        }

        _shrink_to(count);
    }

    void _shrink_to(size_type count)
    {
        MG_SMALL_VECTOR_ASSERT(count <= size());

        if (!uses_local_storage() && count <= num_local_elems) {
            _switch_to_local_storage(count);
            return;
        }

        while (size() > count) {
            _destroy_at(size() - 1);
            --m_size;
        }
    }

    // Move elements between the two buffers, destroying elements in the old buffer.
    // Strong exception-safety. (noexcept commented out due to warnings.)
    void _move_elems_between_buffers(elem_data_t* src,
                                     elem_data_t* dst,
                                     size_type num) /* noexcept(nothrow_move) */
    {
        MG_SMALL_VECTOR_ASSERT(src != nullptr);
        MG_SMALL_VECTOR_ASSERT(dst != nullptr);

        size_type i = 0;

        try {
            for (i = 0; i < num; ++i) {
                T& elem = reinterpret_cast<T&>(src[i]);
                new (&dst[i]) T(std::move_if_noexcept(elem));
            }
        }
        catch (...) {
            // If copying failed, destroy the copies that were made.
            for (size_type u = 0; u < i; ++u) {
                // Destroy copies in new buffer.
                const elem_data_t* addr = &dst[i - 1 - u];
                reinterpret_cast<const_pointer>(addr)->~T();
            }

            throw;
        }

        // If successful, destroy original elements
        for (i = 0; i < num; ++i) {
            reinterpret_cast<pointer>(&src[num - 1 - i])->~T();
        }
    }

    // Switch to local buffer for element storage. (noexcept commented due to warnings).
    void _switch_to_local_storage(size_type elems_to_move) /* noexcept(nothrow_move) */
    {
        MG_SMALL_VECTOR_ASSERT(!uses_local_storage());
        MG_SMALL_VECTOR_ASSERT(elems_to_move <= num_local_elems);

        // Since m_capacity is in union with local buffer, we need to copy it before writing to
        // local buffer.
        const size_type tmp_capacity = m_capacity;
        ExternalBuffer tmp_buffer = std::move(m_external_buffer);

        // Try to move -- with strong exception guarantee.
        try {
            m_external_buffer.~ExternalBuffer();
            _move_elems_between_buffers(tmp_buffer.get(), &m_buffer[0], elems_to_move);
        }
        catch (...) {
            // Restore external storage on failure
            m_capacity = tmp_capacity;
            new (&m_external_buffer) ExternalBuffer(std::move(tmp_buffer));
            throw;
        }

        m_size = elems_to_move;
        m_uses_external_storage = 0;
    }

    // Switch to new external buffer for element storage.
    void _switch_to_external_storage(ExternalBuffer&& new_buffer,
                                     size_type new_capacity) noexcept(nothrow_move)
    {
        MG_SMALL_VECTOR_ASSERT(new_capacity >= size());

        _move_elems_between_buffers(_storage_ptr(), new_buffer.get(), size());

        if (uses_local_storage()) {
            // Initialise external-buffer-owning pointer
            new (&m_external_buffer) ExternalBuffer();
        }

        m_external_buffer = std::move(new_buffer);
        m_capacity = new_capacity;
        m_uses_external_storage = 1;
    }

    // Rotates the last num elems to start at pos (invoked by position insert/emplace)
    void _move_last_n_to_pos(size_type index, size_type num)
    {
        MG_SMALL_VECTOR_ASSERT(size() - num >= index);
        detail::rotate(begin() + index, end() - num, end());
    }

    // Moves num elements at pos to end. Returns new position iterator.
    iterator _move_to_end(const_iterator pos, size_type num)
    {
        const auto mut_pos = begin() + _distance(cbegin(), pos);
        MG_SMALL_VECTOR_ASSERT(mut_pos + num <= end());
        return detail::rotate(mut_pos, mut_pos + num, end());
    }

    // Implementation for range-insert overloads.
    template<typename InputIt> iterator _insert_range(size_type index, InputIt first, InputIt last)
    {
        size_type num_elems = 0;
        for (InputIt it = first; it != last; ++it) {
            emplace_back(*it);
            ++num_elems;
        }

        _move_last_n_to_pos(index, num_elems);
        return begin() + index;
    }

private:
    // We do not need access to capacity and local buffer at the same time.
    union {
        elem_data_t m_buffer[num_local_elems];
        struct {
            size_type m_capacity;
            ExternalBuffer m_external_buffer;
        };
    };

    // We use only one bit of the size variable to store whether we are using external storage.
    size_type m_size : sizeof(size_type) * 8 - 1;
    size_type m_uses_external_storage : 1;
};

// Comparison operators

template<typename T, std::size_t num_local_elems_a, std::size_t num_local_elems_b>
bool operator==(const small_vector<T, num_local_elems_a>& a,
                const small_vector<T, num_local_elems_b>& b)
{
    return a.size() == b.size() &&
           detail::range_compare(a.begin(), a.end(), b.begin(), b.end()) == 0;
}

template<typename T, std::size_t num_local_elems_a, std::size_t num_local_elems_b>
bool operator!=(const small_vector<T, num_local_elems_a>& a,
                const small_vector<T, num_local_elems_b>& b)
{
    return a.size() != b.size() ||
           detail::range_compare(a.begin(), a.end(), b.begin(), b.end()) != 0;
}

template<typename T, std::size_t num_local_elems_a, std::size_t num_local_elems_b>
bool operator<(const small_vector<T, num_local_elems_a>& a,
               const small_vector<T, num_local_elems_b>& b)
{
    return detail::range_compare(a.begin(), a.end(), b.begin(), b.end()) < 0;
}

template<typename T, std::size_t num_local_elems_a, std::size_t num_local_elems_b>
bool operator<=(const small_vector<T, num_local_elems_a>& a,
                const small_vector<T, num_local_elems_b>& b)
{
    return detail::range_compare(a.begin(), a.end(), b.begin(), b.end()) <= 0;
}

template<typename T, std::size_t num_local_elems_a, std::size_t num_local_elems_b>
bool operator>(const small_vector<T, num_local_elems_a>& a,
               const small_vector<T, num_local_elems_b>& b)
{
    return detail::range_compare(a.begin(), a.end(), b.begin(), b.end()) > 0;
}

template<typename T, std::size_t num_local_elems_a, std::size_t num_local_elems_b>
bool operator>=(const small_vector<T, num_local_elems_a>& a,
                const small_vector<T, num_local_elems_b>& b)
{
    return detail::range_compare(a.begin(), a.end(), b.begin(), b.end()) >= 0;
}

// Swap

template<typename T, std::size_t num_local_elems_a, std::size_t num_local_elems_b>
void swap(small_vector<T, num_local_elems_a>& a,
          small_vector<T, num_local_elems_b>& b) noexcept(noexcept(a.swap(b)))
{
    a.swap(b);
}

} // namespace Mg

#if defined(__GNUC__) && !defined(__clang__)
#    pragma GCC diagnostic pop
#endif
