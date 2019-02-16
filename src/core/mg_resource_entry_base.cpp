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

#include "mg/core/mg_resource_entry_base.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_resource_exceptions.h"
#include "mg/core/mg_resource_loading_input.h"

#include <fmt/core.h>

namespace Mg {

void ResourceEntryBase::load_resource()
{
    MG_ASSERT(!is_loaded());
    MG_ASSERT(ref_count == 0);
    MG_ASSERT(m_p_owning_cache != nullptr);
    MG_ASSERT(m_p_loader != nullptr);

    last_access = std::chrono::system_clock::now();

    std::vector<std::byte> file_data;
    file_data.resize(loader().file_size(resource_id()));

    loader().load_file(resource_id(), file_data);
    ResourceLoadingInput input{ file_data, owning_cache(), *this };

    LoadResourceResult result = create_resource().load_resource(input);

    switch (result.result_code) {
    case LoadResourceResult::DataError:
        g_log.write_error(fmt::format(
            "Loading resource '{}': DataError: {}", resource_id().str_view(), result.error_reason));
        throw ResourceDataError{};
    case LoadResourceResult::Success: break;
    }
}

} // namespace Mg
