//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_buffer_texture.h
 * Buffer texture -- transfer arbitrary data buffer to GPU as a texture.
 */

#pragma once

#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"

// TODO: automagic double-buffering?

namespace Mg::gfx {

class BufferTexture {
public:
    enum class Channels { R, RG, RGB, RGBA };
    enum class Format { UNSIGNED_NORMALISED, UNSIGNED, SIGNED, FLOAT };
    enum class BitDepth { BITS_8, BITS_16, BITS_32 };

    struct Type {
        Channels channels;
        Format fmt;
        BitDepth bit_depth;
    };

    BufferTexture(Type type, size_t buffer_size);

    BufferTexture(Type type, span<const std::byte> data) : BufferTexture(type, data.size())
    {
        set_data(data);
    }

    MG_MAKE_DEFAULT_MOVABLE(BufferTexture);
    MG_MAKE_NON_COPYABLE(BufferTexture);

    ~BufferTexture();

    void set_data(span<const std::byte> data) noexcept;

    size_t size() const noexcept { return m_buffer_size; }

    TextureHandle handle() const noexcept { return m_tex_id; }

private:
    TextureHandle m_tex_id;
    BufferHandle m_buf_id;
    size_t m_buffer_size = 0;
};

} // namespace Mg::gfx
