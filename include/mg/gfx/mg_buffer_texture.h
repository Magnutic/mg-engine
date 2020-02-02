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

/** @file mg_buffer_texture.h
 * Buffer texture -- transfer arbitrary data buffer to GPU as a texture.
 */

#pragma once

#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_opaque_handle.h"

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

    OpaqueHandle::Value gfx_api_handle() const noexcept { return m_tex_id.value; }

private:
    OpaqueHandle m_tex_id;
    OpaqueHandle m_buf_id;
    size_t m_buffer_size = 0;
};

} // namespace Mg::gfx
