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

/** @file mg_compacting_heap.h
 * Allocator which may defragment by compacting allocated memory, moving objects to close gaps.
 */

#pragma once

#include <cstddef> // std::max_align_t
#include <iterator>
#include <memory>
#include <type_traits>
#include <vector>

#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_ptr_math.h"

namespace Mg::memory {

//--------------------------------------------------------------------------------------------------
// Implementation helpers
//--------------------------------------------------------------------------------------------------

namespace detail {

/** Mover for compacting heap, implements moving of objects in a type-erased manner which does not
 * invoke UB.
 */
class CHMover {
public:
    virtual void move(void* dst, void* src, size_t num_elems) = 0;
    virtual void destroy(void* target, size_t num_elems)      = 0;
    virtual ~CHMover() {}
};

/** Implementation of CHMover for the given type. */
template<typename T> class CHMoverImpl : public CHMover {
public:
    using type = std::decay_t<T>;

    static_assert(std::is_move_constructible_v<type>,
                  "Types allocated in Mg::memory::CompactingHeap must be movable.");

    void move(void* dst, void* src, size_t num_elems) noexcept override
    {
        // It is difficult to make this function exception-safe, given that dst and src may overlap.
        // Instead, it is marked noexcept, meaning a throw in copy or move would result in an
        // immediate crash -- how often does a copy- or move-constructor throw, anyway? Allocation
        // failures are not a concern, since we use placement-new.

        for (size_t i = 0; i < num_elems; ++i) {
            type& src_ = *(reinterpret_cast<type*>(src) + i); // NOLINT
            void* dst_ = ptr_math::add(dst, i * sizeof(type));

            auto distance         = ptr_math::byte_difference(&src_, dst_);
            bool dst_src_overlaps = (std::abs(distance) < ptrdiff_t(sizeof(type)));

            // If destination and source overlap, make a temporary copy to avoid having construction
            // at destination overwrite source data.
            if (dst_src_overlaps) {
                type temp_copy(std::move_if_noexcept(src_));
                src_.~type();
                new (dst_) type(std::move(temp_copy));
            }
            else {
                new (dst_) type(std::move_if_noexcept(src_));
                src_.~type();
            }
        }
    }

    void destroy(void* target, size_t num_elems) noexcept override
    {
        for (size_t i = 0; i < num_elems; ++i) {
            type* ptr = reinterpret_cast<type*>(target) + i; // NOLINT
            ptr->~type();
        }
    }
};

template<typename T> class CHHandleBase;

} // namespace detail

template<typename T> class CHHandle;

//--------------------------------------------------------------------------------------------------
// CompactingHeap
//--------------------------------------------------------------------------------------------------

/** Allocator which may defragment by compacting allocated memory, moving objects to close gaps.
 * All objects allocated from a compacting heap must be referenced using CHHandle or CHPtr, since
 * the pointee data may move around in memory.
 */
class CompactingHeap {
public:
    template<typename T> friend class detail::CHHandleBase;
    template<typename T> friend class CHHandle;

    explicit CompactingHeap(size_t size_in_bytes)
        : m_data(std::make_unique<unsigned char[]>(size_in_bytes)), m_data_size(size_in_bytes)
    {
        m_alloc_info.reserve(k_initial_alloc_info_vector_size);
    }

    // Non-copyable, non-movable
    CompactingHeap(const CompactingHeap&) = delete;
    CompactingHeap(CompactingHeap&&)      = delete;
    CompactingHeap& operator=(const CompactingHeap&) = delete;
    CompactingHeap& operator=(CompactingHeap&&) = delete;

    //----------------------------------------------------------------------------------------------
    // Member functions

    /** Allocate a single `T` using supplied arguments.
     * @return Owning handle to allocated object. Use this handle to access the object -- data might
     * move around in memory, and the returned handle deals with that whereas a regular pointer /
     * reference would not.
     *
     * @tparam T Type of value to allocate.
     * @tparam Args Constructor argument types.
     * @param args Constructor arguments.
     */
    template<typename T, typename... Args, typename = std::enable_if_t<!std::is_array_v<T>>>
    CHHandle<T> alloc(Args&&... args)
    {
        size_t alloc_index = _alloc_impl(sizeof(T), 1);
        new (_alloc_info_at(alloc_index).start) T(std::forward<Args>(args)...);
        _alloc_info_at(alloc_index).mover = std::make_unique<detail::CHMoverImpl<T>>();
        return CHHandle<T>(this, alloc_index);
    }

