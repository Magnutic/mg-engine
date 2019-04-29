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

/** @file mg_uniform_buffer.h
 * Buffer for shader inputs.
 */

#pragma once

#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_opaque_handle.h"

#include <cstddef>
#include <cstdint>

namespace Mg::gfx {

enum class UniformBufferSlot : uint32_t;

/** Uniform buffer objects are groups of shader uniforms -- 'constant' shader input data.
 * @remark Constant from the shader's perspective, i.e. immutable and not varying per-vertex or
 * per-fragment.
 *
 * Uniform buffer objects are bound to global slots (see UniformBufferSlot), rather than being
 * bound to the currently active shader program. Thus, uniform buffer objects do not need to be
 * re-bound when the active shader program is switched.
 *
 * To make a ShaderProgram use a UniformBuffer as source data for one of its uniform blocks,
 * set the uniform block's binding to a given UniformBufferSlot:
 *
 * [OpenGL only]
 *     ShaderProgram::set_uniform_block_binding(block_name, uniform_buffer_slot)
 *
 * @remark As of writing -- 2018 -- only an OpenGL back-end is implemented, but in a Vulkan or
 * Direct3D back-end, it would instead be necessary to specify the uniform buffer slot directly in
 * the shader code instead.
 */
class UniformBuffer {
public:
    /** Create a new buffer of the given size (in bytes), and -- optionally -- initial data. */
    explicit UniformBuffer(size_t size, void* data = nullptr);
    ~UniformBuffer();

    MG_MAKE_NON_COPYABLE(UniformBuffer);
    MG_MAKE_DEFAULT_MOVABLE(UniformBuffer);

    /** Replace the buffer's data. */
    void set_data(span<const std::byte> data);

    /** Get the buffer's size in bytes. */
    size_t size() const { return m_size; }

    OpaqueHandle::Value gfx_api_handle() const { return m_internal_ubo_id.value; }

    /** Get the maximum size for a UBO on the present system. */
    static size_t max_size();

private:
    OpaqueHandle m_internal_ubo_id;
    size_t       m_size = 0;
};

} // namespace Mg::gfx
