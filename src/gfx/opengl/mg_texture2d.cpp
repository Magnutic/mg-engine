//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_texture2d.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/resources/mg_texture_resource.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

namespace Mg::gfx {

namespace {

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

//--------------------------------------------------------------------------------------------------
// render_target helper functions
//--------------------------------------------------------------------------------------------------

// Helper function to get appropriate texture internal_format for a given render-target format
uint32_t gl_internal_format_for_format(RenderTargetParams::Format format)
{
    switch (format) {
    case RenderTargetParams::Format::RGBA8:
        return GL_RGBA8;
    case RenderTargetParams::Format::RGBA16F:
        return GL_RGBA16F;
    case RenderTargetParams::Format::RGBA32F:
        return GL_RGBA32F;
    case RenderTargetParams::Format::Depth24:
        return GL_DEPTH24_STENCIL8;
    default:
        g_log.write_error("gl_internal_format_for_format() undefined for given format type.");
        throw RuntimeError();
    }
}

// Helper function to get appropriate texture format for a given render-target format
uint32_t gl_format_for_format(RenderTargetParams::Format format)
{
    switch (format) {
    case RenderTargetParams::Format::RGBA8:
        [[fallthrough]];
    case RenderTargetParams::Format::RGBA16F:
        [[fallthrough]];
    case RenderTargetParams::Format::RGBA32F:
        return GL_RGBA;
    case RenderTargetParams::Format::Depth24:
        return GL_DEPTH_STENCIL;
    default:
        g_log.write_error("gl_format_for_format() undefined for given format type.");
        throw RuntimeError();
    }
}

// Helper function to get appropriate texture data type for a given render-target format
uint32_t gl_type_for_format(RenderTargetParams::Format format)
{
    switch (format) {
    case RenderTargetParams::Format::RGBA8:
        return GL_UNSIGNED_BYTE;
    case RenderTargetParams::Format::RGBA16F:
        return GL_FLOAT;
    case RenderTargetParams::Format::RGBA32F:
        return GL_FLOAT;
    case RenderTargetParams::Format::Depth24:
        return GL_UNSIGNED_INT_24_8;
    default:
        g_log.write_error("gl_type_for_format() undefined for given format type.");
        throw RuntimeError();
    }
}

GlTextureInfo gl_texture_info_for_render_target(const RenderTargetParams& params)
{
    GlTextureInfo tex_info = {};
    tex_info.compressed = false;
    tex_info.format = gl_format_for_format(params.texture_format);
    tex_info.internal_format = gl_internal_format_for_format(params.texture_format);
    tex_info.type = gl_type_for_format(params.texture_format);
    tex_info.width = params.width;
    tex_info.height = params.height;
    tex_info.mip_levels = params.num_mip_levels;
    tex_info.aniso = 0.0f;
    return tex_info;
}

// Create a texture appropriate for use with the given rendertarget settings
TextureHandle generate_gl_render_target_texture(const RenderTargetParams& params)
{
    uint32_t id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    const GlTextureInfo info = gl_texture_info_for_render_target(params);

    const auto [min_filter, mag_filter] = [&] {
        if (info.format == GL_DEPTH_STENCIL || params.filter_mode == TextureFilterMode::Nearest) {
            return std::pair(GL_NEAREST, GL_NEAREST);
        }
        return (info.mip_levels > 1) ? std::pair(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR)
                                     : std::pair(GL_LINEAR, GL_LINEAR);
    }();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // If possible, allocate storage in one go (depends on availability of OpenGL extension)
    static const bool have_texture_storage = (GLAD_GL_ARB_texture_storage != 0);

    if (have_texture_storage) {
        glTexStorage2D(GL_TEXTURE_2D,
                       info.mip_levels,
                       info.internal_format,
                       info.width,
                       info.height);
    }
    else {
        for (auto i = 0; i < info.mip_levels; ++i) {
            const auto mip_width = info.width >> i;
            const auto mip_height = info.height >> i;

            // Allocate storage for texture target
            glTexImage2D(GL_TEXTURE_2D,                       // target
                         i,                                   // miplevel
                         narrow<GLint>(info.internal_format), // internalformat
                         mip_width,                           // texture width
                         mip_height,                          // texture height
                         0,                                   // border (must be 0)
                         info.format,                         // format
                         info.type,                           // pixel data type
                         nullptr                              // pixel data
            );
        }
    }

    MG_CHECK_GL_ERROR();

    glBindTexture(GL_TEXTURE_2D, 0);
    return TextureHandle{ id };
}

//--------------------------------------------------------------------------------------------------
// from_texture_resource helper functions
//--------------------------------------------------------------------------------------------------

// Get texture format info as required by OpenGL
GlTextureInfo gl_texture_info(const TextureResource& texture) noexcept
{
    GlTextureInfo info{};

    const auto& tex_format = texture.format();
    const auto& tex_settings = texture.settings();

    info.mip_levels = narrow<int32_t>(tex_format.mip_levels);
    info.width = narrow<int32_t>(tex_format.width);
    info.height = narrow<int32_t>(tex_format.height);

    // Texture channels are all 8-bit, so far.
    info.type = GL_UNSIGNED_BYTE;

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
        info.internal_format = tex_settings.dxt1_has_alpha
                                   ? sRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
                                          : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
                                   : sRGB ? GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
                                          : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        break;

    case TextureResource::PixelFormat::DXT3:
        info.compressed = true;
        info.internal_format = sRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
                                    : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        break;

    case TextureResource::PixelFormat::DXT5:
        info.compressed = true;
        info.internal_format = sRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
                                    : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        break;

    case TextureResource::PixelFormat::ATI2:
        info.compressed = true;
        info.internal_format = GL_COMPRESSED_RG_RGTC2;
        break;

    case TextureResource::PixelFormat::BGR:
        info.internal_format = GL_RGB8;
        info.format = GL_BGR;
        break;

    case TextureResource::PixelFormat::BGRA:
        info.internal_format = GL_RGBA8;
        info.format = GL_BGRA;
        break;
    }

