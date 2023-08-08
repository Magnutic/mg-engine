//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_texture_cube.h"

#include "mg/core/mg_runtime_error.h"
#include "mg/resources/mg_texture_resource.h"
#include "mg/utils/mg_math_utils.h"

#include "mg_texture_common.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

namespace Mg::gfx {

namespace {

//--------------------------------------------------------------------------------------------------
// Helpers for creating texture from TextureResource
//--------------------------------------------------------------------------------------------------

void upload_compressed_mip(GLuint texture_id,
                           int32_t mip_index,
                           uint32_t image_index,
                           const GlTextureInfo& info,
                           int32_t size,
                           const GLvoid* data) noexcept
{
    const auto width = max(1, info.width >> mip_index);
    const auto height = max(1, info.height >> mip_index);

    // N.B. OpenGL docs are misleading about the 'format' param; it should have been called
    // 'internalformat' to avoid confusion with glTexImage2D's 'format' parameter.
    glCompressedTextureSubImage3D(texture_id,
                                  mip_index,
                                  0,
                                  0,
                                  as<GLint>(image_index),
                                  width,
                                  height,
                                  1,
                                  info.internal_format,
                                  size,
                                  data);

    MG_CHECK_GL_ERROR();
}

void upload_uncompressed_mip(GLuint texture_id,
                             int32_t mip_index,
                             uint32_t image_index,
                             const GlTextureInfo& info,
                             int32_t /* size */,
                             const GLvoid* data) noexcept
{
    const auto width = max(1, info.width >> mip_index);
    const auto height = max(1, info.height >> mip_index);

    glTextureSubImage3D(texture_id,
                        mip_index,
                        0,
                        0,
                        as<GLint>(image_index),
                        width,
                        height,
                        1,
                        info.format,
                        info.type,
                        data);

    MG_CHECK_GL_ERROR();
}

TextureHandle generate_gl_texture_from(const TextureResource& resource,
                                       const TextureSettings& settings)
{
    const GlTextureInfo info = gl_texture_info(resource, settings);

    if (!resource.is_cube_map()) {
        throw Mg::RuntimeError{
            "Error attempting to use texture {} as a cube map: texture is not a cube map.",
            resource.resource_id().str_view()
        };
    }

    GLuint texture_id{};
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &texture_id);

    // Set anisotropic filtering level
    glTextureParameterf(texture_id, GL_TEXTURE_MAX_ANISOTROPY, info.aniso);

    // Allocate storage.
    glTextureStorage2D(texture_id, info.mip_levels, info.internal_format, info.width, info.height);

    decltype(upload_compressed_mip)* upload_function = (info.compressed ? upload_compressed_mip
                                                                        : upload_uncompressed_mip);

    // Upload texture data, mipmap by mipmap
    for (uint32_t image_index = 0; image_index < 6; ++image_index) {
        for (int32_t mip_index = 0; mip_index < info.mip_levels; ++mip_index) {
            const auto mip_data = resource.pixel_data(as<uint32_t>(mip_index), image_index);
            auto pixels = mip_data.data.data();
            auto size = as<int32_t>(mip_data.data.size_bytes());

            upload_function(texture_id, mip_index, image_index, info, size, pixels);
        }
    }

    set_sampling_params(settings);
    MG_CHECK_GL_ERROR();

    return TextureHandle{ texture_id };
}

} // namespace

//--------------------------------------------------------------------------------------------------
// TextureCube implementation
//--------------------------------------------------------------------------------------------------

TextureCube TextureCube::from_texture_resource(const TextureResource& resource,
                                               const TextureSettings& settings)
{
    TextureCube tex(generate_gl_texture_from(resource, settings));

    tex.m_id = resource.resource_id();
    tex.m_image_size.width = as<int32_t>(resource.format().width);
    tex.m_image_size.height = as<int32_t>(resource.format().height);

    return tex;
}

// Unload texture from OpenGL context
void TextureCube::unload() noexcept
{
    const auto tex_id = m_handle.as_gl_id();

    if (tex_id != 0) {
        glDeleteTextures(1, &tex_id);
    }
}

} // namespace Mg::gfx