    /** Allocate array.
     * @return Owning handle to allocated storage. Use this handle to access the array -- data might
     * move around in memory, and the returned handle deals with that whereas a regular pointer /
     * reference would not.
     *
     * @tparam T Type of array to allocate, must be dynamic array type.
     * @param num Extent of array to allocate
     */
    template<typename T, typename = std::enable_if_t<std::is_array_v<T> && std::extent_v<T> == 0>>
    CHHandle<T> alloc(size_t num)
    {
        using ElemT        = std::remove_extent_t<T>;
        size_t alloc_index = _alloc_impl(sizeof(ElemT), num);
        new (_alloc_info_at(alloc_index).start) ElemT[num]();
        _alloc_info_at(alloc_index).mover = std::make_unique<detail::CHMoverImpl<ElemT>>();
        return CHHandle<ElemT[]>(this, alloc_index);
    }

    /** Allocate an array of copies of the values in the supplied iterator range.
     * @return Owning handle to allocated array. Use this handle to access the array -- data might
     * move around in memory, and the returned handle deals with that whereas a regular pointer /
     * reference would not.
     */
    template<typename It, typename T = typename std::iterator_traits<It>::value_type>
    CHHandle<T[]> alloc_copy(It first, It last)
    {
        auto num_elems = std::distance(first, last);
        MG_ASSERT(num_elems >= 0);
        size_t alloc_index                = _alloc_impl(sizeof(T), static_cast<size_t>(num_elems));
        _alloc_info_at(alloc_index).mover = std::make_unique<detail::CHMoverImpl<T>>();

        auto elem_ptr = static_cast<char*>(_alloc_info_at(alloc_index).start);

        for (It it = first; it != last; ++it) {
            new (elem_ptr) T(*it);
            elem_ptr += sizeof(T); // NOLINT
        }

        return CHHandle<T[]>(this, alloc_index);
    }

    /** Compact (defragment) the heap by moving contained data. */
    void compact() noexcept;

    /** Return whether the heap has enough free space for `num` copies of `T` at the end. If not,
     * compacting the heap by calling `compact()` may free up enough space.
     */
    template<typename T> bool has_space_for(size_t num) const noexcept
    {
        return (_calculate_alloc_size(sizeof(T), num) + m_data_head <= m_data_size);
    }

    /** The number of bytes currently in use in this heap. */
    size_t num_used_bytes() const noexcept
    {
        MG_ASSERT(m_num_allocated_bytes >= 0);
        return static_cast<size_t>(m_num_allocated_bytes);
    }

    /** Size of buffer holding allocated data. */
    size_t buffer_size() const noexcept { return m_data_size; }

    /** The amount of free space (in bytes) in this heap. Note that this space may be fragmented,
     * so calling compact() might be necessary before further allocations.
     */
    size_t free_space() const noexcept { return buffer_size() - num_used_bytes(); }

private:
    struct AllocInfo {
        void*  start{};     // Pointer to first element of allocation.
        size_t num_elems{}; // Number of elements.
        size_t raw_size{};  // Size in bytes occupied by this allocation (N.B. may include padding).
        std::unique_ptr<detail::CHMover> mover{};
    };

    // Get reference to allocation info data of the allocation with the given index
    AllocInfo& _alloc_info_at(size_t index) noexcept
    {
        MG_ASSERT(index < m_alloc_info.size());
        return m_alloc_info[index];
    }

    size_t _alloc_impl(size_t elem_size, size_t num);

    size_t _calculate_alloc_size(size_t elem_size, size_t num) const noexcept
    {
        // Calculate size such that allocation ends on a properly aligned address.
        static constexpr size_t alignment = alignof(std::max_align_t);
        return (num * elem_size + alignment - 1) & ~(alignment - 1);
    }

    void _dealloc(size_t alloc_index)
    {
        AllocInfo& ai = _alloc_info_at(alloc_index);
        ai.mover->destroy(ai.start, ai.num_elems);
        m_num_allocated_bytes -= static_cast<ptrdiff_t>(ai.raw_size);
        MG_ASSERT(m_num_allocated_bytes >= 0);
        ai = AllocInfo{};
    }

