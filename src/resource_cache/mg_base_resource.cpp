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
