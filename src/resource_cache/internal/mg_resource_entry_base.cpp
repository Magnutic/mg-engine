//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/resource_cache/internal/mg_resource_entry_base.h"

#include "mg/containers/mg_array.h"
#include "mg/core/mg_log.h"
#include "mg/resource_cache/mg_base_resource.h"
#include "mg/resource_cache/mg_resource_exceptions.h"
#include "mg/resource_cache/mg_resource_loading_input.h"

#include <fmt/core.h>

namespace Mg {

void ResourceEntryBase::load_resource()
{
    MG_ASSERT(!is_loaded());
    MG_ASSERT(ref_count == 0);
    MG_ASSERT(m_p_owning_cache != nullptr);
    MG_ASSERT(m_p_loader != nullptr);

    last_access = std::time(nullptr);

    // Load raw data.
    const auto file_size = loader().file_size(resource_id());
    auto file_data = Array<std::byte>::make(file_size);
    loader().load_file(resource_id(), file_data);

    // Structure providing interface needed for resources to load data.
    ResourceLoadingInput input{ std::move(file_data), owning_cache(), *this };

    // Init contained resource.
    BaseResource& resource = create_resource();
    m_resource_type_id = resource.type_id();
    m_has_been_loaded = true;
    m_time_stamp = loader().file_time_stamp(resource_id());
    LoadResourceResult result = resource.load_resource(input);

    switch (result.result_code) {
    case LoadResourceResult::DataError:
        log.error(fmt::format("Loading resource '{}': DataError: {}",
                                      resource_id().str_view(),
                                      result.error_reason));
        throw ResourceDataError{};

    case LoadResourceResult::Success:
        break;
    }
}

} // namespace Mg
