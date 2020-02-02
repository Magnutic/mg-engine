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
        g_log.write_error(fmt::format("Loading resource '{}': DataError: {}",
                                      resource_id().str_view(),
                                      result.error_reason));
        throw ResourceDataError{};

    case LoadResourceResult::Success:
        break;
    }
}

} // namespace Mg
