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

#include "mg/gfx/mg_mesh_repository.h"

#include "mg/containers/mg_pooling_vector.h"
#include "mg/core/mg_log.h"
#include "mg/resources/mg_mesh_resource.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_stl_helpers.h"

#include "../mg_mesh_info.h"
#include "mg_glad.h"

#include <fmt/core.h>

namespace Mg::gfx {

namespace {

//--------------------------------------------------------------------------------------------------
// Internal types
//--------------------------------------------------------------------------------------------------

// Internal storage type
class MeshNode {
public:
    internal::MeshInfo mesh_info;

    // Index of associated buffer objects.
    uint32_t vertex_buffer_index{};
    uint32_t index_buffer_index{};

    MeshNode() = default;

    MG_MAKE_NON_MOVABLE(MeshNode);
    MG_MAKE_NON_COPYABLE(MeshNode);

    // Destruction of MeshNode should destroy assocated VAO.
    ~MeshNode() { clear(); }

    void clear()
    {
        const auto vao_id = static_cast<uint32_t>(mesh_info.gfx_api_mesh_object_id);
        if (vao_id == 0) { return; }

        const auto* meshname [[maybe_unused]] = mesh_info.mesh_id.c_str();

        MG_LOG_DEBUG(fmt::format("Deleting VAO {} (Mesh '{}')", vao_id, meshname));
        glDeleteVertexArrays(1, &vao_id);
    }
};

/** Index of vertex buffer object. N.B.: not OpenGL object id, but rather index into local data
 * structure.
 */
enum class VboIndex : uint32_t {};

/** Index of index buffer object. N.B.: not OpenGL object id, but rather index into local data
 * structure.
 */
enum class IboIndex : uint32_t {};

/** RAII-owns an OpenGL buffer. */
struct BufferObject {
    enum class Type { Vertex, Index };

    uint32_t gfx_api_id{ 0 };
    int32_t  num_users{ 0 }; // Number of meshes whose data resides in this buffer
    Type     type{};

    BufferObject() = default;
    ~BufferObject()
    {
        MG_LOG_DEBUG(fmt::format("Deleting buffer object {}", gfx_api_id));
        glDeleteBuffers(1, &gfx_api_id);
    }

    MG_MAKE_NON_COPYABLE(BufferObject);
    MG_MAKE_NON_MOVABLE(BufferObject);
};

} // namespace

//--------------------------------------------------------------------------------------------------

class MeshRepositoryImpl {
    // Size of the pools allocated for the internal data structures, in number of elements.
    // This is a fairly arbitrary choice:  larger pools may make allocations more rare and provide
    // better data locality but could also waste space if the pool is never filled.
    static constexpr size_t k_mesh_data_pool_size     = 512;
    static constexpr size_t k_buffer_object_pool_size = 512;

public:
    MeshRepositoryImpl() = default;

    MeshHandle create(const MeshResource& mesh_res);

    void update(const MeshResource& mesh_res);

    void destroy(MeshHandle handle);

private:
    friend class MeshBufferImpl; // needs access to _make_xxxx_buffer below

    VboIndex _make_vertex_buffer(size_t size);
    IboIndex _make_index_buffer(size_t size);

    void _make_mesh_in_node(MeshNode&           node,
                            const MeshResource& mesh_res,
                            VboIndex            vbo_index,
                            size_t              vbo_data_offset,
                            IboIndex            ibo_index,
                            size_t              ibo_data_offset);

    MeshHandle _make_mesh(const MeshResource& mesh_res,
                          VboIndex            vbo_index,
                          size_t              vbo_data_offset,
                          IboIndex            ibo_index,
                          size_t              ibo_data_offset)
    {
        auto [index, p_mesh_node]     = m_mesh_data.construct();
        internal::MeshInfo& mesh_info = p_mesh_node->mesh_info;
        mesh_info.self_index          = index;
        _make_mesh_in_node(
            *p_mesh_node, mesh_res, vbo_index, vbo_data_offset, ibo_index, ibo_data_offset);
        node_map.push_back({ mesh_res.resource_id(), p_mesh_node });
        return internal::make_mesh_handle(&mesh_info);
    }

    void _clear_mesh_node(MeshNode& node);

    // Internal mesh meta-data storage
    PoolingVector<BufferObject> m_buffer_objects{ k_buffer_object_pool_size };
    PoolingVector<MeshNode>     m_mesh_data{ k_mesh_data_pool_size };

