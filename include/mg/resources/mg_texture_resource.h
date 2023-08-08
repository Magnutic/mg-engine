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

namespace CubemapFace {
enum Value : uint32_t { positive_x, negative_x, positive_y, negative_y, positive_z, negative_z };
}

/** Texture resource class supporting DDS texture data. */
class TextureResource final : public BaseResource {
public:
    /** Info on the format of the texture. This describes how to interpret the binary data. */
    struct Format {
        gfx::PixelFormat pixel_format = gfx::PixelFormat::BGR;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mip_levels = 0;
        uint32_t num_images = 0;
    };

    struct MipLevelData {
        std::span<const std::byte> data;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    using BaseResource::BaseResource;

    /** Access the binary pixel data.
     * @param mip_index Which mipmap to get.
     * @param image_index index of the image. For a cubemap, you can use one of the values under
     * Mg::CubemapFace.
     */
    MipLevelData pixel_data(uint32_t mip_index, uint32_t image_index = 0) const noexcept;

    /** Get texture format info. */
    const Format& format() const noexcept { return m_format; }

    bool should_reload_on_file_change() const noexcept override { return true; }

    Identifier type_id() const noexcept override { return "TextureResource"; }

    bool is_cube_map() const noexcept { return m_format.num_images == 6; }

protected:
    /** Constructs a texture from file. Only DDS files are supported. */
    LoadResourceResult load_resource_impl(ResourceLoadingInput& input) override;

private:
    Format m_format;
    Array<std::byte> m_pixel_data;
};

} // namespace Mg
