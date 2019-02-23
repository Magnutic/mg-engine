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

/** @file mg_resource_loading_input.h
 * Helper class used as input to Resource types' `load_resource()` function.
 * @see BaseResource
 * @see ResourceCache
 */

#pragma once

#include "mg/core/mg_resource_cache.h"

namespace Mg {

/** Input to resource types' `load_resource()` member function. */
class ResourceLoadingInput {
public:
    explicit ResourceLoadingInput(Array<std::byte>   data,
                                  ResourceCache&     owning_cache,
                                  ResourceEntryBase& resource_entry)
        : m_data(std::move(data)), m_owning_cache(&owning_cache), m_resource_entry(&resource_entry)
    {}

    span<const std::byte> resource_data() const noexcept { return m_data; }

    std::string_view resource_data_as_text() const noexcept
    {
        return { reinterpret_cast<const char*>(m_data.data()), m_data.size() }; // NOLINT
    }

    /** Load a resource and mark this resource as dependent on the newly loaded resource. */
    template<typename ResT>
    ResourceHandle<ResT> load_dependency(Identifier dependency_file_id) const
    {
        time_point file_time_stamp = m_owning_cache->file_time_stamp(dependency_file_id);
        m_resource_entry->dependencies.push_back({ dependency_file_id, file_time_stamp });
        return m_owning_cache->resource_handle<ResT>(dependency_file_id);
    }


private:
    Array<std::byte>   m_data;
    ResourceCache*     m_owning_cache;
    ResourceEntryBase* m_resource_entry;
};


}; // namespace Mg
