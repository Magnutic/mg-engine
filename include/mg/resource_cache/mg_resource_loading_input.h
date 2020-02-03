//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_resource_loading_input.h
 * Helper class used as input to Resource types' `load_resource()` function.
 * @see BaseResource
 * @see ResourceCache
 */

#pragma once

#include "mg/resource_cache/mg_resource_cache.h"

namespace Mg {

/** Input to resource types' `load_resource()` member function. */
class ResourceLoadingInput {
public:
    ResourceLoadingInput(Array<std::byte> data,
                         ResourceCache& owning_cache,
                         ResourceEntryBase& resource_entry) noexcept
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
        const auto file_time_stamp = m_owning_cache->file_time_stamp(dependency_file_id);
        const auto handle = m_owning_cache->resource_handle<ResT>(dependency_file_id);

        // Write dependency after look-up.
        // Order is important, as look-ups might throw.
        m_resource_entry->dependencies.push_back({ dependency_file_id, file_time_stamp });

        return handle;
    }

private:
    Array<std::byte> m_data;
    ResourceCache* m_owning_cache;
    ResourceEntryBase* m_resource_entry;
};


}; // namespace Mg
