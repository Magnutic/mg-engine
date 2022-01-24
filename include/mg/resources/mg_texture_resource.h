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

    /** The format in which a texture is compressed. */
    enum class PixelFormat {
        BGR,  /** Uncompressed, 24-bit BGR. */
        BGRA, /** Uncompressed, 32-bit BGRA. */
        DXT1, /** RGB, optionally 1-bit alpha, DXT compression. */
        DXT3, /** ARGB, explicit alpha, DXT compression. */
        DXT5, /** ARGB, interpolated alpha, DXT compression. */
        ATI1, /** Special compression for single-channel images. */
        ATI2  /** Special XY compression format for normal maps. */
    };

    /** How textures are filtered. For most textures, you want LINEAR_MIPMAP_LINEAR. */
    enum class Filtering {
        Nearest,                /** Nearest neighbour filtering */
        Linear,                 /** Smoothly interpolated */
        Nearest_mipmap_nearest, /** Nearest with mipmapping */
        Linear_mipmap_nearest,  /** Linear with mipmapping */
        Nearest_mipmap_linear,  /** Nearest, smooth transitions between mips */
        Linear_mipmap_linear    /** Linear, smooth transitions between mips */
    };

    /** What happens when textures are sampled outside the [0, 1] UV-range. */
    enum class EdgeSampling {
        Repeat,          /** Texture is repeated endlessly */
        Mirrored_repeat, /** Texture is repeated but alternately mirrored */
        Clamp            /** Edge colour is propagated to infinity */
    };

    /** Whether the texture is in sRGB colour space. Generally, this is the case
     * (and is desirable) for colour textures, as it provides precision that
     * more closely matches human visual perception. But data textures such as
     * normal-maps should be stored in linear colour space.
     * SRGBSetting::Default will choose linear for ATI2, since it is used for
     * normal maps.
     */
    enum class SRGBSetting {
        Default, /** Automatically choose sRGB setting based on pixel format. */
        sRGB,    /** Always treat as sRGB. */
        Linear   /** Always treat as linear. */
    };

    /** Info on the format of the texture, this describes how to interpret the
     * binary data. */
    struct Format {
        PixelFormat pixel_format;
        DimT width, height;
        MipIndexT mip_levels;
    };

    /** Configurable settings for this texture. */
    struct Settings {
        /** Texture's filtering. See documentation for Filtering enum. */
        Filtering filtering = Filtering::Linear_mipmap_linear;

        /** Texture's edge sampling. See documentation for EdgeSampling enum. */
        EdgeSampling edge_sampling = EdgeSampling::Repeat;

        /** Whether texture data is to be interpreted as being in sRGB colour space. This is usually
         * what you want for colour maps, but not for textures containing other data (e.g. normal
         * maps).
         */
        SRGBSetting sRGB = SRGBSetting::Default;

        /** Whether DXT1 texture has alpha. Unused for other texture formats. */
        bool dxt1_has_alpha = false;
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

    /** Access the texture settings struct. */
    Settings& settings() noexcept { return m_settings; }

    /** Access the texture settings struct. */
    const Settings& settings() const noexcept { return m_settings; }

    bool should_reload_on_file_change() const noexcept override { return true; }

    Identifier type_id() const noexcept override { return "TextureResource"; }

protected:
    /** Constructs a texture from file. Only DDS files are supported. */
    LoadResourceResult load_resource_impl(ResourceLoadingInput& input) override;

private:
    Format m_format;
    Settings m_settings;
    Array<std::byte> m_pixel_data;
};

} // namespace Mg
