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

#include "mg/gfx/mg_buffer_texture.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

namespace Mg::gfx {

uint32_t buffer_texture_type_to_gl_enums(BufferTexture::Type type)
{
    using Channels = BufferTexture::Channels;
    using Fmt      = BufferTexture::Format;
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

    g_log.write_error("Unexpected BufferTexture::Type.");
    throw RuntimeError();
}

BufferTexture::BufferTexture(Type type, size_t buffer_size) : m_buffer_size(buffer_size)
{
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

    // N.B. cannot directly use these as arguments to OpenGL functions, as they expect pointer to
    // GLuint (uint32_t) whereas these are OpaqueHandle::Value (an opaque typedef of uint64_t).
    m_tex_id = tex_id;
    m_buf_id = buf_id;

    MG_CHECK_GL_ERROR();
}

BufferTexture::~BufferTexture()
{
    const auto buf_id = static_cast<GLuint>(m_buf_id.value);
    const auto tex_id = static_cast<GLuint>(m_tex_id.value);
    glDeleteTextures(1, &tex_id);
    glDeleteBuffers(1, &buf_id);
}

void BufferTexture::set_data(span<const std::byte> data) noexcept
{
    // Update data buffer contents
    const auto buf_id = static_cast<GLuint>(m_buf_id.value);
    glBindBuffer(GL_TEXTURE_BUFFER, buf_id);
    glBufferSubData(GL_TEXTURE_BUFFER, 0, narrow<GLsizeiptr>(data.size_bytes()), data.data());
}

} // namespace Mg::gfx
