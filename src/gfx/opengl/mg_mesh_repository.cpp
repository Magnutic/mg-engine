//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_mesh_repository.h"

#include "mg/containers/mg_flat_map.h"
#include "mg/core/mg_log.h"
#include "mg/resources/mg_mesh_resource.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_stl_helpers.h"

#include "../mg_mesh_info.h"
#include "mg_glad.h"

#include <plf_colony.h>

#include <glm/glm.hpp>

#include <fmt/core.h>

#include <numeric>

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
        if (vao_id == 0) {
            return;
        }

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
    int32_t num_users{ 0 }; // Number of meshes whose data resides in this buffer
    Type type{};

    BufferObject() = default;
    ~BufferObject()
    {
        MG_LOG_DEBUG(fmt::format("Deleting buffer object {}", gfx_api_id));
        glDeleteBuffers(1, &gfx_api_id);
    }

    MG_MAKE_NON_COPYABLE(BufferObject);
    MG_MAKE_NON_MOVABLE(BufferObject);
};

MeshDataView::BoundingInfo calculate_mesh_bounding_info(span<const Vertex> vertices,
                                                        span<const uint_vertex_index> indices)
{
    MG_ASSERT(!vertices.empty() && !indices.empty());
    const auto num_indices = static_cast<float>(indices.size());

    auto add_position = [&](glm::vec3 vec, uint_vertex_index vert_idx) {
        return vec + vertices[vert_idx].position;
    };
    const glm::vec3 centre = std::accumulate(indices.begin(),
                                             indices.end(),
                                             glm::vec3{ 0.0f },
                                             add_position) /
                             num_indices;

    auto cmp_vertex = [](const Vertex& lhs, const Vertex& rhs) {
        return glm::all(glm::lessThan(lhs.position, rhs.position));
    };
    const auto [min_it, max_it] = std::minmax_element(vertices.begin(), vertices.end(), cmp_vertex);

    const float radius = std::max(glm::distance(centre, min_it->position),
                                  glm::distance(centre, max_it->position));

    return { centre, radius };
}

} // namespace

//--------------------------------------------------------------------------------------------------

class MeshRepositoryImpl {
public:
    MeshRepositoryImpl() = default;

    MeshHandle create(Identifier mesh_id, const MeshDataView& data);

    bool update(Identifier mesh_id, const MeshDataView& new_data);

    void destroy(MeshHandle handle);

    MeshNode* get(Identifier mesh_id) const
    {
        const auto it = node_map.find(mesh_id);
        if (it == node_map.end()) {
            return nullptr;
        }
        return it->second;
    }

private:
    friend class MeshBufferImpl; // needs access to _make_xxxx_buffer below

    VboIndex _make_vertex_buffer(size_t size);
    IboIndex _make_index_buffer(size_t size);

    struct MakeMeshParams {
        // Where to put the data
        VboIndex vbo_index;
        size_t vbo_data_offset;
        IboIndex ibo_index;
        size_t ibo_data_offset;

        // Data itself
        MeshDataView mesh_data;
        glm::vec3 centre;
        float radius;
    };

    MakeMeshParams _mesh_params_from_mesh_data(const MeshDataView& data)
    {
        const auto [centre, radius] = data.bounding_info.value_or(
            calculate_mesh_bounding_info(data.vertices, data.indices));

        MakeMeshParams params = {};
        params.vbo_index = _make_vertex_buffer(data.vertices.size_bytes());
        params.ibo_index = _make_index_buffer(data.indices.size_bytes());
        params.vbo_data_offset = 0;
        params.ibo_data_offset = 0;
        params.centre = centre;
        params.radius = radius;
        params.mesh_data = data;

        return params;
    }

    void _make_mesh_in_node(MeshNode& node, Identifier mesh_id, const MakeMeshParams& params);

