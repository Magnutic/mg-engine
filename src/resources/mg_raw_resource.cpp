//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/resources/mg_raw_resource.h"

#include "mg/resource_cache/mg_resource_loading_input.h"

namespace Mg {

LoadResourceResult RawResource::load_resource_impl(const ResourceLoadingInput& input)
{
    span<const std::byte> data = input.resource_data();
    m_buffer = Array<std::byte>::make_copy(data);
    return LoadResourceResult::success();
}

} // namespace Mg
