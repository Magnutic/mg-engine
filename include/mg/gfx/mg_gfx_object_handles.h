//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_gfx_object_handles.h.h
 * Handles for referring to graphics-API objects in a type-safe and API-agnostic way.
 */

#pragma once

#include <cstdint>
#include <utility>

namespace Mg::gfx {

enum class GfxObjectType {
    vertex_array,
    buffer,
    texture,
    uniform_buffer,
    vertex_shader,
    geometry_shader,
    fragment_shader,
    pipeline
};

using GfxObjectHandleValue = uint64_t;

/** Type-safe wrapper for 64-bit handles. This is used for wrapping handles to graphics API objects.
 */
template<GfxObjectType type_> class GfxObjectHandle {
public:
    static constexpr GfxObjectType type = type_;
    using Value = GfxObjectHandleValue;

    GfxObjectHandle() = default;

    explicit GfxObjectHandle(GfxObjectHandleValue id) noexcept : m_value{ id } {}

    GfxObjectHandle(const GfxObjectHandle&) = default;
    GfxObjectHandle(GfxObjectHandle&& rhs) : m_value(rhs.m_value) { rhs.m_value = 0; }
    GfxObjectHandle& operator=(const GfxObjectHandle&) noexcept = default;
    GfxObjectHandle& operator=(GfxObjectHandle&& rhs) noexcept
    {
        GfxObjectHandle temp(std::move(rhs));
        swap(temp);
        return *this;
    }

    void swap(GfxObjectHandle& other) noexcept { std::swap(m_value, other.m_value); }

    Value get() const { return m_value; }
    void set(Value value) { m_value = value; }

    friend bool operator==(const GfxObjectHandle& lhs, const GfxObjectHandle& rhs) noexcept
    {
        return lhs.m_value == rhs.m_value;
    }

    friend bool operator!=(const GfxObjectHandle& lhs, const GfxObjectHandle& rhs) noexcept
    {
        return lhs.m_value != rhs.m_value;
    }

private:
    Value m_value = 0;
};

using VertexArrayHandle = GfxObjectHandle<GfxObjectType::vertex_array>;

using BufferHandle = GfxObjectHandle<GfxObjectType::buffer>;

using TextureHandle = GfxObjectHandle<GfxObjectType::texture>;

using UniformBufferHandle = GfxObjectHandle<GfxObjectType::uniform_buffer>;

using VertexShaderHandle = GfxObjectHandle<GfxObjectType::vertex_shader>;

using GeometryShaderHandle = GfxObjectHandle<GfxObjectType::geometry_shader>;

using FragmentShaderHandle = GfxObjectHandle<GfxObjectType::fragment_shader>;

using PipelineHandle = GfxObjectHandle<GfxObjectType::pipeline>;

} // namespace Mg::gfx