    MeshHandle _make_mesh(Identifier mesh_id, const MakeMeshParams& params)
    {
        const auto it = m_mesh_data.emplace();
        const auto index = narrow<uint32_t>(m_mesh_data.get_index_from_iterator(it));

        internal::MeshInfo& mesh_info = it->mesh_info;
        mesh_info.self_index = index;
        _make_mesh_in_node(*it, mesh_id, params);
        node_map.insert({ mesh_id, &(*it) });
        return internal::make_mesh_handle(&mesh_info);
    }

    void _clear_mesh_node(MeshNode& node);

    // Internal mesh meta-data storage
    plf::colony<BufferObject> m_buffer_objects;
    plf::colony<MeshNode> m_mesh_data;

    // Used for looking up a mesh node by identifier.
    FlatMap<Identifier, MeshNode*> node_map;
};

MeshHandle MeshRepositoryImpl::create(Identifier mesh_id, const MeshDataView& mesh_data)
{
    // Check precondition
    const bool has_vertices = !mesh_data.vertices.empty();
    const bool has_indices = !mesh_data.indices.empty();

    if (!has_vertices || !has_indices) {
        const std::string problem = !has_vertices ? "no vertex data" : "no index data";
        const std::string msg = fmt::format("MeshRepository: cannot create mesh '{}': {}.",
                                            mesh_id.str_view(),
                                            problem);
        throw std::runtime_error(msg);
    }

    const MakeMeshParams params = _mesh_params_from_mesh_data(mesh_data);
    return _make_mesh(mesh_id, params);
}

bool MeshRepositoryImpl::update(Identifier mesh_id, const MeshDataView& data)
{
    MeshNode* node = get(mesh_id);

    // If not found, then we do not have a mesh using the updated resource, so ignore.
    if (!node) {
        return false;
    }

    // Use the existing node to ensure MeshHandles remain valid.
    _make_mesh_in_node(*node, mesh_id, _mesh_params_from_mesh_data(data));
    g_log.write_verbose(fmt::format("MeshRepository::update(): Updated {}", mesh_id.str_view()));
    return true;
}

void MeshRepositoryImpl::destroy(MeshHandle handle)
{
    auto& mesh_info = internal::mesh_info(handle);
    const auto index = mesh_info.self_index;
    const Identifier mesh_id = mesh_info.mesh_id;

    const auto it = m_mesh_data.get_iterator_from_index(index);
    MeshNode& node = *it;
    _clear_mesh_node(node);
    m_mesh_data.erase(it);

    // Erase from resource_id -> node map.
    node_map.erase(mesh_id);
}

VboIndex MeshRepositoryImpl::_make_vertex_buffer(size_t size)
{
    const auto it = m_buffer_objects.emplace();
    it->type = BufferObject::Type::Vertex;

    glGenBuffers(1, &it->gfx_api_id);
    glBindBuffer(GL_ARRAY_BUFFER, it->gfx_api_id);
    glBufferData(GL_ARRAY_BUFFER, narrow<GLsizeiptr>(size), nullptr, GL_STATIC_DRAW);

    const auto index = narrow<uint32_t>(m_buffer_objects.get_index_from_iterator(it));

    return VboIndex{ index };
}

IboIndex MeshRepositoryImpl::_make_index_buffer(size_t size)
{
    const auto it = m_buffer_objects.emplace();
    it->type = BufferObject::Type::Index;

    glGenBuffers(1, &it->gfx_api_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, it->gfx_api_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, narrow<GLsizeiptr>(size), nullptr, GL_STATIC_DRAW);

    const auto index = narrow<uint32_t>(m_buffer_objects.get_index_from_iterator(it));

    return IboIndex{ index };
}