    // Get anisotropic filtering level
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &info.aniso);

    return info;
}

// Set up texture sampling parameters for currently bound texture
void set_sampling_params(const TextureResource::Settings& settings) noexcept
{
    GLint edge_sampling = 0;

    switch (settings.edge_sampling) {
    case TextureResource::EdgeSampling::CLAMP:
        // N.B. a common mistake is to use GL_CLAMP here.
        edge_sampling = GL_CLAMP_TO_EDGE;
        break;

    case TextureResource::EdgeSampling::REPEAT:
        edge_sampling = GL_REPEAT;
        break;

    case TextureResource::EdgeSampling::MIRRORED_REPEAT:
        edge_sampling = GL_MIRRORED_REPEAT;
        break;
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

void upload_compressed_mip(bool preallocated,
                           int32_t mip_index,
                           const GlTextureInfo& info,
                           int32_t size,
                           const GLvoid* data) noexcept
{
    const auto width = info.width >> mip_index;
    const auto height = info.height >> mip_index;

    if (preallocated) {
        // N.B. OpenGL docs are misleading about the 'format' param, it should have been called
        // 'internalformat' to avoid confusion with glTexImage2D's 'format' parameter.
        glCompressedTexSubImage2D(
            GL_TEXTURE_2D, mip_index, 0, 0, width, height, info.internal_format, size, data);

        return;
    }

    glCompressedTexImage2D(
        GL_TEXTURE_2D, mip_index, info.internal_format, width, height, 0, size, data);

    MG_CHECK_GL_ERROR();
}

void upload_uncompressed_mip(bool preallocated,
                             int32_t mip_index,
                             const GlTextureInfo& info,
                             int32_t /* size */,
                             const GLvoid* data) noexcept
{
    const auto width = info.width >> mip_index;
    const auto height = info.height >> mip_index;

    if (preallocated) {
        glTexSubImage2D(
            GL_TEXTURE_2D, mip_index, 0, 0, width, height, info.format, info.type, data);

        return;
    }

    // N.B. internalformat is unsigned in glCompressedTexImage2D, but signed in glTexImage2D.
    // Another day in OpenGL-land!
    glTexImage2D(GL_TEXTURE_2D,
                 mip_index,
                 narrow<GLint>(info.internal_format),
                 width,
                 height,
                 0,
                 info.format,
                 info.type,
                 data);

    MG_CHECK_GL_ERROR();
}

TextureHandle generate_gl_texture_from(const TextureResource& texture_resource) noexcept
{
    const GlTextureInfo info = gl_texture_info(texture_resource);

    GLuint texture_id{};
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // Set anisotropic filtering level
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, info.aniso);

    // If possible, allocate storage in one go (depends on availability of OpenGL extension)
    static const bool preallocate = (GLAD_GL_ARB_texture_storage != 0);

    if (preallocate) {
        glTexStorage2D(GL_TEXTURE_2D,
                       info.mip_levels,
                       info.internal_format,
                       info.width,
                       info.height);
    }

    decltype(upload_compressed_mip)* upload_function = (info.compressed ? upload_compressed_mip
                                                                        : upload_uncompressed_mip);

    // Upload texture data, mipmap by mipmap
    for (int32_t mip_index = 0; mip_index < info.mip_levels; ++mip_index) {
        const auto mip_data = texture_resource.pixel_data(narrow<uint32_t>(mip_index));
        auto pixels = mip_data.data.data();
        auto size = narrow<int32_t>(mip_data.data.size_bytes());

        upload_function(preallocate, mip_index, info, size, pixels);
    }

    set_sampling_params(texture_resource.settings());
    MG_CHECK_GL_ERROR();

    return TextureHandle{ texture_id };
}

} // namespace

//--------------------------------------------------------------------------------------------------
// Texture2D implementation
//--------------------------------------------------------------------------------------------------

Texture2D Texture2D::from_texture_resource(const TextureResource& texture_resource)
{
    Texture2D tex(generate_gl_texture_from(texture_resource));

    tex.m_id = texture_resource.resource_id();
    tex.m_image_size.width = narrow<int32_t>(texture_resource.format().width);
    tex.m_image_size.height = narrow<int32_t>(texture_resource.format().height);

    return tex;
}

Texture2D Texture2D::render_target(const RenderTargetParams& params)
{
    Texture2D tex(generate_gl_render_target_texture(params));

    tex.m_id = params.render_target_id;
    tex.m_image_size.width = params.width;
    tex.m_image_size.height = params.height;

    return tex;
}

// Unload texture from OpenGL context
void Texture2D::unload() noexcept
{
    const auto tex_id = narrow<GLuint>(m_handle.get());

    if (tex_id != 0) {
        glDeleteTextures(1, &tex_id);
    }
}

} // namespace Mg::gfx
