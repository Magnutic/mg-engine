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

#include <mg/memory/mg_compacting_heap.h>

#include <algorithm>
#include <cstring>

namespace Mg::memory {

void CompactingHeap::compact() noexcept
{
    std::vector<AllocInfo*> alloc_info;
    alloc_info.reserve(m_alloc_info.size());

    for (auto&& ai : m_alloc_info) { alloc_info.push_back(&ai); }

    // Sort copy by allocation address.
    std::sort(alloc_info.begin(), alloc_info.end(), [](AllocInfo* lhs, AllocInfo* rhs) {
        return lhs->start < rhs->start;
    });

    // Compact: move data toward lower address to cover any gaps.
    // Note that alignment is guaranteed, as CompactingHeap::alloc() makes sure all allocation sizes
    // end on aligned addresses.
    size_t new_data_head = 0;
    for (AllocInfo* ai : alloc_info) {
        if (ai->start == nullptr) { continue; }

        auto new_start = &m_data[new_data_head];
        ai->mover->move(new_start, ai->start, ai->num_elems);
        ai->start = new_start;

        new_data_head += ai->raw_size;
    }

    MG_ASSERT(new_data_head <= m_data_head);
    m_data_head = new_data_head;
}

size_t CompactingHeap::_alloc_impl(size_t elem_size, size_t num)
{
    // Perhaps allocations of size 0 should be prevented with an assert (as it likely represents a
    // bug somewhere in user code), but for consistency with new[] and std::make_unique, we allow
    // 0-size allocations.
    size_t alloc_size = _calculate_alloc_size(elem_size, num);

    // If allocation cannot fit at end of heap, throw.
    if (alloc_size + m_data_head > m_data_size) { throw std::bad_alloc{}; }

    // Find unused AllocInfo.
    // TODO: Optimisation: intrusive linked-list of free AllocInfos instead of linear search.
    AllocInfo* target = nullptr;
    size_t     target_index{};

    for (; target_index < m_alloc_info.size(); ++target_index) {
        AllocInfo& ai = _alloc_info_at(target_index);

        if (ai.start == nullptr) {
            target = &ai;
            break;
        }
    }

    // If there is no free AllocInfo, then push back a new one.
    if (target == nullptr) {
        target       = &m_alloc_info.emplace_back();
        target_index = m_alloc_info.size() - 1; // Redundant, but keep just to be sure.
    }

    target->raw_size  = alloc_size;
    target->num_elems = num;
    target->start     = &m_data[m_data_head];

    m_data_head += alloc_size;
    m_num_allocated_bytes += static_cast<ptrdiff_t>(alloc_size);

    return target_index;
}

} // namespace Mg::memory
