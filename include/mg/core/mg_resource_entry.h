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

/** @file mg_resource_entry.h
 * Internal storage format for resource types in ResourceCache.
 * @see Mg::ResourceCache
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_macros.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <vector>

namespace Mg {

class ResourceCache;
class BaseResource;

using time_point = std::chrono::system_clock::time_point;

/** ResourceEntry is the internal storage-node type for resources in a ResourceCache.
 * ResourceEntryBase implements the resource-type-independent functionality as an abstract base
 * class, allowing the stored resource to be accessed as BaseResource via get_resource.
 */
class ResourceEntryBase {
public:
    ResourceEntryBase(Identifier resource_id_, time_point time_stamp_, ResourceCache& owner)
        : resource_id(resource_id_), time_stamp(time_stamp_), p_owning_cache(&owner)
    {}

    MG_MAKE_NON_COPYABLE(ResourceEntryBase);
    MG_MAKE_NON_MOVABLE(ResourceEntryBase);

    virtual ~ResourceEntryBase() {}

    virtual BaseResource&       get_resource()       = 0;
    virtual const BaseResource& get_resource() const = 0;

    /** Make a new (empty) ResourceEntry of the same derived type as this one. */
    virtual std::unique_ptr<ResourceEntryBase> new_entry(Identifier resource_id,
                                                         time_point time_stamp_) = 0;

    /** Swap values. Requires that this and other are of the same derived type. */
    virtual void swap_entry(ResourceEntryBase& other) noexcept = 0;

    /** Return the stored resource object, or, if it does not exist, create a new empty one. */
    virtual BaseResource& get_or_create_resource() = 0;

    /** Whether resource is loaded. */
    virtual bool is_loaded() = 0;

    /** Unload stored resource. */
    virtual void unload() = 0;

    /** Whether resource is unloadable: is loaded and reference count is zero. */
    bool is_unloadable() { return is_loaded() && ref_count == 0; }

    struct Dependency {
        Identifier dependency_id;
        time_point time_stamp;
    };

    Identifier resource_id;

    /** A list of resource files upon which this resource depends. This is used to trigger
     * re-loading of this resource if those files are changed. Dependencies are automatically
     * tracked when a dependency is loaded in a resource type's `load_resource()` function via
     * `ResourceDataLoader::load_dependency()`.
     */
    std::vector<Dependency> dependencies;

    time_point time_stamp{};
    time_point last_access{};

    std::atomic_int32_t ref_count      = 0;
    ResourceCache*      p_owning_cache = nullptr;

protected:
    void load_resource();
};

/** Reference-counting access to a resource within a ResourceCache.
 * Do not store this anywhere -- instead, store the resource's ResourceHandle, and then get a
 * ResourceAccessGuard from the ResourceHandle only when access is needed (i.e. within function
 * bodies, on the stack).
 *
 * Usage example:
 *
 *     void some_function_that_uses_a_resource(ResourceHandle resource_handle) {
 *         ResourceCache& res_cache = ...
 *         ResourceAccessGuard<ResType> res_access = res_cache.access_resource(resource_handle);
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
    ResourceAccessGuard(ResourceEntryBase& resource_entry) : m_entry(&resource_entry)
    {
        m_entry->ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    ~ResourceAccessGuard() { m_entry->ref_count.fetch_sub(1, std::memory_order_acq_rel); }

    // Not copyable or movable -- ResourceAccessGuard should only be placed on the stack, not be
    // stored or passed around.
    MG_MAKE_NON_COPYABLE(ResourceAccessGuard);
    MG_MAKE_NON_MOVABLE(ResourceAccessGuard);

    time_point file_time_stamp() const noexcept { return m_entry->time_stamp; }

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
    ResourceEntryBase* m_entry = nullptr;
};

/** ResourceEntry is the internal storage-node type for resources stored within a ResourceCache. */
template<typename ResT> class ResourceEntry final : public ResourceEntryBase {
public:
    using ResourceEntryBase::ResourceEntryBase;

    ResourceAccessGuard<ResT> access_resource()
    {
        if (!is_loaded()) { load_resource(); }
        last_access = std::chrono::system_clock::now();
        return ResourceAccessGuard<ResT>(*this);
    }

    // Allow base class to access resource member.
    // ResT is assumed to be derived from BaseResource, as all resource types have to be.
    ResT&       get_resource() override { return m_resource.value(); }
    const ResT& get_resource() const override { return m_resource.value(); }

    std::unique_ptr<ResourceEntryBase> new_entry(Identifier resource_id,
                                                 time_point time_stamp_) override
    {
        return std::make_unique<ResourceEntry>(resource_id, time_stamp_, *p_owning_cache);
    }

    void swap_entry(ResourceEntryBase& other) noexcept override
    {
        using std::swap;
        auto& rhs = static_cast<ResourceEntry&>(other);

        MG_ASSERT(ref_count == 0 && rhs.ref_count == 0 && "Trying to swap an in-use resource.");
        MG_ASSERT(p_owning_cache == rhs.p_owning_cache);

        swap(dependencies, rhs.dependencies);
        swap(time_stamp, rhs.time_stamp);
        swap(last_access, rhs.last_access);
        swap(m_resource, rhs.m_resource);
    }

    ResT& get_or_create_resource() override
    {
        if (!is_loaded()) { m_resource.emplace(resource_id); }
        return m_resource.value();
    }

    bool is_loaded() override { return m_resource.has_value(); }

    void unload() override
    {
        MG_ASSERT(is_unloadable());
        m_resource.reset();
    }

private:
    std::optional<ResT> m_resource;
};

} // namespace Mg
