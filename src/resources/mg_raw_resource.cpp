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

#include "mg/resources/mg_raw_resource.h"

#include "mg/core/mg_resource_cache.h"

namespace Mg {

LoadResourceResult RawResource::load_resource_impl(const LoadResourceParams& load_params)
{
    span<const std::byte> data = load_params.resource_data();
    m_buffer                   = Array<std::byte>::make_copy(data);
    return LoadResourceResult::success();
}

} // namespace Mg
