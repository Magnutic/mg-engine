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

/** @file mg_pooling_vector.h
 * Dynamic-array data structure which grows without moving elements and thus supports move-only type
 * and never invalidates references/pointers to elements.
 */

#pragma once

#include "mg/containers/mg_array.h"
#include "mg/containers/mg_small_vector.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_macros.h"

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace Mg {

namespace detail {

// A pool for storage of objects in PoolingVector.
// PoolingVector allocates a new one of these whenever the existing ones are full.
template<typename T> class StoragePool {
public:
    MG_MAKE_DEFAULT_COPYABLE(StoragePool);
    MG_MAKE_DEFAULT_MOVABLE(StoragePool);

    StoragePool() = default;

    StoragePool(size_t size)
        : m_storage(Array<StorageT>::make(size)), m_present(Array<bool>::make(size))
    {}

    ~StoragePool()
    {
        for (size_t i = 0; i < m_storage.size(); ++i) {
            if (m_present[i]) { destroy(i); }
        }
    }

    template<typename... Args> T& emplace(size_t i, Args&&... args)
    {
        if (m_present[i]) { destroy(i); }

        T* ptr       = new (&m_storage[i]) T(std::forward<Args>(args)...);
        m_present[i] = true;

        return *ptr;
    }

    T& get(size_t i) noexcept
    {
        MG_ASSERT_DEBUG(m_present[i]);
        return reinterpret_cast<T&>(m_storage[i]); // NOLINT
    }

    const T& get(size_t i) const noexcept
    {
        MG_ASSERT_DEBUG(m_present[i]);
        return reinterpret_cast<const T&>(m_storage[i]); // NOLINT
    }

    void destroy(size_t i)
    {
        get(i).~T();
        m_present[i] = false;
    }

    bool is_present(size_t i) const { return m_present[i]; }

private:
    using StorageT = std::aligned_storage_t<sizeof(T), alignof(T)>;
    Array<StorageT> m_storage;
    Array<bool>     m_present; // TODO: this should ideally be a bit-field instead of array of bool.
};

template<typename OwnerT> class PoolingVectorIt;

} // namespace detail

/** Dynamic-array data structure which grows by allocating a fixed-size 'pool' whenever it is out of
 * space. Hence, elements never get moved, and pointers/references to elements remain valid until
 * pointees are destroyed. This also makes the data structure suitable for non-movable types.
 *
 * N.B.: unlike std::vector, it does not store elements contiguously.
 */
template<typename T> class PoolingVector {
public:
    using value_type      = T;
    using reference       = T&;
    using pointer         = T*;
    using const_reference = const T&;
    using const_pointer   = const T*;
    using size_type       = uint32_t;
    using iterator        = detail::PoolingVectorIt<PoolingVector>;
    using const_iterator  = detail::PoolingVectorIt<const PoolingVector>;

    /** Construct.
     * @param pool_size Size of each individual element-pool (in number of elements). The
     * PoolingVector will allocate storage for elements in pools of this size.
     * Must be greater than zero.
     */
    explicit PoolingVector(size_t pool_size) : m_pool_size{ pool_size }
    {
        MG_ASSERT(pool_size > 0);
        _grow();
    }

    struct ConstructReturn {
        size_type index;
        T*        ptr;
    };

    /** Construct object using supplied arguments.
     * Time complexity: constant time (but may allocate a new pool if full).
     * @return struct of `size_type index; T* ptr;`
     */
    template<typename... Args> ConstructReturn construct(Args&&... args)
    {
        if (m_free_indices.empty()) { _grow(); }

        const size_type index = m_free_indices.back();
        m_free_indices.pop_back();

        const ElemIndex ei{ _internal_index_unchecked(index) };
        T* ptr = &m_pools[ei.pool_index].emplace(ei.element_index, std::forward<Args>(args)...);
        ++m_size;

        return { index, ptr };
    }

    /** Destroy contained object at the given index. Does not invalidate indices or pointers to
     * other members.
     * Time complexity: constant.
     */
    void destroy(size_type index)
    {
        const ElemIndex ei = _internal_index(index);
        m_free_indices.push_back(index);
        m_pools[ei.pool_index].destroy(ei.element_index);
        --m_size;
    }

    /** Get element at index. Precondition: an element exists at index. */
    T& operator[](size_type index) noexcept
    {
        const ElemIndex ei = _internal_index(index);
        return m_pools[ei.pool_index].get(ei.element_index);
    }

    /** Get element at index. Precondition: an element exists at index. */
    const T& operator[](size_type index) const noexcept
    {
        const ElemIndex ei = _internal_index(index);
        return m_pools[ei.pool_index].get(ei.element_index);
    }

    /** Get whether there exists an element at the given index. */
    bool index_valid(size_type index) const noexcept
    {
        return _internal_index_valid(_internal_index_unchecked(index));
    }

    iterator       begin() noexcept { return iterator(*this, 0); }
    const_iterator begin() const noexcept { return const_iterator(*this, 0); }
    const_iterator cbegin() const noexcept { return const_iterator(*this, 0); }

    iterator       end() noexcept { return iterator(*this, _guaranteed_end_index()); }
    const_iterator end() const noexcept { return const_iterator(*this, _guaranteed_end_index()); }
    const_iterator cend() const noexcept { return const_iterator(*this, _guaranteed_end_index()); }

    size_t pool_size() const noexcept { return m_pool_size; }
    size_t num_pools() const noexcept { return m_pools.size(); }
    size_t size() const noexcept { return m_size; }
    bool   empty() const noexcept { return size() == 0; }

    void clear() noexcept
    {
        m_pools.clear();
        _grow();
    }

private:
    friend class detail::PoolingVectorIt<PoolingVector>;

    struct ElemIndex {
        size_t pool_index{};
        size_t element_index{};
    };

    // Index of and within the respective pool.
    ElemIndex _internal_index_unchecked(size_t index) const noexcept
    {
        const auto pool_index    = index / m_pool_size;
        const auto element_index = index % m_pool_size;
        return { pool_index, element_index };
    }

    ElemIndex _internal_index(size_t index) const noexcept
    {
        const ElemIndex ei = _internal_index_unchecked(index);
        MG_ASSERT(_internal_index_valid(ei));
        return ei;
    }

    bool _internal_index_valid(ElemIndex ei) const noexcept
    {
        return ei.pool_index < m_pools.size() && ei.element_index < m_pool_size &&
               m_pools[ei.pool_index].is_present(ei.element_index);
    }

    // Return an index that is guaranteed to be past the end.
    size_type _guaranteed_end_index() const noexcept
    {
        return narrow<size_type>(num_pools() * pool_size());
    }

    void _grow()
    {
        MG_ASSERT(m_pool_size > 0);

        m_pools.emplace_back(m_pool_size);

        const size_t new_index_start = (m_pools.size() - 1) * m_pool_size;
        const size_t new_index_end   = new_index_start + m_pool_size;
        const size_t num_new_indices = new_index_end - new_index_start;

        for (size_t i = 0; i < num_new_indices; ++i) {
            m_free_indices.push_back(size_type(new_index_end - 1 - i));
        }
    }

    small_vector<detail::StoragePool<T>, 1> m_pools;
    small_vector<size_type, 1>              m_free_indices;

    size_t m_pool_size{};
    size_t m_size{};
};

namespace detail {
template<typename OwnerT> class PoolingVectorIt {
public:
    using value_type        = std::remove_reference_t<decltype(std::declval<OwnerT>()[0])>;
    using pointer           = value_type*;
    using reference         = value_type&;
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;

    PoolingVectorIt() = default;

    explicit PoolingVectorIt(OwnerT& owner, uint32_t index) : m_owner(&owner), m_index(index)
    {
        if (!m_owner->index_valid(m_index) && !is_past_end()) { ++(*this); }
    }

    operator PoolingVectorIt<const OwnerT>() const noexcept
    {
        return PoolingVectorIt<const OwnerT>(*m_owner, m_index);
    }

    value_type& operator*() const noexcept { return (*m_owner)[m_index]; }
    value_type* operator->() const noexcept { return std::addressof((*m_owner)[m_index]); }

    PoolingVectorIt operator++() noexcept
    {
        do {
            ++m_index;
        } while (!m_owner->index_valid(m_index) && !is_past_end());

        return *this;
    }

    PoolingVectorIt operator++(int) noexcept
    {
        PoolingVectorIt tmp{ *this };
        ++(*this);
        return tmp;
    }

    friend bool operator==(const PoolingVectorIt lhs, const PoolingVectorIt rhs) noexcept
    {
        return lhs.m_owner == rhs.m_owner && lhs.m_index == rhs.m_index;
    }

    friend bool operator!=(const PoolingVectorIt lhs, const PoolingVectorIt rhs) noexcept
    {
        return !(lhs == rhs);
    }

private:
    bool is_past_end() const noexcept
    {
        const auto pool_index = m_index / m_owner->pool_size();
        MG_ASSERT_DEBUG(pool_index <= m_owner->num_pools());
        return pool_index == m_owner->num_pools();
    };

    OwnerT*  m_owner = nullptr;
    uint32_t m_index = 0;
};
} // namespace detail

} // namespace Mg
