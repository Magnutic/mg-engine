//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

#include "mg/gfx/mg_texture2d.h"

#include "mg/resources/mg_texture_resource.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

#include <stdexcept>

namespace Mg::gfx {

//--------------------------------------------------------------------------------------------------
// render_target helper functions
//--------------------------------------------------------------------------------------------------

// Helper function to get appropriate texture internal_format for a given render-target format
static GLint gl_internal_format_for_format(RenderTargetParams::Format format)
{
    switch (format) {
    case RenderTargetParams::Format::RGBA8: return GL_RGBA8;
    case RenderTargetParams::Format::RGBA16F: return GL_RGBA16F;
    case RenderTargetParams::Format::RGBA32F: return GL_RGBA32F;
    case RenderTargetParams::Format::Depth24: return GL_DEPTH24_STENCIL8;
    default:
        throw std::logic_error{
            "gl_internal_format_for_format() undefined for given format type."
        };
    }
}

// Helper function to get appropriate texture format for a given render-target format
static uint32_t gl_format_for_format(RenderTargetParams::Format format)
{
    switch (format) {
    case RenderTargetParams::Format::RGBA8: [[fallthrough]];
    case RenderTargetParams::Format::RGBA16F: [[fallthrough]];
    case RenderTargetParams::Format::RGBA32F: return GL_RGBA;
    case RenderTargetParams::Format::Depth24: return GL_DEPTH_STENCIL;
    default: throw std::logic_error{ "gl_type_for_format() undefined for given format type." };
    }
}

// Helper function to get appropriate texture data type for a given render-target format
static uint32_t gl_type_for_format(RenderTargetParams::Format format)
{
    switch (format) {
    case RenderTargetParams::Format::RGBA8: return GL_UNSIGNED_BYTE;
    case RenderTargetParams::Format::RGBA16F: return GL_FLOAT;
    case RenderTargetParams::Format::RGBA32F: return GL_FLOAT;
    case RenderTargetParams::Format::Depth24: return GL_UNSIGNED_INT_24_8;
    default: throw std::logic_error{ "gl_type_for_format() undefined for given format type." };
    }
}

// Create a texture appropriate for use with the given rendertarget settings
static Texture2D::GfxApiHandle generate_gl_render_target_texture(const RenderTargetParams& params)
{
    uint32_t id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    GLint filter_mode{};
    switch (params.filter_mode) {
    case TextureFilterMode::Nearest: filter_mode = GL_NEAREST; break;
    case TextureFilterMode::Linear: filter_mode = GL_LINEAR; break;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_mode);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLint    internal_format = gl_internal_format_for_format(params.texture_format);
    uint32_t format          = gl_format_for_format(params.texture_format);
    uint32_t type            = gl_type_for_format(params.texture_format);

    // Allocate storage for texture target
    glTexImage2D(GL_TEXTURE_2D,   // target
                 0,               // miplevel
                 internal_format, // internalformat
                 params.width,    // texture width
                 params.height,   // texture height
                 0,               // border (must be 0)
                 format,          // format
                 type,            // pixel data type
                 nullptr          // pixel data
    );

    MG_CHECK_GL_ERROR();

    glBindTexture(GL_TEXTURE_2D, 0);
    return Texture2D::GfxApiHandle{ id };
}

//--------------------------------------------------------------------------------------------------
// from_texture_resource helper functions
//--------------------------------------------------------------------------------------------------

// Texture format info as required by OpenGL
struct GlTextureInfo {
    uint32_t internal_format;
    uint32_t format;
    int32_t  mip_levels;
    int32_t  width, height;
    float    aniso;
    bool     compressed;
};

// Get texture format info as required by OpenGL
static GlTextureInfo gl_texture_info(const TextureResource& texture)
{
    GlTextureInfo info{};

    const auto& tex_format   = texture.format();
    const auto& tex_settings = texture.settings();

    info.mip_levels = static_cast<int32_t>(tex_format.mip_levels);
    info.width      = static_cast<int32_t>(tex_format.width);
    info.height     = static_cast<int32_t>(tex_format.height);

    // Determine whether to use sRGB colour space
    bool sRGB = tex_settings.sRGB == TextureResource::SRGBSetting::SRGB;

    if (tex_settings.sRGB == TextureResource::SRGBSetting::DEFAULT) {
        // Default to sRGB unless it is a normal map (ATI2 compression)
        sRGB = tex_format.pixel_format != TextureResource::PixelFormat::ATI2;
    }

    // Pick OpenGL pixel format
    switch (tex_format.pixel_format) {
    case TextureResource::PixelFormat::DXT1:
        info.compressed = true;
        info.internal_format =
            tex_settings.dxt1_has_alpha
                ? sRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
                : sRGB ? GL_COMPRESSED_SRGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        break;

    case TextureResource::PixelFormat::DXT3:
        info.compressed = true;
        info.internal_format =
            sRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        break;

    case TextureResource::PixelFormat::DXT5:
        info.compressed = true;
        info.internal_format =
            sRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        break;

    case TextureResource::PixelFormat::ATI2:
        info.compressed      = true;
        info.internal_format = GL_COMPRESSED_RG_RGTC2;
        break;

    case TextureResource::PixelFormat::BGR:
        info.internal_format = GL_RGB8;
        info.format          = GL_BGR;
        break;

    case TextureResource::PixelFormat::BGRA:
        info.internal_format = GL_RGBA8;
        info.format          = GL_BGRA;
        break;
    }

    // Get anisotropic filtering level
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &info.aniso);

