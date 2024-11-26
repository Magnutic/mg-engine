//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/resources/mg_texture_array_resource.h"

#include "mg/core/resource_cache/mg_resource_loading_input.h"

#include <hjson/hjson.h>

namespace Mg {

LoadResourceResult TextureArrayResource::load_resource_impl(ResourceLoadingInput& input)
{
    const auto text = input.resource_data_as_text();
    const auto value = Hjson::Unmarshal(text.data(), text.size());
    if (value.type() != Hjson::Type::Vector) {
        return LoadResourceResult::data_error("Expected list of texture resource ids");
    }

    m_textures.clear();

    // NOLINTNEXTLINE(modernize-loop-convert) // Hjson::Value::begin/end are only valid for maps
    for (int i = 0; as<size_t>(i) < value.size(); ++i) {
        m_textures.push_back(input.load_dependency<TextureResource>(
            Identifier::from_runtime_string(value[i].to_string())));
    }

    return LoadResourceResult::success();
}

} // namespace Mg
