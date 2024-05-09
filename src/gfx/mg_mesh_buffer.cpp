//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_mesh_buffer.h"

#include "mg/resources/mg_mesh_resource.h"
#include "mg_mesh_pool_impl.h"

namespace Mg::gfx {

//--------------------------------------------------------------------------------------------------
// Implementation
//--------------------------------------------------------------------------------------------------

class MeshBuffer::Impl {
public:
    Impl(MeshPoolImpl& pool_impl,
         mesh_data::VertexBufferSize vertex_buffer_size,
         mesh_data::IndexBufferSize index_buffer_size,
         mesh_data::InfluencesBufferSize influences_buffer_size)
        : m_pool_impl(&pool_impl)
        , m_vertex_buffer_size(static_cast<size_t>(vertex_buffer_size))
        , m_index_buffer_size(static_cast<size_t>(index_buffer_size))
        , m_influences_buffer_size(static_cast<size_t>(influences_buffer_size))
        , m_vertex_buffer(make_vertex_buffer(pool_impl, m_vertex_buffer_size))
        , m_index_buffer(make_index_buffer(pool_impl, m_index_buffer_size))
        , m_influences_buffer(m_influences_buffer_size > 0
                                  ? make_vertex_buffer(pool_impl, m_influences_buffer_size)
                                  : nullptr)
    {
        MG_ASSERT(m_vertex_buffer_size > 0);
        MG_ASSERT(m_index_buffer_size > 0);
    }

    MeshBuffer::CreateReturn create_in_buffer(const mesh_data::MeshDataView& mesh_data,
                                              Identifier name)
    {
        MG_GFX_DEBUG_GROUP("MeshBuffer::Impl::create");

        if (mesh_data.vertices.size_bytes() + m_vertex_buffer_offset > m_vertex_buffer_size) {
            return { nullptr, MeshBuffer::ReturnCode::Vertex_buffer_full };
        }

        if (mesh_data.indices.size_bytes() + m_index_buffer_offset > m_index_buffer_size) {
            return { nullptr, MeshBuffer::ReturnCode::Index_buffer_full };
        }

        const bool influences_buffer_is_full =
            mesh_data.animation_data && (mesh_data.animation_data->influences.size_bytes() +
                                         m_influences_buffer_offset) > m_influences_buffer_size;
        if (influences_buffer_is_full) {
            return { nullptr, MeshBuffer::ReturnCode::Influences_buffer_full };
        }

        MakeMeshParams params = mesh_params_from_mesh_data(*m_pool_impl, mesh_data);
        const auto mesh_handle = make_mesh(*m_pool_impl, name, params);

        m_vertex_buffer_offset += mesh_data.vertices.size_bytes();
        m_index_buffer_offset += mesh_data.indices.size_bytes();

        if (mesh_data.animation_data) {
            m_influences_buffer_offset += mesh_data.animation_data->influences.size_bytes();
        }

        return { mesh_handle, MeshBuffer::ReturnCode::Success };
    }

private:
    MeshPoolImpl* m_pool_impl = nullptr;

    // Data offsets; where to put next mesh's data.
    size_t m_vertex_buffer_offset = 0;     // Current offset into vertex buffer
    size_t m_index_buffer_offset = 0;      // Current offset into index buffer
    size_t m_influences_buffer_offset = 0; // Current offset into influences buffer

    size_t m_vertex_buffer_size = 0;
    size_t m_index_buffer_size = 0;
    size_t m_influences_buffer_size = 0;

    SharedBuffer* m_vertex_buffer = nullptr;
    SharedBuffer* m_index_buffer = nullptr;
    SharedBuffer* m_influences_buffer = nullptr;
};

//--------------------------------------------------------------------------------------------------
// Public interface
//--------------------------------------------------------------------------------------------------

MeshBuffer::MeshBuffer(MeshPoolImpl& mesh_pool,
                       mesh_data::VertexBufferSize vertex_buffer_size,
                       mesh_data::IndexBufferSize index_buffer_size,
                       mesh_data::InfluencesBufferSize influences_buffer_size)
    : m_impl(mesh_pool, vertex_buffer_size, index_buffer_size, influences_buffer_size)
{}

MeshBuffer::CreateReturn MeshBuffer::create_in_buffer(const MeshResource& resource)
{
    return m_impl->create_in_buffer(resource.data_view(), resource.resource_id());
}

MeshBuffer::CreateReturn MeshBuffer::create_in_buffer(const mesh_data::MeshDataView& mesh_data,
                                                      Identifier name)
{
    return m_impl->create_in_buffer(mesh_data, name);
}

} // namespace Mg::gfx
