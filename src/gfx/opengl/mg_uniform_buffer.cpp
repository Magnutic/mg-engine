//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_uniform_buffer.h"

#include "mg_glad.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/utils/mg_assert.h"

#include <fmt/core.h>

#include <algorithm> // std::min
#include <cstring>   // memcpy

namespace Mg::gfx {

UniformBuffer::UniformBuffer(size_t size, void* data) : m_size(size)
{
    MG_GFX_DEBUG_GROUP("Create UniformBuffer")

    if (size > max_size()) {
        log.error("UniformBuffer of size {} exceeds system maximum of {}.", size, max_size());
        throw RuntimeError{};
    }

    GLuint ubo_id = 0;
    glGenBuffers(1, &ubo_id);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_id);
    glBufferData(GL_UNIFORM_BUFFER, GLsizeiptr(m_size), data, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    m_handle.set(ubo_id);
}

UniformBuffer::~UniformBuffer()
{
    MG_GFX_DEBUG_GROUP("UniformBuffer::~UniformBuffer")

    const auto ubo_id = narrow<GLuint>(m_handle.get());
    glDeleteBuffers(1, &ubo_id);
}

void UniformBuffer::set_data(span<const std::byte> data, size_t dest_offset)
{
    MG_GFX_DEBUG_GROUP("UniformBuffer::set_data")

    const auto ubo_id = narrow<GLuint>(m_handle.get());

    if (ubo_id == 0) {
        log.warning("Attempting to write to uninitialised UBO");
        return;
    }

    MG_ASSERT(dest_offset < m_size);
    const size_t available_size = m_size - dest_offset;

    if (available_size < data.size_bytes()) {
        log.error(
            "UniformBuffer at {}: set_data(): could not fit data in buffer (data size {}, "
            "buffer size {}, writing starting at offset {})",
            static_cast<void*>(this),
            data.size_bytes(),
            m_size,
            dest_offset);

        throw RuntimeError{};
    }

    glBindBuffer(GL_UNIFORM_BUFFER, ubo_id);
    GLvoid* p = glMapBufferRange(GL_UNIFORM_BUFFER,
                                 narrow<GLintptr>(dest_offset),
                                 narrow<GLsizeiptr>(data.size_bytes()),
                                 GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
    MG_ASSERT(p != nullptr);

    std::memcpy(p, data.data(), data.size_bytes());
    glUnmapBuffer(GL_UNIFORM_BUFFER);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

size_t UniformBuffer::max_size()
{
    static GLint64 result = 0;

    if (result == 0) {
        glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &result);
        log.verbose("GL_MAX_UNIFORM_BLOCK_SIZE: {}", result);
    }

    return narrow<size_t>(result);
}

} // namespace Mg::gfx