    // Used for looking up a mesh node by identifier.
    // TODO: lookup can be optimised by storing in sorted order. Use std::lower_bound.
    std::vector<std::pair<Identifier, MeshNode*>> node_map;
};

MeshHandle MeshRepositoryImpl::create(const MeshResource& mesh_res)
{
    const VboIndex vbo_index = _make_vertex_buffer(mesh_res.vertices().size_bytes());
    const IboIndex ibo_index = _make_index_buffer(mesh_res.indices().size_bytes());
    return _make_mesh(mesh_res, vbo_index, 0, ibo_index, 0);
}

void MeshRepositoryImpl::update(const MeshResource& mesh_res)
{
    const Identifier resource_id = mesh_res.resource_id();
    const auto       it = find_if(node_map, [&](auto& pair) { return pair.first == resource_id; });

    // If not found, then we do not have a mesh using the updated resource, so ignore.
    if (it == node_map.end()) { return; }

    MeshNode& node = *it->second;

    const VboIndex vbo_index = _make_vertex_buffer(mesh_res.vertices().size_bytes());
    const IboIndex ibo_index = _make_index_buffer(mesh_res.indices().size_bytes());

    // Clearing the existing node and creating the new mesh within ensures MeshHandles remain valid.
    node.clear();
    _make_mesh_in_node(node, mesh_res, vbo_index, 0, ibo_index, 0);

    g_log.write_verbose(
        fmt::format("MeshRepository::update(): Updated {}", resource_id.str_view()));
}

void MeshRepositoryImpl::destroy(MeshHandle handle)
{
    auto&      mesh_info = internal::mesh_info(handle);
    const auto index     = mesh_info.self_index;
    MeshNode&  node      = m_mesh_data[index];
    _clear_mesh_node(node);
    m_mesh_data.destroy(index);

    // Erase from resource_id -> node map.
    const auto it = find_if(node_map, [&](auto& pair) { return pair.second == &node; });
    if (it != node_map.end()) { node_map.erase(it); }
}

VboIndex MeshRepositoryImpl::_make_vertex_buffer(size_t size)
{
    auto [vbo_index, p_vbo] = m_buffer_objects.construct();
    p_vbo->type             = BufferObject::Type::Vertex;

    glGenBuffers(1, &p_vbo->gfx_api_id);
    glBindBuffer(GL_ARRAY_BUFFER, p_vbo->gfx_api_id);
    glBufferData(GL_ARRAY_BUFFER, narrow<GLsizeiptr>(size), nullptr, GL_STATIC_DRAW);

    return VboIndex{ vbo_index };
}

IboIndex MeshRepositoryImpl::_make_index_buffer(size_t size)
{
    auto [ibo_index, p_ibo] = m_buffer_objects.construct();
    p_ibo->type             = BufferObject::Type::Index;

    glGenBuffers(1, &p_ibo->gfx_api_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, p_ibo->gfx_api_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, narrow<GLsizeiptr>(size), nullptr, GL_STATIC_DRAW);

    return IboIndex{ ibo_index };
}

// Create mesh from MeshResouce, storing data in the given vertex and index buffers
void MeshRepositoryImpl::_make_mesh_in_node(MeshNode&           node,
                                            const MeshResource& mesh_res,
                                            VboIndex            vbo_index,
                                            size_t              vbo_data_offset,
                                            IboIndex            ibo_index,
                                            size_t              ibo_data_offset)
{
    node.mesh_info.mesh_id = mesh_res.resource_id();
    node.mesh_info.centre  = mesh_res.centre();
    node.mesh_info.radius  = mesh_res.radius();

    for (auto&& sm : mesh_res.sub_meshes()) {
        node.mesh_info.submeshes.push_back({ sm.begin, sm.amount });
    }


    uint32_t vao_id = 0;
    glGenVertexArrays(1, &vao_id);
    node.mesh_info.gfx_api_mesh_object_id = static_cast<OpaqueHandle::Value>(vao_id);

    glBindVertexArray(vao_id);

    {
        BufferObject& vbo        = m_buffer_objects[static_cast<uint32_t>(vbo_index)];
        node.vertex_buffer_index = static_cast<uint32_t>(vbo_index);
        ++vbo.num_users;

        const auto vbo_data = mesh_res.vertices().as_bytes();
        glBindBuffer(GL_ARRAY_BUFFER, vbo.gfx_api_id);
        glBufferSubData(GL_ARRAY_BUFFER,
                        narrow<GLintptr>(vbo_data_offset),
                        narrow<GLsizeiptr>(vbo_data.size()),
                        vbo_data.data());
    }

    {
        BufferObject& ibo       = m_buffer_objects[static_cast<uint32_t>(ibo_index)];
        node.index_buffer_index = static_cast<uint32_t>(ibo_index);
        ++ibo.num_users;

        const auto ibo_data = mesh_res.indices().as_bytes();
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo.gfx_api_id);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                        narrow<GLintptr>(ibo_data_offset),
                        narrow<GLsizeiptr>(ibo_data.size()),
                        ibo_data.data());
    }

    uint32_t  attrib_i = 0;
    uintptr_t offset   = 0;
    uint32_t  stride   = 0;

    for (const VertexAttribute& a : g_attrib_array) { stride += a.size; }

    for (const VertexAttribute& a : g_attrib_array) {
        const GLvoid* gl_offset{};
        std::memcpy(&gl_offset, &offset, sizeof(offset));
        glVertexAttribPointer(attrib_i,
                              narrow<GLint>(a.num),
                              static_cast<uint32_t>(a.type),
                              static_cast<GLboolean>(a.normalised),
                              narrow<int32_t>(stride),
                              gl_offset); // NOLINT

        glEnableVertexAttribArray(attrib_i);

        offset += a.size;
        ++attrib_i;
    }

    glBindVertexArray(0);
}

