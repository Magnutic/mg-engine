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

/** @file mg_pooling_vector.h
 * Dynamic-array data structure which grows without moving elements and thus supports move-only type
 * and never invalidates references/pointers to elements.
 */

#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "mg/containers/mg_small_vector.h"
#include "mg/utils/mg_assert.h"

namespace Mg {

/** Dynamic-array data structure which grows by allocating a fixed-size 'pool' whenever it is out of
 * space. Hence, elements never get moved, and pointers/references to elements remain valid until
 * pointees are destroyed. This also makes the data structure suitable for non-movable types.
 *
 * N.B.: unlike std::vector, it does not store elements contiguously.
 */
template<typename T> class PoolingVector {
public:
    explicit PoolingVector(size_t pool_size) : m_pool_size{ pool_size } { _grow(); }

    struct ConstructReturn {
        uint32_t index;
        T*       ptr;
    };

    /** Construct object using supplied arguments.
     * Time complexity: amortised constant time (may allocate if full).
     * @return struct of `uint32_t index; T* ptr;`
     */
    template<typename... Args> ConstructReturn construct(Args&&... args)
    {
        if (m_free_indices.empty()) { _grow(); }

        uint32_t index = m_free_indices.back();
        m_free_indices.pop_back();

        const ElemIndex ei{ _internal_index(index) };
        T* ptr = &m_pools[ei.pool_index][ei.element_index].emplace(std::forward<Args>(args)...);

        return { index, ptr };
    }

    /** Destroy contained object at the given index. Does not invalidate indices or pointers to
     * other members.
     * Time complexity: constant.
     */
    void destroy(uint32_t index) noexcept
    {
        const ElemIndex ei{ _internal_index(index) };
        m_free_indices.push_back(index);
        m_pools[ei.pool_index][ei.element_index] = std::nullopt;
    }

    T& operator[](uint32_t index) noexcept
    {
        const ElemIndex ei{ _internal_index(index) };
        MG_ASSERT_DEBUG(m_pools[ei.pool_index][ei.element_index].has_value());
        return *m_pools[ei.pool_index][ei.element_index];
    }

    const T& operator[](uint32_t index) const noexcept
    {
        const ElemIndex ei{ _internal_index(index) };
        MG_ASSERT_DEBUG(m_pools[ei.pool_index][ei.element_index].has_value());
        return *m_pools[ei.pool_index][ei.element_index];
    }

private:
    struct ElemIndex {
        size_t pool_index{};
        size_t element_index{};
    };

    ElemIndex _internal_index(size_t index) const noexcept
    {
        const auto pool_index    = index / m_pool_size;
        const auto element_index = index % m_pool_size;

        MG_ASSERT_DEBUG(pool_index < m_pools.size());
        MG_ASSERT_DEBUG(element_index < m_pool_size);

        return { pool_index, element_index };
    }

    void _grow()
    {
        MG_ASSERT(m_pool_size > 0);

        m_pools.emplace_back(std::make_unique<Pool>(m_pool_size));

        const size_t new_index_start = (m_pools.size() - 1) * m_pool_size;
        const size_t new_index_end   = new_index_start + m_pool_size;
        const size_t num_new_indices = new_index_end - new_index_start;

        for (size_t i = 0; i < num_new_indices; ++i) {
            m_free_indices.push_back(uint32_t(new_index_end - 1 - i));
        }
    }

    using StorageT = std::optional<T>;
    using Pool     = StorageT[];

    size_t m_pool_size{};

    small_vector<std::unique_ptr<Pool>, 10> m_pools;
    std::vector<uint32_t>                   m_free_indices;
};

} // namespace Mg
