//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_resource_entry.h
 * Internal storage format for resource types in ResourceCache.
 * @see Mg::ResourceHandle
 * @see Mg::ResourceCache
 * @see Mg::ResourceAccessGuard
 * @see Mg::ResourceEntryBase
 */

#pragma once

#include "mg/resource_cache/internal/mg_resource_entry_base.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_optional.h"

namespace Mg {

/** ResourceEntry is the internal storage-node type for resources stored within a ResourceCache. */
template<typename ResT> class ResourceEntry final : public ResourceEntryBase {
    static_assert(std::is_base_of_v<BaseResource, ResT>,
                  "Type must be derived from Mg::BaseResource.");
    static_assert(!std::is_abstract_v<ResT>, "Resource types must not be abstract.");
    static_assert(std::is_constructible_v<ResT, Identifier>);

public:
    using ResourceEntryBase::ResourceEntryBase;

    // Allow base class to access resource member.
    // ResT is assumed to be derived from BaseResource, as all resource types have to be.
    ResT& get_resource() override { return m_resource.value(); }
    const ResT& get_resource() const override { return m_resource.value(); }

    void unload() override
    {
        MG_ASSERT(ref_count == 0);
        dependencies.clear();
        m_resource.reset();
    }

    bool is_loaded() const override { return m_resource.has_value(); }

protected:
    ResT& create_resource() override { return m_resource.emplace(resource_id()); }

private:
    Opt<ResT> m_resource;
};

} // namespace Mg