void MeshRepositoryImpl::_clear_mesh_node(MeshNode& node)
{
    node.clear();

    BufferObject& vbo = m_buffer_objects[node.vertex_buffer_index];
    if (--vbo.num_users == 0) { m_buffer_objects.destroy(node.vertex_buffer_index); }

    BufferObject& ibo = m_buffer_objects[node.index_buffer_index];
    if (--ibo.num_users == 0) { m_buffer_objects.destroy(node.index_buffer_index); }
}

class MeshBufferImpl {
public:
    MeshBufferImpl(MeshRepositoryImpl& mesh_repository,
                   VertexBufferSize    vertex_buffer_size,
                   IndexBufferSize     index_buffer_size)
        : m_mesh_repository(&mesh_repository)
        , m_vbo_size(static_cast<size_t>(vertex_buffer_size))
        , m_ibo_size(static_cast<size_t>(index_buffer_size))
        , m_vbo_id(mesh_repository._make_vertex_buffer(m_vbo_size))
        , m_ibo_id(mesh_repository._make_index_buffer(m_ibo_size))
    {}

    MeshBuffer::CreateReturn create(const MeshResource& resource)
    {
        if (resource.vertices().size_bytes() + m_vbo_offset > m_vbo_size) {
            return { nullopt, MeshBuffer::ReturnCode::Vertex_buffer_full };
        }

        if (resource.indices().size_bytes() + m_ibo_offset > m_ibo_size) {
            return { nullopt, MeshBuffer::ReturnCode::Index_buffer_full };
        }

        MeshHandle mesh = m_mesh_repository->_make_mesh(resource,
                                                        m_vbo_id,
                                                        m_vbo_offset,
                                                        m_ibo_id,
                                                        m_ibo_offset);

        m_vbo_offset += resource.vertices().size_bytes();
        m_ibo_offset += resource.indices().size_bytes();

        return { mesh, MeshBuffer::ReturnCode::Success };
    }

private:
    MeshRepositoryImpl* m_mesh_repository = nullptr;

    size_t m_vbo_offset{ 0 }; // Current offset into vertex buffer -- where to put next mesh's data
    size_t m_ibo_offset{ 0 }; // Current offset into index buffer -- where to put next mesh's data

    size_t m_vbo_size{ 0 };
    size_t m_ibo_size{ 0 };

    VboIndex m_vbo_id{ 0 };
    IboIndex m_ibo_id{ 0 };
};

//--------------------------------------------------------------------------------------------------

MeshBuffer::~MeshBuffer() = default;

MeshBuffer::CreateReturn MeshBuffer::create(const MeshResource& resource)
{
    return impl().create(resource);
}

//--------------------------------------------------------------------------------------------------

MeshRepository::MeshRepository()  = default;
MeshRepository::~MeshRepository() = default;

MeshHandle MeshRepository::create(const MeshResource& mesh_res)
{
    return impl().create(mesh_res);
}

void MeshRepository::destroy(MeshHandle handle)
{
    impl().destroy(handle);
}

void MeshRepository::update(const MeshResource& mesh_res)
{
    impl().update(mesh_res);
}

MeshBuffer MeshRepository::new_mesh_buffer(VertexBufferSize vertex_buffer_size,
                                           IndexBufferSize  index_buffer_size)
{
    return MeshBuffer{ impl(), vertex_buffer_size, index_buffer_size };
}

} // namespace Mg::gfx