// Create mesh from MeshResouce, storing data in the given vertex and index buffers
void MeshRepositoryImpl::_make_mesh_in_node(MeshNode& node,
                                            Identifier mesh_id,
                                            const MakeMeshParams& params)
{
    node.clear();

    node.mesh_info.mesh_id = mesh_id;
    node.mesh_info.centre = params.centre;
    node.mesh_info.radius = params.radius;

    for (auto&& sm : params.mesh_data.sub_meshes) {
        node.mesh_info.submeshes.push_back({ sm.begin, sm.amount });
    }

    uint32_t vao_id = 0;
    glGenVertexArrays(1, &vao_id);
    node.mesh_info.gfx_api_mesh_object_id = static_cast<OpaqueHandle::Value>(vao_id);

    glBindVertexArray(vao_id);

    {
        const auto vbo_index = static_cast<uint32_t>(params.vbo_index);
        BufferObject& vbo = *m_buffer_objects.get_iterator_from_index(vbo_index);
        node.vertex_buffer_index = vbo_index;
        ++vbo.num_users;

        const auto vbo_data = params.mesh_data.vertices.as_bytes();
        glBindBuffer(GL_ARRAY_BUFFER, vbo.gfx_api_id);
        glBufferSubData(GL_ARRAY_BUFFER,
                        narrow<GLintptr>(params.vbo_data_offset),
                        narrow<GLsizeiptr>(vbo_data.size()),
                        vbo_data.data());
    }

    {
        const auto ibo_index = static_cast<uint32_t>(params.ibo_index);
        BufferObject& ibo = *m_buffer_objects.get_iterator_from_index(ibo_index);
        node.index_buffer_index = ibo_index;
        ++ibo.num_users;

        const auto ibo_data = params.mesh_data.indices.as_bytes();
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo.gfx_api_id);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                        narrow<GLintptr>(params.ibo_data_offset),
                        narrow<GLsizeiptr>(ibo_data.size()),
                        ibo_data.data());
    }

    uint32_t attrib_i = 0;
    uintptr_t offset = 0;
    uint32_t stride = 0;

    for (const VertexAttribute& a : g_attrib_array) {
        stride += a.size;
    }

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

    const auto vbo_it = m_buffer_objects.get_iterator_from_index(node.vertex_buffer_index);
    const auto ibo_it = m_buffer_objects.get_iterator_from_index(node.index_buffer_index);

    BufferObject& vbo = *vbo_it;
    if (--vbo.num_users == 0) {
        m_buffer_objects.erase(vbo_it);
    }

    BufferObject& ibo = *ibo_it;
    if (--ibo.num_users == 0) {
        m_buffer_objects.erase(ibo_it);
    }
}

class MeshBufferImpl {
public:
    MeshBufferImpl(MeshRepositoryImpl& mesh_repository,
                   VertexBufferSize vertex_buffer_size,
                   IndexBufferSize index_buffer_size)
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

        const MeshHandle mesh_handle = m_mesh_repository->_make_mesh(
            resource.resource_id(),
            m_mesh_repository->_mesh_params_from_mesh_data(resource.data_view()));

        m_vbo_offset += resource.vertices().size_bytes();
        m_ibo_offset += resource.indices().size_bytes();

        return { mesh_handle, MeshBuffer::ReturnCode::Success };
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

MeshRepository::MeshRepository() = default;
MeshRepository::~MeshRepository() = default;

MeshHandle MeshRepository::create(const MeshResource& mesh_res)
{
    return impl().create(mesh_res.resource_id(), mesh_res.data_view());
}

MeshHandle MeshRepository::create(const MeshDataView& mesh_data, Identifier mesh_id)
{
    return impl().create(mesh_id, mesh_data);
}

Opt<MeshHandle> MeshRepository::get(Identifier mesh_id) const
{
    const MeshNode* node = impl().get(mesh_id);
    if (!node) {
        return nullopt;
    }
    return internal::make_mesh_handle(&node->mesh_info);
}

void MeshRepository::destroy(MeshHandle handle)
{
    impl().destroy(handle);
}

bool MeshRepository::update(const MeshResource& mesh_res)
{
    return impl().update(mesh_res.resource_id(), mesh_res.data_view());
}

MeshBuffer MeshRepository::new_mesh_buffer(VertexBufferSize vertex_buffer_size,
                                           IndexBufferSize index_buffer_size)
{
    return MeshBuffer{ impl(), vertex_buffer_size, index_buffer_size };
}

} // namespace Mg::gfx
