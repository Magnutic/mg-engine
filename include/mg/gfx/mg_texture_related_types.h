//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_render_target_params.h
 * Types related to textures, e.g. construction parameter types, texture units.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/utils/mg_assert.h"

#include <cstdint>

namespace Mg::gfx {

/** TextureUnit values may be at most this large. */
static constexpr size_t k_max_texture_unit = 15;

/** TextureUnit -- target to which a sampler may be bound. */
class TextureUnit {
public:
    constexpr explicit TextureUnit(uint32_t unit) : m_unit(unit)
    {
        MG_ASSERT(unit <= k_max_texture_unit);
    }

    constexpr uint32_t get() const noexcept { return m_unit; }

private:
    uint32_t m_unit = 0;
};

struct ImageSize {
    int32_t width{};
    int32_t height{};
};

inline bool operator==(ImageSize lhs, ImageSize rhs) noexcept
{
    return lhs.width == rhs.width && lhs.height == rhs.height;
}

inline bool operator!=(ImageSize lhs, ImageSize rhs) noexcept
{
    return !(lhs == rhs);
}

/** Texture sampling filtering methods. */
enum class TextureFilterMode {
    Nearest, /** Nearest-neighbour filtering. */
    Linear   /** Linearly interpolated -- smooth -- filtering. */
};
/** Input parameters for creating render-target textures. */
struct RenderTargetParams {
    enum class Format {
        RGBA8,   /** Red/Green/Blue/Alpha channels of 8-bit unsigned int */
        RGBA16F, /** Red/Green/Blue/Alpha channels of 16-bit float */
        RGBA32F, /** Red/Green/Blue/Alpha channels of 32-bit float */
        Depth24, /** 24-bit depth */
    };

    Identifier render_target_id{ "<anonymous render target texture>" };

    int32_t width = 0;
    int32_t height = 0;

    int32_t num_mip_levels = 1;

    TextureFilterMode filter_mode{};
    Format texture_format{};
};

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

/** Whether the texture is in sRGB colour space. Generally, this is the case (and is desirable) for
 * colour textures, as it provides precision that more closely matches human visual perception. But
 * data textures such as normal-maps should be stored in linear colour space. SRGBSetting::Default
 * will choose linear for ATI2, since it is used for normal maps.
 */
enum class SRGBSetting {
    Default, /** Automatically choose sRGB setting based on pixel format. */
    sRGB,    /** Always treat as sRGB. */
    Linear   /** Always treat as linear. */
};

/** Configurable settings for a texture. */
struct TextureSettings {
    /** Texture's filtering. See documentation for Filtering enum. */
    Filtering filtering = Filtering::Linear_mipmap_linear;

    /** Texture's edge sampling. See documentation for EdgeSampling enum. */
    EdgeSampling edge_sampling = EdgeSampling::Repeat;

    /** Whether texture data is to be interpreted as being in sRGB colour space. This is usually
     * what you want for colour maps, but not for textures containing other data (e.g. normal maps).
     */
    SRGBSetting sRGB = SRGBSetting::Default;

    /** Whether DXT1 texture has alpha. Unused for other texture formats. */
    bool dxt1_has_alpha = false;
};

} // namespace Mg::gfx
