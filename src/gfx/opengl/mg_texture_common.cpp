//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_texture_related_types.h"
#include "mg/resources/mg_texture_resource.h"

#include "mg_glad.h"

namespace Mg::gfx {

// Texture format info as required by OpenGL
struct GlTextureInfo {
    uint32_t internal_format;
    uint32_t format;
    uint32_t type;
    int32_t mip_levels;
    int32_t width;
    int32_t height;
    float aniso;
    bool compressed;
};

// Get texture format info as required by OpenGL
GlTextureInfo gl_texture_info(const TextureResource& resource,
                              const TextureSettings& settings) noexcept
{
    GlTextureInfo info{};

    const auto& format = resource.format();

    info.mip_levels = as<int32_t>(format.mip_levels);
    info.width = as<int32_t>(format.width);
    info.height = as<int32_t>(format.height);

    // Determine whether to use sRGB colour space
    bool sRGB = settings.sRGB == gfx::SRGBSetting::sRGB;

    if (settings.sRGB == gfx::SRGBSetting::Default) {
        // Default to sRGB unless it is a normal map (ATI2 compression)
        sRGB = format.pixel_format != gfx::PixelFormat::ATI2;
    }

    // Pick OpenGL pixel format
    switch (format.pixel_format) {
    case gfx::PixelFormat::DXT1:
        info.type = GL_UNSIGNED_BYTE;
        info.compressed = true;
        info.internal_format =
            settings.dxt1_has_alpha
                ? (sRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
                : (sRGB ? GL_COMPRESSED_SRGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
        break;

    case gfx::PixelFormat::DXT3:
        info.type = GL_UNSIGNED_BYTE;
        info.compressed = true;
        info.internal_format = sRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
                                    : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        break;

    case gfx::PixelFormat::DXT5:
        info.type = GL_UNSIGNED_BYTE;
        info.compressed = true;
        info.internal_format = sRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
                                    : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        break;

    case gfx::PixelFormat::ATI1:
        info.type = GL_UNSIGNED_BYTE;
        info.compressed = true;
        info.internal_format = GL_COMPRESSED_RED_RGTC1;
        break;

    case gfx::PixelFormat::ATI2:
        info.type = GL_UNSIGNED_BYTE;
        info.compressed = true;
        info.internal_format = GL_COMPRESSED_RG_RGTC2;
        break;

    case gfx::PixelFormat::BGR:
        info.type = GL_UNSIGNED_BYTE;
        info.internal_format = GL_RGB8;
        info.format = GL_BGR;
        break;

    case gfx::PixelFormat::BGRA:
        info.type = GL_UNSIGNED_BYTE;
        info.internal_format = GL_RGBA8;
        info.format = GL_BGRA;
        break;

    case gfx::PixelFormat::BPTC_RGB_SFLOAT:
        info.type = GL_NONE;
        info.compressed = true;
        info.internal_format = GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;
        info.format = GL_RGB16;
        break;

    case gfx::PixelFormat::BPTC_RGB_UFLOAT:
        info.type = GL_NONE;
        info.compressed = true;
        info.internal_format = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
        info.format = GL_RGB16;
        break;
    }

    // Get anisotropic filtering level
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &info.aniso);

    return info;
}

// Set up texture sampling parameters for currently bound texture
void set_sampling_params(const TextureSettings& settings) noexcept
{
    GLint edge_sampling = 0;

    switch (settings.edge_sampling) {
    case gfx::EdgeSampling::Clamp:
        // N.B. a common mistake is to use GL_CLAMP here.
        edge_sampling = GL_CLAMP_TO_EDGE;
        break;

    case gfx::EdgeSampling::Repeat:
        edge_sampling = GL_REPEAT;
        break;

    case gfx::EdgeSampling::Mirrored_repeat:
        edge_sampling = GL_MIRRORED_REPEAT;
        break;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, edge_sampling);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, edge_sampling);

    GLint min_filter = 0;
    GLint mag_filter = 0;

    switch (settings.filtering) {
    case gfx::Filtering::Nearest:
        min_filter = GL_NEAREST;
        mag_filter = GL_NEAREST;
        break;
    case gfx::Filtering::Nearest_mipmap_nearest:
        min_filter = GL_NEAREST_MIPMAP_NEAREST;
        mag_filter = GL_NEAREST;
        break;
    case gfx::Filtering::Nearest_mipmap_linear:
        min_filter = GL_NEAREST_MIPMAP_LINEAR;
        mag_filter = GL_NEAREST;
        break;
    case gfx::Filtering::Linear:
        min_filter = GL_LINEAR;
        mag_filter = GL_LINEAR;
        break;
    case gfx::Filtering::Linear_mipmap_nearest:
        min_filter = GL_LINEAR_MIPMAP_NEAREST;
        mag_filter = GL_LINEAR;
        break;
    case gfx::Filtering::Linear_mipmap_linear:
        min_filter = GL_LINEAR_MIPMAP_LINEAR;
        mag_filter = GL_LINEAR;
        break;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
}

} // namespace Mg::gfx
