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
    ResT&       get_resource() override { return m_resource.value(); }
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
