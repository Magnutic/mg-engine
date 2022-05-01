//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_texture_related_types.h"

#include "mg/utils/mg_string_utils.h"

namespace Mg::gfx {

Opt<TextureCategory> deduce_texture_category(std::string_view texture_filename)
{
    const auto filepath = replace_char(texture_filename, '\\', '/');
    const auto filename_with_ext = substring_after_last(filepath, '/');
    const auto filename = substring_until_last(filename_with_ext, '.');

    for (const auto& [category, suffix] : g_texture_category_to_filename_suffix_map) {
        if (is_suffix_of(suffix, filename)) {
            return category;
        }
    }

    return nullopt;
}

} // namespace Mg::gfx