    return info;
}

static void allocate_texture_storage(const GlTextureInfo& info)
{
    glTexStorage2D(GL_TEXTURE_2D, info.mip_levels, info.internal_format, info.width, info.height);
}

// Set up texture sampling parameters for currently bound texture
static void set_sampling_params(const TextureResource::Settings& settings)
{
    GLint edge_sampling = 0;

    switch (settings.edge_sampling) {
    case TextureResource::EdgeSampling::CLAMP:
        // N.B. a common mistake is to use GL_CLAMP here.
        edge_sampling = GL_CLAMP_TO_EDGE;
        break;

    case TextureResource::EdgeSampling::REPEAT: edge_sampling = GL_REPEAT; break;

    case TextureResource::EdgeSampling::MIRRORED_REPEAT: edge_sampling = GL_MIRRORED_REPEAT; break;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, edge_sampling);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, edge_sampling);

    GLint min_filter = 0, mag_filter = 0;

    switch (settings.filtering) {
    case TextureResource::Filtering::NEAREST:
        min_filter = GL_NEAREST;
        mag_filter = GL_NEAREST;
        break;
    case TextureResource::Filtering::NEAREST_MIPMAP_NEAREST:
        min_filter = GL_NEAREST_MIPMAP_NEAREST;
        mag_filter = GL_NEAREST;
        break;
    case TextureResource::Filtering::NEAREST_MIPMAP_LINEAR:
        min_filter = GL_NEAREST_MIPMAP_LINEAR;
        mag_filter = GL_NEAREST;
        break;
    case TextureResource::Filtering::LINEAR:
        min_filter = GL_LINEAR;
        mag_filter = GL_LINEAR;
        break;
    case TextureResource::Filtering::LINEAR_MIPMAP_NEAREST:
        min_filter = GL_LINEAR_MIPMAP_NEAREST;
        mag_filter = GL_LINEAR;
        break;
    case TextureResource::Filtering::LINEAR_MIPMAP_LINEAR:
        min_filter = GL_LINEAR_MIPMAP_LINEAR;
        mag_filter = GL_LINEAR;
        break;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
}

static void upload_compressed_mip(bool    preallocated,
                                  int32_t mip_index,
                                  uint32_t /* format */,
                                  uint32_t      internal_format,
                                  int32_t       width,
                                  int32_t       height,
                                  int32_t       size,
                                  const GLvoid* data)
{
    if (preallocated) {
        // N.B. OpenGL docs are misleading about the 'format' param, it
        // should have be called 'internalformat' to avoid confusion with
        // glTexImage2D's 'format' parameter.
        glCompressedTexSubImage2D(
            GL_TEXTURE_2D, mip_index, 0, 0, width, height, internal_format, size, data);

        return;
    }

    glCompressedTexImage2D(GL_TEXTURE_2D, mip_index, internal_format, width, height, 0, size, data);

    MG_CHECK_GL_ERROR();
}

