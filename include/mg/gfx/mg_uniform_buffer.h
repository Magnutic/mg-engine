//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_uniform_buffer.h
 * Buffer for shader inputs.
 */

#pragma once

#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"
#include "mg/gfx/mg_gfx_object_handles.h"

#include <cstddef>
#include <cstdint>

namespace Mg::gfx {

enum class UniformBufferSlot : uint32_t;

/** Uniform buffer objects are groups of shader uniforms -- 'constant' shader input data.
 * @remark Constant from the shader's perspective, i.e. immutable and not varying per-vertex or
 * per-fragment.
 */
class UniformBuffer {
public:
    /** Create a new buffer of the given size (in bytes), and -- optionally -- initial data. */
    explicit UniformBuffer(size_t size, void* data = nullptr);
    ~UniformBuffer();

    MG_MAKE_NON_COPYABLE(UniformBuffer);
    MG_MAKE_DEFAULT_MOVABLE(UniformBuffer);

    /** Replace the buffer's data. */
    void set_data(span<const std::byte> data, size_t dest_offset = 0);

    /** Get the buffer's size in bytes. */
    size_t size() const noexcept { return m_size; }

    UniformBufferHandle handle() const noexcept { return m_handle; }

    /** Get the maximum size for a UBO on the present system. */
    static size_t max_size();

private:
    UniformBufferHandle m_handle;
    size_t m_size = 0;
};

} // namespace Mg::gfx
