//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_texture2d.h"

#include "mg/core/mg_runtime_error.h"
#include "mg/resources/mg_texture_resource.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_math_utils.h"

#include "mg_texture_common.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

namespace Mg::gfx {

namespace {

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
        throw RuntimeError{ "gl_internal_format_for_format() undefined for given format type." };
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
        throw RuntimeError{ "gl_format_for_format() undefined for given format type." };
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
        throw RuntimeError{ "gl_type_for_format() undefined for given format type." };
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

    // Allocate storage.
    glTexStorage2D(GL_TEXTURE_2D, info.mip_levels, info.internal_format, info.width, info.height);

    MG_CHECK_GL_ERROR();

    glBindTexture(GL_TEXTURE_2D, 0);
    return TextureHandle{ id };
}

//--------------------------------------------------------------------------------------------------
// Helpers for creating texture from TextureResource
//--------------------------------------------------------------------------------------------------

void upload_compressed_mip(int32_t mip_index,
                           const GlTextureInfo& info,
                           int32_t size,
                           const GLvoid* data) noexcept
{
    const auto width = max(1, info.width >> mip_index);
    const auto height = max(1, info.height >> mip_index);

    // N.B. OpenGL docs are misleading about the 'format' param, it should have been called
    // 'internalformat' to avoid confusion with glTexImage2D's 'format' parameter.
    glCompressedTexSubImage2D(
        GL_TEXTURE_2D, mip_index, 0, 0, width, height, info.internal_format, size, data);

    MG_CHECK_GL_ERROR();
}

void upload_uncompressed_mip(int32_t mip_index,
                             const GlTextureInfo& info,
                             int32_t /* size */,
                             const GLvoid* data) noexcept
{
    const auto width = max(1, info.width >> mip_index);
    const auto height = max(1, info.height >> mip_index);

    glTexSubImage2D(GL_TEXTURE_2D, mip_index, 0, 0, width, height, info.format, info.type, data);

    MG_CHECK_GL_ERROR();
}

TextureHandle generate_gl_texture_from(const TextureResource& resource,
                                       const TextureSettings& settings) noexcept
{
    const GlTextureInfo info = gl_texture_info(resource, settings);

    GLuint texture_id{};
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // Set anisotropic filtering level
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, info.aniso);

    // Allocate storage.
    glTexStorage2D(GL_TEXTURE_2D, info.mip_levels, info.internal_format, info.width, info.height);

    decltype(upload_compressed_mip)* upload_function = (info.compressed ? upload_compressed_mip
                                                                        : upload_uncompressed_mip);

    // Upload texture data, mipmap by mipmap
    for (int32_t mip_index = 0; mip_index < info.mip_levels; ++mip_index) {
        const auto mip_data = resource.pixel_data(as<uint32_t>(mip_index));
        auto pixels = mip_data.data.data();
        auto size = as<int32_t>(mip_data.data.size_bytes());

        upload_function(mip_index, info, size, pixels);
    }

    set_sampling_params(settings);
    MG_CHECK_GL_ERROR();

    return TextureHandle{ texture_id };
}

TextureHandle generate_gl_texture_from(std::span<const uint8_t> rgba8_buffer,
                                       const int32_t width,
                                       const int32_t height,
                                       const TextureSettings& settings)
{
    GLuint texture_id{};
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    MG_ASSERT(as<int32_t>(rgba8_buffer.size()) == width * height * 4);

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA8,
                 width,
                 height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 rgba8_buffer.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    set_sampling_params(settings);
    MG_CHECK_GL_ERROR();

    return TextureHandle{ texture_id };
}

} // namespace

//--------------------------------------------------------------------------------------------------
// Texture2D implementation
//--------------------------------------------------------------------------------------------------

Texture2D Texture2D::from_texture_resource(const TextureResource& resource,
                                           const TextureSettings& settings)
{
    Texture2D tex(generate_gl_texture_from(resource, settings));

    tex.m_id = resource.resource_id();
    tex.m_image_size.width = as<int32_t>(resource.format().width);
    tex.m_image_size.height = as<int32_t>(resource.format().height);

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

Texture2D Texture2D::from_rgba8_buffer(Identifier id,
                                       std::span<const uint8_t> rgba8_buffer,
                                       int32_t width,
                                       int32_t height,
                                       const TextureSettings& settings)
{
    Texture2D tex(generate_gl_texture_from(rgba8_buffer, width, height, settings));

    tex.m_id = id;
    tex.m_image_size.width = width;
    tex.m_image_size.height = height;

    return tex;
}

// Unload texture from OpenGL context
void Texture2D::unload() noexcept
{
    const auto tex_id = m_handle.as_gl_id();

    if (tex_id != 0) {
        glDeleteTextures(1, &tex_id);
    }
}

} // namespace Mg::gfx
