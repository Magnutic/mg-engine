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

#include "mg/gfx/mg_uniform_buffer.h"

#include "mg_glad.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/utils/mg_assert.h"

#include <fmt/core.h>

#include <algorithm> // std::min
#include <cstring>   // memcpy

namespace Mg::gfx {

UniformBuffer::UniformBuffer(size_t size, void* data) : m_size(size)
{
    GLuint ubo_id = 0;
    glGenBuffers(1, &ubo_id);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_id);
    glBufferData(GL_UNIFORM_BUFFER, GLsizeiptr(m_size), data, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // N.B. cannot directly use this as an argument to glGenBuffers, as it expects pointer to GLuint
    // (uint32_t) whereas this is OpaqueHandle::Value (an opaque typedef of uint64_t).
    m_internal_ubo_id = ubo_id;
}

UniformBuffer::~UniformBuffer()
{
    GLuint ubo_id = static_cast<uint32_t>(gfx_api_handle());
    glDeleteBuffers(1, &ubo_id);
}

void UniformBuffer::set_data(span<const std::byte> data, size_t dest_offset)
{
    GLuint ubo_id = static_cast<uint32_t>(gfx_api_handle());

    if (ubo_id == 0) {
        g_log.write_warning("Attempting to write to uninitialised UBO");
        return;
    }

    MG_ASSERT(dest_offset < m_size);
    const size_t available_size = m_size - dest_offset;

    if (available_size < data.size_bytes()) {
        g_log.write_error(fmt::format(
            "UniformBuffer at {}: set_data(): could not fit data in buffer (data size {}, "
            "buffer size {}, writing starting at offset {})",
            static_cast<void*>(this),
            data.size_bytes(),
            m_size,
            dest_offset));

        throw RuntimeError{};
    }

    glBindBuffer(GL_UNIFORM_BUFFER, ubo_id);
    GLvoid* p = glMapBufferRange(GL_UNIFORM_BUFFER,
                                 narrow<GLintptr>(dest_offset),
                                 narrow<GLsizeiptr>(data.size_bytes()),
                                 GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
    MG_ASSERT(p != nullptr);

    std::memcpy(p, &data[0], data.size_bytes());
    glUnmapBuffer(GL_UNIFORM_BUFFER);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

size_t UniformBuffer::max_size()
{
    static GLint64 result = 0;

    if (result == 0) {
        glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &result);
        g_log.write_verbose(fmt::format("GL_MAX_UNIFORM_BLOCK_SIZE: {}", result));
    }

    return narrow<size_t>(result);
}

} // namespace Mg::gfx
