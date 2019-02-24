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

/** @file mg_resource_access_guard.h
 * Scope guard for safely accessing a Resource's data.
 * @see Mg::ResourceHandle
 * @see Mg::ResourceCache
 * @see Mg::ResourceEntryBase
 */

#pragma once

#include "mg/core/mg_resource_entry.h"
#include "mg/core/mg_resource_handle.h"
#include "mg/utils/mg_macros.h"

namespace Mg {

/** Reference-counting access to a resource within a ResourceCache.
 * Do not store this anywhere -- instead, store the resource's ResourceHandle, and then get a
 * ResourceAccessGuard from the ResourceHandle only when access is needed (i.e. within function
 * bodies, on the stack).
 *
 * Usage example:
 *
 *     void some_function_that_uses_a_resource(ResourceHandle resource_handle) {
 *         ResourceAccessGuard<ResType> res_access{ resource_handle };
 *         auto something = res_access->something_in_the_resource;
 *         // etc. Resource can be safely accessed as long as `res_access` remains in scope.
 *     }
 *
 * As long as at least one ResourceAccessGuard to a given resource exist, then that resource will
 * not be unloaded from the ResourceCache.
 * @see Mg::ResourceCache
 */
template<typename ResT> class ResourceAccessGuard {
public:
    ResourceAccessGuard(ResourceHandle<ResT> handle)
        : m_entry(handle.m_p_entry), m_lock(handle.m_p_entry->mutex)
    {
        if (!m_entry->is_loaded()) { m_entry->load_resource(); }
        m_entry->last_access = std::chrono::system_clock::now();
        ++m_entry->ref_count;
    }

    ~ResourceAccessGuard() { --m_entry->ref_count; }

    // Not copyable or movable -- ResourceAccessGuard should only be placed on the stack, not be
    // stored or passed around.
    MG_MAKE_NON_COPYABLE(ResourceAccessGuard);
    MG_MAKE_NON_MOVABLE(ResourceAccessGuard);

    time_point file_time_stamp() const noexcept { return m_entry->time_stamp(); }

    const ResT& operator*() const& noexcept { return *get(); }
    const ResT* operator->() const& noexcept { return get(); }

    ResT& operator*() & noexcept { return *get(); }
    ResT* operator->() & noexcept { return get(); }

    ResT* get() & noexcept
    {
        BaseResource* base_res = &m_entry->get_resource();
        return static_cast<ResT*>(base_res);
    }

    const ResT* get() const& noexcept
    {
        const BaseResource* base_res = &m_entry->get_resource();
        return static_cast<const ResT*>(base_res);
    }

    // Disallow dereferencing rvalue ResourceAccessGuard.
    // ResourceAccessGuard is intended to remain on the stack for the duration of the resource use,
    // this prevents simple mistakes violating that intention.
    const ResT& operator*() const&&  = delete;
    const ResT* operator->() const&& = delete;

    ResT& operator*() &&  = delete;
    ResT* operator->() && = delete;

    ResT*       get() &&      = delete;
    const ResT* get() const&& = delete;

private:
    ResourceEntryBase*                        m_entry = nullptr;
    std::shared_lock<std::shared_timed_mutex> m_lock;
};

} // namespace Mg
