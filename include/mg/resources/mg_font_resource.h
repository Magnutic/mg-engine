//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_font_resource.h
 * Resource for font data.
 */

#pragma once

#include "mg/containers/mg_array.h"
#include "mg/resource_cache/mg_base_resource.h"

namespace Mg {

class FontResource : public BaseResource {
public:
    using BaseResource::BaseResource;

    bool should_reload_on_file_change() const override { return true; }

    Identifier type_id() const override { return "FontResource"; }

    /** Access raw font data. */
    std::span<const std::byte> data() const noexcept { return m_font_data; }

protected:
    LoadResourceResult load_resource_impl(ResourceLoadingInput& input) override;

private:
    Array<std::byte> m_font_data;
};

} // namespace Mg
