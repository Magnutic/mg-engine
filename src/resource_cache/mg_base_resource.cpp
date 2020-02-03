//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/resource_cache/mg_base_resource.h"

#include "mg/core/mg_log.h"
#include "mg/resource_cache/mg_resource_exceptions.h"

#include <fmt/core.h>

namespace Mg {

LoadResourceResult BaseResource::load_resource(const ResourceLoadingInput& params)
{
    g_log.write_verbose(fmt::format("Loading resource '{}'...", resource_id().str_view()));

    try {
        return load_resource_impl(params);
    }
    catch (const ResourceNotFound&) {
        return LoadResourceResult::data_error("Dependency not found.");
    }
}

} // namespace Mg
