//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_texture_array_resource.h
 * .
 */

#pragma once

#include "mg/core/resource_cache/mg_base_resource.h"
#include "mg/core/resource_cache/mg_resource_handle.h"
#include "mg/core/resources/mg_texture_resource.h"

#include <span>
#include <vector>

namespace Mg {

class TextureArrayResource final : public BaseResource {
public:
    using BaseResource::BaseResource;

    bool should_reload_on_file_change() const noexcept override { return true; }

    Identifier type_id() const noexcept override { return "TextureArrayResource"; }

    std::span<const ResourceHandle<TextureResource>> textures() const { return m_textures; }

protected:
    /** Constructs a texture from file. Only DDS files are supported. */
    LoadResourceResult load_resource_impl(ResourceLoadingInput& input) override;

private:
    std::vector<ResourceHandle<TextureResource>> m_textures;
};

} // namespace Mg