    //----------------------------------------------------------------------------------------------
    // Data members

    // How many elements to reserve for m_alloc_info on construction. Fairly arbitrary choice, just
    // reducing the number of times the vector grows in the average case.
    static constexpr size_t k_initial_alloc_info_vector_size = 50;

    // Data storage buffer.
    std::unique_ptr<unsigned char[]> m_data;
    size_t                           m_data_size{};

    // Meta-data: the extra step of indirection to support moving element data.
    std::vector<AllocInfo> m_alloc_info;

    // Offset into m_data where the next allocation should start.
    size_t m_data_head{};

    // Number of bytes that have been allocated.
    ptrdiff_t m_num_allocated_bytes{};
};

//--------------------------------------------------------------------------------------------------

namespace detail {

// Base of handle types: implements shared functionality of CHHandle and CHPtr.
template<typename T> class CHHandleBase {
public:
    using element_type = std::remove_cv_t<std::remove_extent_t<T>>;

    using pointer         = element_type*;
    using reference       = element_type&;
    using const_pointer   = const element_type*;
    using const_reference = const element_type&;

    static constexpr bool is_array = std::is_array_v<T>;

    CHHandleBase() = default;

    void swap(CHHandleBase& rhs)
    {
        std::swap(m_owning_heap, rhs.m_owning_heap);
        std::swap(m_alloc_index, rhs.m_alloc_index);
    }

    size_t size() const noexcept { return is_null() ? 0 : _alloc_info().num_elems; }

    bool is_null() const noexcept { return m_owning_heap == nullptr; }

    // Raw pointer access --------------------------------------------------------------------------

    pointer get() const noexcept
    {
        return is_null() ? nullptr : static_cast<pointer>(_alloc_info().start);
    }

    /** Alias for get(), mainly for compatibility with gsl::span constructor. */
    pointer data() const noexcept { return get(); }

    // Index array CHHandle ------------------------------------------------------------------------

    reference operator[](size_t index) const noexcept
    {
        static_assert(is_array, "Cannot []-index non-array CHHandle");
        MG_ASSERT(!is_null());
        MG_ASSERT(index < size());
        return get()[index];
    }

    // Dereferencing non-array CHHandle ------------------------------------------------------------
    // clang-format off
    pointer   operator->() const noexcept { static_assert(!is_array); return  get(); }
    reference operator*()  const noexcept { static_assert(!is_array); return *get(); }

    // Iterate over element(s) ---------------------------------------------------------------------

    pointer       begin()        noexcept { return get(); }
    const_pointer cbegin() const noexcept { return get(); }
    const_pointer begin()  const noexcept { return cbegin(); }

    pointer       end()          noexcept { return get() + size(); }
    const_pointer cend()   const noexcept { return get() + size(); }
    const_pointer end()    const noexcept { return cend(); }
    // clang-format on

protected:
    explicit CHHandleBase(CompactingHeap* owning_heap, size_t alloc_index) noexcept
        : m_owning_heap{ owning_heap }, m_alloc_index{ alloc_index }
    {}

    // Get allocation-info struct for the pointee in CompactingHeap.
    CompactingHeap::AllocInfo& _alloc_info() const noexcept
    {
        MG_ASSERT(m_owning_heap != nullptr);
        return m_owning_heap->_alloc_info_at(m_alloc_index);
    }

    // Reset to default-constructed state.
    void clear() noexcept
    {
        m_owning_heap = nullptr;
        m_alloc_index = 0;
    }

    // Helper to shorten implementation of operator== for subtypes.
    template<typename U> bool equals(const CHHandleBase<U>& rhs) const noexcept
    {
        return m_owning_heap == rhs.m_owning_heap && m_alloc_index == rhs.m_alloc_index;
    }

    CompactingHeap* m_owning_heap{};
    size_t m_alloc_index = 0; // Index of allocation info in CompactingHeap internal data structure
};

// Identifies well-formed conversions between CHHandle/CHPtr types: true when pointer types are
// compatible, and the slicing problem is not invoked.
template<typename T, typename U>
static constexpr bool ch_handle_conversion_is_valid =
    std::is_convertible_v<std::remove_extent_t<U>*, std::remove_extent_t<T>*> &&
    ((!std::is_array_v<U> && !std::is_array_v<T>) ||
     std::is_same_v<std::remove_cv_t<U>, std::remove_cv_t<T>>);

} // namespace detail

/** Handle to element or array stored in a CompactingHeap, with unique-ownership semantics (like
 * std::unique_ptr).
 * Elements stored in a CompactingHeap may move around, and this handle deals with that. As such, do
 * not store a pointer or reference to the pointed-to element(s), always use CHHandle or CHPtr.
 */
template<typename T> class CHHandle : public detail::CHHandleBase<T> {
    using Base = detail::CHHandleBase<T>;

public:
    CHHandle() = default;

    CHHandle(std::nullptr_t) {}

    // Not copyable (unique-ownership semantics)
    CHHandle(const CHHandle&) = delete;

    CHHandle(CHHandle&& rhs) noexcept : Base(rhs.m_owning_heap, rhs.m_alloc_index) { rhs.clear(); }

    // Allow pointer conversions (add CV; derived-to-base for non-array CHHandle)
    template<typename U, typename = std::enable_if_t<detail::ch_handle_conversion_is_valid<T, U>>>
    CHHandle(CHHandle<U>&& rhs) : Base(rhs.m_owning_heap, rhs.m_alloc_index)
    {
        rhs.clear();
    }

    ~CHHandle()
    {
        if (this->m_owning_heap != nullptr) { this->m_owning_heap->_dealloc(this->m_alloc_index); }
    }

    CHHandle& operator=(CHHandle rhs) noexcept
    {
        this->swap(rhs);
        return *this;
    }

    void reset()
    {
        CHHandle tmp{};
        this->swap(tmp);
    }

    // Comparison operators ------------------------------------------------------------------------
    friend bool operator==(const CHHandle<T>& l, const CHHandle<T>& r) { return l.equals(r); }
    friend bool operator!=(const CHHandle<T>& l, const CHHandle<T>& r) { return !(l == r); }

private:
    template<typename> friend class CHHandle;
    template<typename> friend class CHPtr;
    friend class CompactingHeap;

    // Constructor, only CompactingHeap may construct non-null CHHandle.
    explicit CHHandle(CompactingHeap* owning_heap, size_t alloc_index) noexcept
        : Base{ owning_heap, alloc_index }
    {}
};

template<typename T> void swap(CHHandle<T>& lhs, CHHandle<T>& rhs)
{
    lhs.swap(rhs);
}

/** Non-owning handle to element or array stored in a CompactingHeap.
 * Elements stored in a CompactingHeap may move around, and this handle deals with that. As such, do
 * not store a pointer or reference to the pointed-to element(s), always use CHHandle or CHPtr.
 */
template<typename T> class CHPtr : public detail::CHHandleBase<T> {
    using Base = detail::CHHandleBase<T>;

public:
    CHPtr() = default;

    CHPtr(std::nullptr_t) {}

    CHPtr(const CHPtr&) = default;

    // Allow pointer conversions (add CV; derived-to-base for non-array CHPtr)
    template<typename U, typename = std::enable_if_t<detail::ch_handle_conversion_is_valid<T, U>>>
    CHPtr(const CHPtr<U>& rhs) : Base(rhs.m_owning_heap, rhs.m_alloc_index)
    {}

    // Allow pointer-converting conversion from CHHandle (e.g. CHHandle<T> -> CHPtr<const TBase>
    // where TBase is a base class of T)
    template<typename U, typename = std::enable_if_t<detail::ch_handle_conversion_is_valid<T, U>>>
    CHPtr(const CHHandle<U>& rhs) : Base(rhs.m_owning_heap, rhs.m_alloc_index)
    {}

    CHPtr& operator=(CHPtr rhs) noexcept
    {
        this->swap(rhs);
        return *this;
    }

    // Comparison operators ------------------------------------------------------------------------
    friend bool operator==(const CHPtr<T>& l, const CHPtr<T>& r) { return l.equals(r); }
    friend bool operator!=(const CHPtr<T>& l, const CHPtr<T>& r) { return !(l == r); }

private:
    template<typename> friend class CHPtr;
};

template<typename T> void swap(CHPtr<T>& lhs, CHPtr<T>& rhs)
{
    lhs.swap(rhs);
}

} // namespace Mg::memory
