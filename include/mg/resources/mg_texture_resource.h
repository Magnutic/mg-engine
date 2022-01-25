//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_texture_resource.h
 * TextureResource data resource type.
 */

#pragma once

#include "mg/containers/mg_array.h"
#include "mg/gfx/mg_texture_related_types.h"
#include "mg/resource_cache/mg_base_resource.h"
#include "mg/utils/mg_gsl.h"

#include <cstdint>

namespace Mg {

/** Texture resource class supporting DDS texture data. */
class TextureResource final : public BaseResource {
public:
    /** Size type for texture dimensions (width, height, ...). */
    using DimT = uint32_t;

    /** Mip-map index type. */
    using MipIndexT = uint32_t;

    /** Info on the format of the texture, this describes how to interpret the
     * binary data. */
    struct Format {
        gfx::PixelFormat pixel_format;
        DimT width, height;
        MipIndexT mip_levels;
    };

    struct MipLevelData {
        span<const std::byte> data;
        DimT width = 0;
        DimT height = 0;
    };

    using BaseResource::BaseResource;

    /** Access the binary pixel data.
     * @param mip_index Which mipmap to get.
     */
    MipLevelData pixel_data(MipIndexT mip_index) const noexcept;

    /** Get texture format info. */
    const Format& format() const noexcept { return m_format; }

    bool should_reload_on_file_change() const noexcept override { return true; }

    Identifier type_id() const noexcept override { return "TextureResource"; }

protected:
    /** Constructs a texture from file. Only DDS files are supported. */
    LoadResourceResult load_resource_impl(ResourceLoadingInput& input) override;

private:
    Format m_format;
    Array<std::byte> m_pixel_data;
};

} // namespace Mg
