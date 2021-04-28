//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_resource_access_guard.h
 * Scope guard for safely accessing a Resource's data.
 * @see Mg::ResourceHandle
 * @see Mg::ResourceCache
 * @see Mg::ResourceEntryBase
 */

#pragma once

#include "mg/resource_cache/internal/mg_resource_entry.h"
#include "mg/resource_cache/mg_resource_handle.h"
#include "mg/utils/mg_macros.h"

namespace Mg {

/** Reference-counting access to a resource within a ResourceCache.
 * Do not store this anywhere -- instead, store the resource's ResourceHandle, and then get a
 * ResourceAccessGuard from the ResourceHandle only when access is needed (i.e. within function
 * bodies, on the stack).
 *
 * Usage example:
 *
 *     void some_function_that_uses_a_resource(ResourceHandle<ResourceType> resource_handle) {
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
    explicit ResourceAccessGuard(BaseResourceHandle handle)
        : m_entry(handle.m_p_entry), m_lock(handle.m_p_entry->mutex)
    {
        if (!m_entry->is_loaded()) {
            m_entry->load_resource();
        }
        m_entry->last_access = std::time(nullptr);
        ++m_entry->ref_count;

        MG_ASSERT(
            _deref().type_id() == m_entry->resource_type_id() &&
            "ResourceAccessGuard constructed using ResourceHandle to wrong type of resource.");
    }

    // Constructor taking specific resource handle type allows argument deduction.
    explicit ResourceAccessGuard(ResourceHandle<ResT> handle)
        : ResourceAccessGuard(BaseResourceHandle(handle))
    {}

    ~ResourceAccessGuard() { --m_entry->ref_count; }

    // Not copyable or movable -- ResourceAccessGuard should only be placed on the stack, not be
    // stored or passed around.
    MG_MAKE_NON_COPYABLE(ResourceAccessGuard);
    MG_MAKE_NON_MOVABLE(ResourceAccessGuard);

    std::time_t file_time_stamp() const noexcept { return m_entry->time_stamp(); }

    const ResT& operator*() const& noexcept { return _deref(); }
    const ResT* operator->() const& noexcept { return &_deref(); }

    ResT& operator*() & noexcept { return _deref(); }
    ResT* operator->() & noexcept { return &_deref(); }

    ResT* get() & noexcept { return &_deref(); }
    const ResT* get() const& noexcept { return &_deref(); }

    // Disallow dereferencing rvalue ResourceAccessGuard.
    // ResourceAccessGuard is intended to remain on the stack for the duration of the resource use,
    // this prevents simple mistakes violating that intention.
    const ResT& operator*() const&& = delete;
    const ResT* operator->() const&& = delete;

    ResT& operator*() && = delete;
    ResT* operator->() && = delete;

    ResT* get() && = delete;
    const ResT* get() const&& = delete;

private:
    ResT& _deref() const noexcept
    {
        ResT& resource = static_cast<ResT&>(m_entry->get_resource());
        return resource;
    }

    ResourceEntryBase* m_entry = nullptr;
    std::shared_lock<std::shared_timed_mutex> m_lock;
};

} // namespace Mg
