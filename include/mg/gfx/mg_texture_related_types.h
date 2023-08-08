//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_render_target_params.h
 * Types related to textures, e.g. construction parameter types, texture units.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_optional.h"

#include <array>
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
    /** Nearest-neighbour filtering. */
    Nearest,
    /** Linearly interpolated -- smooth -- filtering. */
    Linear
};
/** Input parameters for creating render-target textures. */
struct RenderTargetParams {
    enum class Format {
        /** Red/Green/Blue/Alpha channels of 8-bit unsigned int */
        RGBA8,
        /** Red/Green/Blue/Alpha channels of 16-bit float */
        RGBA16F,
        /** Red/Green/Blue/Alpha channels of 32-bit float */
        RGBA32F,
        /** 24-bit depth */
        Depth24,
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
    /** Uncompressed, 24-bit BGR. */
    BGR,
    /** Uncompressed, 32-bit BGRA. */
    BGRA,
    /** RGB, optionally 1-bit alpha, DXT compression. */
    DXT1,
    /** ARGB, explicit alpha, DXT compression. */
    DXT3,
    /** ARGB, interpolated alpha, DXT compression. */
    DXT5,
    /** Special compression for single-channel images. */
    ATI1,
    /** Special XY compression format for normal maps. */
    ATI2,
    /** RGB, unsigned floating-point values, BPTC compression. */
    BPTC_RGB_UFLOAT,
    /** RGB, signed floating-point values, BPTC compression. */
    BPTC_RGB_SFLOAT,
};

/** How textures are filtered. For most textures, you want LINEAR_MIPMAP_LINEAR. */
enum class Filtering {
    /** Nearest neighbour filtering */
    Nearest,
    /** Smoothly interpolated */
    Linear,
    /** Nearest with mipmapping */
    Nearest_mipmap_nearest,
    /** Linear with mipmapping */
    Linear_mipmap_nearest,
    /** Nearest, smooth transitions between mips */
    Nearest_mipmap_linear,
    /** Linear, smooth transitions between mips */
    Linear_mipmap_linear
};

/** What happens when textures are sampled outside the [0, 1] UV-range. */
enum class EdgeSampling {
    /** Texture is repeated endlessly */
    Repeat,
    /** Texture is repeated but alternately mirrored */
    Mirrored_repeat,
    /** Edge colour is propagated to infinity */
    Clamp
};

/** Whether the texture is in sRGB colour space. Generally, this is the case (and is desirable) for
 * colour textures, as it provides precision that more closely matches human visual perception. But
 * data textures such as normal-maps should be stored in linear colour space. SRGBSetting::Default
 * will choose linear for ATI2, since it is used for normal maps.
 */
enum class SRGBSetting {
    /** Automatically choose sRGB setting based on pixel format. */
    Default,
    /** Always treat as sRGB. */
    sRGB,
    /** Always treat as linear. */
    Linear
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

/** Category of texture. */
enum class TextureCategory {
    /** Only diffuse (surface colour) information; no alpha channel. */
    Diffuse,

    /** Diffuse (surface colour) information with an alpha channel representing transparency. */
    Diffuse_transparent,

    /** Diffuse (surface colour) and an alpha channel with some data other than transparency. */
    Diffuse_alpha,

    /** Surface normal map. */
    Normal,

    /** Specular-glossiness map. Specular colour in RGB channels and glossiness in alpha channel.
     * Specular-workflow alternative to Ao_roughness_metallic.
     */
    Specular_gloss,

    /** Red channel: ambient occlusion, green channel: surface roughness, blue channel:
     * metallicness. Metallic-roughness-workflow alternative to Specular_gloss.
     */
    Ao_roughness_metallic,
};

/** Mapping associating filename suffixes to texture categories. This lets us deduce category based
 * simply on filename, removing the need for extra configuration files or embedded metadata.
 */
inline constexpr std::array<std::pair<TextureCategory, std::string_view>, 6>
    g_texture_category_to_filename_suffix_map = { //
        { { TextureCategory::Diffuse, "_d" },
          { TextureCategory::Diffuse_transparent, "_t" },
          { TextureCategory::Diffuse_alpha, "_da" },
          { TextureCategory::Normal, "_n" },
          { TextureCategory::Specular_gloss, "_s" },
          { TextureCategory::Ao_roughness_metallic, "_arm" }

        }
    };

/** Deduce category depending on filename suffix. */
Opt<TextureCategory> deduce_texture_category(std::string_view texture_filename);

} // namespace Mg::gfx
