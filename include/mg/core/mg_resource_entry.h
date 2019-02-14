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
 * @see Mg::ResourceHandle
 * @see Mg::ResourceCache
 * @see Mg::ResourceAccessGuard
 * @see Mg::ResourceEntryBase
 */

#pragma once

#include "mg/core/mg_resource_access_guard.h"
#include "mg/core/mg_resource_entry_base.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_macros.h"

#include <optional>

namespace Mg {

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

    std::unique_ptr<ResourceEntryBase> new_entry(IFileLoader& loader,
                                                 time_point   time_stamp_) override
    {
        return std::make_unique<ResourceEntry>(resource_id, loader, time_stamp_, *p_owning_cache);
    }

    void swap_entry(ResourceEntryBase& other) noexcept override
    {
        using std::swap;
        auto& rhs = static_cast<ResourceEntry&>(other);

        MG_ASSERT(ref_count == 0 && rhs.ref_count == 0 && "Trying to swap an in-use resource.");

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