static void upload_uncompressed_mip(bool     preallocated,
                                    int32_t  mip_index,
                                    uint32_t format,
                                    uint32_t internal_format,
                                    int32_t  width,
                                    int32_t  height,
                                    int32_t /* size */,
                                    const GLvoid* data)
{
    if (preallocated) {
        glTexSubImage2D(
            GL_TEXTURE_2D, mip_index, 0, 0, width, height, format, GL_UNSIGNED_BYTE, data);

        return;
    }

    // N.B. internalformat is a uint32_t in glCompressedTexImage2D, but a GLint
    // in glTexImage2D. Another day in OpenGL-land!
    glTexImage2D(GL_TEXTURE_2D,
                 mip_index,
                 static_cast<GLint>(internal_format),
                 width,
                 height,
                 0,
                 format,
                 GL_UNSIGNED_BYTE,
                 data);

    MG_CHECK_GL_ERROR();
}

static Texture2D::GfxApiHandle generate_gl_texture_from(const TextureResource& texture_resource)
{
    GlTextureInfo info = gl_texture_info(texture_resource);

    GLuint texture_id{};
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // Set anisotropic filtering level
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, info.aniso);

    // If possible, allocate storage in one go (depends on availability of OpenGL extension)
    static bool preallocate = (GLboolean(GLAD_GL_ARB_texture_storage) != 0);

    if (preallocate) { allocate_texture_storage(info); }

    decltype(upload_compressed_mip)* upload_function =
        (info.compressed ? upload_compressed_mip : upload_uncompressed_mip);

    // Upload texture data, mipmap by mipmap
    for (int32_t mip_index = 0; mip_index < info.mip_levels; ++mip_index) {
        auto mip_data = texture_resource.pixel_data(narrow<TextureResource::MipIndexT>(mip_index));
        auto pixels   = static_cast<const GLvoid*>(mip_data.data.data());

        auto size       = narrow<int32_t>(mip_data.data.size_bytes());
        auto mip_width  = narrow<int32_t>(mip_data.width);
        auto mip_height = narrow<int32_t>(mip_data.height);

        upload_function(preallocate,
                        mip_index,
                        info.format,
                        info.internal_format,
                        mip_width,
                        mip_height,
                        size,
                        pixels);
    }

    set_sampling_params(texture_resource.settings());
    MG_CHECK_GL_ERROR();

    return Texture2D::GfxApiHandle{ texture_id };
}

//--------------------------------------------------------------------------------------------------
// Texture2D implementation
//--------------------------------------------------------------------------------------------------

Texture2D Texture2D::from_texture_resource(const TextureResource& texture_resource)
{
    Texture2D tex(generate_gl_texture_from(texture_resource));

    tex.m_id                = texture_resource.resource_id();
    tex.m_image_size.width  = int32_t(texture_resource.format().width);
    tex.m_image_size.height = int32_t(texture_resource.format().height);

    return tex;
}

Texture2D Texture2D::render_target(const RenderTargetParams& params)
{
    Texture2D tex(generate_gl_render_target_texture(params));

    tex.m_id                = params.render_target_id;
    tex.m_image_size.width  = params.width;
    tex.m_image_size.height = params.height;

    return tex;
}

void Texture2D::bind_to(TextureUnit unit) const noexcept
{
    auto tex_id = static_cast<uint32_t>(gfx_api_handle());

    glActiveTexture(GL_TEXTURE0 + unit.get());
    glBindTexture(GL_TEXTURE_2D, tex_id);
}

Texture2D::Texture2D(GfxApiHandle gfx_api_handle)
    : m_gfx_api_handle(static_cast<uint32_t>(gfx_api_handle))
{}


// Unload texture from OpenGL context
void Texture2D::unload()
{
    auto tex_id = static_cast<uint32_t>(gfx_api_handle());

    if (tex_id != 0) {
        glDeleteTextures(1, &tex_id);
        tex_id = 0;
    }
}

} // namespace Mg::gfx
