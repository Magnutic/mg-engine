//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_buffer_texture.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/gfx/mg_gfx_debug_group.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

namespace Mg::gfx {

uint32_t buffer_texture_type_to_gl_enums(BufferTexture::Type type)
{
    using Channels = BufferTexture::Channels;
    using Fmt = BufferTexture::Format;
    using BitDepth = BufferTexture::BitDepth;

    switch (type.channels) {
    case Channels::R:
        switch (type.fmt) {
        case Fmt::UNSIGNED_NORMALISED:
            switch (type.bit_depth) {
            case BitDepth::BITS_8:
                return GL_R8;
            case BitDepth::BITS_16:
                return GL_R16;
            case BitDepth::BITS_32:
                MG_ASSERT(false && "Unsupported combination");
                break;
            }
            break;
        case Fmt::SIGNED:
            switch (type.bit_depth) {
            case BitDepth::BITS_8:
                return GL_R8I;
            case BitDepth::BITS_16:
                return GL_R16I;
            case BitDepth::BITS_32:
                return GL_R32I;
            }
            break;
        case Fmt::UNSIGNED:
            switch (type.bit_depth) {
            case BitDepth::BITS_8:
                return GL_R8UI;
            case BitDepth::BITS_16:
                return GL_R16UI;
            case BitDepth::BITS_32:
                return GL_R32UI;
            }
            break;
        case Fmt::FLOAT:
            switch (type.bit_depth) {
            case BitDepth::BITS_8:
                MG_ASSERT(false && "Unsupported combination");
                break;
            case BitDepth::BITS_16:
                return GL_R16F;
            case BitDepth::BITS_32:
                return GL_R32F;
            }
            break;
        }
        break;
    case Channels::RG:
        switch (type.fmt) {
        case Fmt::UNSIGNED_NORMALISED:
            switch (type.bit_depth) {
            case BitDepth::BITS_8:
                return GL_RG8;
            case BitDepth::BITS_16:
                return GL_RG16;
            case BitDepth::BITS_32:
                MG_ASSERT(false && "Unsupported combination");
                break;
            }
            break;
        case Fmt::SIGNED:
            switch (type.bit_depth) {
            case BitDepth::BITS_8:
                return GL_RG8I;
            case BitDepth::BITS_16:
                return GL_RG16I;
            case BitDepth::BITS_32:
                return GL_RG32I;
            }
            break;
        case Fmt::UNSIGNED:
            switch (type.bit_depth) {
            case BitDepth::BITS_8:
                return GL_RG8UI;
            case BitDepth::BITS_16:
                return GL_RG16UI;
            case BitDepth::BITS_32:
                return GL_RG32UI;
            }
            break;
        case Fmt::FLOAT:
            switch (type.bit_depth) {
            case BitDepth::BITS_8:
                MG_ASSERT(false && "Unsupported combination");
                break;
            case BitDepth::BITS_16:
                return GL_RG16F;
            case BitDepth::BITS_32:
                return GL_RG32F;
            }
            break;
        }
        break;
    case Channels::RGB:
        MG_ASSERT(type.bit_depth == BitDepth::BITS_32);
        switch (type.fmt) {
        case Fmt::UNSIGNED_NORMALISED:
            MG_ASSERT(false && "Unsupported combination");
            break;
        case Fmt::SIGNED:
            return GL_RGB32I;
        case Fmt::UNSIGNED:
            return GL_RGB32UI;
        case Fmt::FLOAT:
            return GL_RGB32F;
        }
        break;
    case Channels::RGBA:
        switch (type.fmt) {
        case Fmt::UNSIGNED_NORMALISED:
            switch (type.bit_depth) {
            case BitDepth::BITS_8:
                return GL_RGBA8;
            case BitDepth::BITS_16:
                return GL_RGBA16;
            case BitDepth::BITS_32:
                MG_ASSERT(false && "Unsupported combination");
                break;
            }
            break;
        case Fmt::SIGNED:
            switch (type.bit_depth) {
            case BitDepth::BITS_8:
                return GL_RGBA8I;
            case BitDepth::BITS_16:
                return GL_RGBA16I;
            case BitDepth::BITS_32:
                return GL_RGBA32I;
            }
            break;
        case Fmt::UNSIGNED:
            switch (type.bit_depth) {
            case BitDepth::BITS_8:
                return GL_RGBA8UI;
            case BitDepth::BITS_16:
                return GL_RGBA16UI;
            case BitDepth::BITS_32:
                return GL_RGBA32UI;
            }
            break;
        case Fmt::FLOAT:
            switch (type.bit_depth) {
            case BitDepth::BITS_8:
                MG_ASSERT(false && "Unsupported combination");
                break;
            case BitDepth::BITS_16:
                return GL_RGBA16F;
            case BitDepth::BITS_32:
                return GL_RGBA32F;
            }
            break;
        }
        break;
    }

    log.error("Unexpected BufferTexture::Type.");
    throw RuntimeError();
}

BufferTexture::BufferTexture(Type type, size_t buffer_size) : m_buffer_size(buffer_size)
{
    MG_GFX_DEBUG_GROUP("BufferTexture::BufferTexture");

    GLuint buf_id = 0;
    GLuint tex_id = 0;

    // Create data buffer and allocate storage
    glGenBuffers(1, &buf_id);
    glBindBuffer(GL_TEXTURE_BUFFER, buf_id);
    glBufferData(GL_TEXTURE_BUFFER, narrow<GLintptr>(buffer_size), nullptr, GL_STREAM_DRAW);

    // Create texture object
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_BUFFER, tex_id);

    // Associate data buffer to texture object
    glTexBuffer(GL_TEXTURE_BUFFER, buffer_texture_type_to_gl_enums(type), buf_id);

    m_tex_id.set(tex_id);
    m_buf_id.set(buf_id);

    MG_CHECK_GL_ERROR();
}

BufferTexture::~BufferTexture()
{
    MG_GFX_DEBUG_GROUP("BufferTexture::~BufferTexture");

    const auto buf_id = narrow<GLuint>(m_buf_id.get());
    const auto tex_id = narrow<GLuint>(m_tex_id.get());
    glDeleteTextures(1, &tex_id);
    glDeleteBuffers(1, &buf_id);
}

void BufferTexture::set_data(span<const std::byte> data) noexcept
{
    MG_GFX_DEBUG_GROUP("BufferTexture::set_data");

    // Update data buffer contents
    const auto buf_id = narrow<GLuint>(m_buf_id.get());
    glBindBuffer(GL_TEXTURE_BUFFER, buf_id);
    glBufferSubData(GL_TEXTURE_BUFFER, 0, narrow<GLsizeiptr>(data.size_bytes()), data.data());
}

} // namespace Mg::gfx
