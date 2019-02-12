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
    ResourceEntryBase(time_point time_stamp_, ResourceCache& owner)
        : time_stamp(time_stamp_), p_owning_cache(&owner)
    {}

    MG_MAKE_NON_COPYABLE(ResourceEntryBase);
    MG_MAKE_DEFAULT_MOVABLE(ResourceEntryBase);

    virtual ~ResourceEntryBase() {}

    virtual BaseResource&       get_resource()       = 0;
    virtual const BaseResource& get_resource() const = 0;

    /** Make a new (empty) ResourceEntry of the same derived type as this one. */
    virtual std::unique_ptr<ResourceEntryBase> new_entry(Identifier resource_id,
                                                         time_point time_stamp_) = 0;

    /** Swap values. Requires that this and other are of the same derived type. */
    virtual void swap_entry(ResourceEntryBase& other) noexcept = 0;

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
};

/** ResourceEntry is the internal storage-node type for resources stored within a ResourceCache. */
template<typename ResT> class ResourceEntry : public ResourceEntryBase {
public:
    std::optional<ResT> resource;

    ResourceEntry(Identifier resource_id_, time_point time_stamp_, ResourceCache& owner)
        : ResourceEntryBase(time_stamp_, owner), resource(resource_id_)
    {}

    // Allow base class to access resource member.
    // ResT is assumed to be derived from BaseResource, as all resource types have to be.
    BaseResource&       get_resource() override { return resource.value(); }
    const BaseResource& get_resource() const override { return resource.value(); }

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
        swap(resource, rhs.resource);
    }

    bool is_loaded() override { return resource.has_value(); }

    void unload() override { resource.reset(); }
};

} // namespace Mg
