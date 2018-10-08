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

#include <chrono>
#include <vector>

#include <mg/core/mg_identifier.h>
#include <mg/utils/mg_macros.h>

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
    ResourceEntryBase(time_point time_stamp_) : time_stamp(time_stamp_) {}

    MG_MAKE_NON_COPYABLE(ResourceEntryBase);
    MG_MAKE_DEFAULT_MOVABLE(ResourceEntryBase);

    virtual ~ResourceEntryBase() {}
    virtual BaseResource&       get_resource()       = 0;
    virtual const BaseResource& get_resource() const = 0;

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

    int32_t ref_count = 0;
};

/** ResourceEntry is the internal storage-node type for resources stored within a ResourceCache. */
template<typename ResT> class ResourceEntry : public ResourceEntryBase {
public:
    ResT resource;

    ResourceEntry(Identifier resource_id, time_point time_stamp_)
        : ResourceEntryBase{ time_stamp_ }, resource(resource_id)
    {}

    // Allow base class to access resource member.
    // ResT is assumed to be derived from BaseResource, as all resource types have to be.
    BaseResource&       get_resource() override { return resource; }
    const BaseResource& get_resource() const override { return resource; }
};

} // namespace Mg
